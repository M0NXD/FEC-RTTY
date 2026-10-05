#include "fectty/portaudio_io.hpp"
#include "fectty/parse.hpp"

#ifdef FECTTY_HAS_PORTAUDIO

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <mutex>

namespace fectty {
namespace {

constexpr double kSampleRate = 48000.0;
// Match the GUI receiver's normal WinMM chunk size. Larger callbacks reduce
// callback overhead while retaining a sub-50 ms audio block at 48 kHz.
constexpr unsigned long kFramesPerBuffer = 2048;
constexpr size_t kMaxQueuedChunks = 1024;

std::mutex g_portaudio_lifetime_mutex;
size_t g_portaudio_users = 0;

bool acquire_portaudio() {
    std::lock_guard lock(g_portaudio_lifetime_mutex);
    if (g_portaudio_users == 0 && Pa_Initialize() != paNoError) return false;
    ++g_portaudio_users;
    return true;
}

void release_portaudio() {
    std::lock_guard lock(g_portaudio_lifetime_mutex);
    if (g_portaudio_users == 0) return;
    --g_portaudio_users;
    if (g_portaudio_users == 0) Pa_Terminate();
}

std::string pa_error(PaError error) {
    return error == paNoError ? std::string{} : std::string(Pa_GetErrorText(error));
}

bool parse_device_id(const std::string& id, int& device) {
    if (id.empty()) return true;
    constexpr std::string_view prefix = "portaudio:";
    if (!id.starts_with(prefix)) return false;
    int value = -1;
    if (!parse_number(std::string_view(id).substr(prefix.size()), value) || value < 0) return false;
    device = value;
    return true;
}

bool valid_device(int device) {
    return device != paNoDevice && Pa_GetDeviceInfo(device) != nullptr;
}

} // namespace

PortAudioIo::PortAudioIo() {
    initialized_ = acquire_portaudio();
    if (!initialized_) set_error("PortAudio initialization failed");
}

PortAudioIo::~PortAudioIo() {
    close();
    if (initialized_) release_portaudio();
}

void PortAudioIo::set_error(std::string message) {
    std::lock_guard lock(error_mutex_);
    error_ = std::move(message);
}

std::vector<AudioDeviceInfo> PortAudioIo::list_devices() {
    std::vector<AudioDeviceInfo> result;
    if (!acquire_portaudio()) return result;
    const auto count = Pa_GetDeviceCount();
    if (count >= 0) {
        for (int i = 0; i < count; ++i) {
            const auto* info = Pa_GetDeviceInfo(i);
            if (!info) continue;
            const auto* host = Pa_GetHostApiInfo(info->hostApi);
            const std::string name = std::string(info->name ? info->name : "PortAudio device")
                + " [" + (host && host->name ? host->name : "unknown host") + "]";
            result.push_back({"portaudio:" + std::to_string(i), name,
                              info->maxInputChannels > 0, info->maxOutputChannels > 0});
        }
    }
    release_portaudio();
    return result;
}

std::vector<AudioDeviceInfo> PortAudioIo::devices() const { return list_devices(); }

bool PortAudioIo::open(const std::string& input, const std::string& output,
                       double sample_rate, AudioInputCallback callback) {
    close();
    callback_ = std::move(callback);
    captured_samples_ = 0;
    dropped_samples_ = 0;
    capture_interruptions_ = 0;
    tx_cancel_ = false;
    set_error({});
    if (!initialized_) {
        set_error("PortAudio is not initialized");
        return false;
    }
    if (!std::isfinite(sample_rate) || std::abs(sample_rate - kSampleRate) > 0.5) {
        set_error("PortAudio modem audio requires 48,000 Hz");
        return false;
    }

    int input_device = Pa_GetDefaultInputDevice();
    output_device_ = Pa_GetDefaultOutputDevice();
    const bool input_disabled = input == "none";
    if ((!input_disabled && !parse_device_id(input, input_device)) ||
        (!output.empty() && !parse_device_id(output, output_device_))) {
        set_error("Audio device IDs must use the portaudio:N format");
        return false;
    }
    if (!valid_device(output_device_) || (!input_disabled && !valid_device(input_device))) {
        set_error("Selected PortAudio device is unavailable");
        return false;
    }

    const auto* output_info = Pa_GetDeviceInfo(output_device_);
    if (!output_info || output_info->maxOutputChannels < 1) {
        set_error("Selected PortAudio output has no mono playback channel");
        return false;
    }
    PaStreamParameters output_parameters{};
    output_parameters.device = output_device_;
    output_parameters.channelCount = 1;
    output_parameters.sampleFormat = paFloat32;
    output_parameters.suggestedLatency = output_info->defaultLowOutputLatency;
    const auto supported = Pa_IsFormatSupported(nullptr, &output_parameters, sample_rate);
    if (supported != paFormatIsSupported) {
        set_error("PortAudio TX format unavailable: " + pa_error(supported));
        return false;
    }

    if (input_disabled) return true;

    const auto* info = Pa_GetDeviceInfo(input_device);
    if (!info || info->maxInputChannels < 1) {
        set_error("Selected PortAudio input has no mono capture channel");
        return false;
    }
    PaStreamParameters parameters{};
    parameters.device = input_device;
    parameters.channelCount = 1;
    parameters.sampleFormat = paFloat32;
    parameters.suggestedLatency = info->defaultLowInputLatency;
    const auto opened = Pa_OpenStream(&input_stream_, &parameters, nullptr, sample_rate,
                                      kFramesPerBuffer, paNoFlag, &PortAudioIo::callback, this);
    if (opened != paNoError) {
        input_stream_ = nullptr;
        set_error("PortAudio RX open failed: " + pa_error(opened));
        return false;
    }
    const auto started = Pa_StartStream(input_stream_);
    if (started != paNoError) {
        set_error("PortAudio RX start failed: " + pa_error(started));
        close();
        return false;
    }
    return true;
}

void PortAudioIo::close() {
    if (input_stream_) {
        Pa_StopStream(input_stream_);
        Pa_CloseStream(input_stream_);
        input_stream_ = nullptr;
    }
    std::lock_guard lock(chunks_mutex_);
    chunks_.clear();
    output_device_ = paNoDevice;
}

bool PortAudioIo::write(std::span<const float> samples) {
    size_t offset = 0;
    return write_stream([&]() {
        const size_t count = std::min<size_t>(kFramesPerBuffer, samples.size() - offset);
        std::vector<float> block(samples.begin() + offset, samples.begin() + offset + count);
        offset += count;
        return block;
    });
}

void PortAudioIo::cancel_write() { tx_cancel_ = true; }

bool PortAudioIo::write_stream(AudioSampleSource source) {
    set_error({});
    if (!source || tx_cancel_.load()) {
        set_error("Transmission cancelled");
        return false;
    }
    if (!initialized_ || output_device_ == paNoDevice) {
        set_error("PortAudio TX is not open");
        return false;
    }
    const auto* info = Pa_GetDeviceInfo(output_device_);
    if (!info || info->maxOutputChannels < 1) {
        set_error("Selected PortAudio output has no mono playback channel");
        return false;
    }
    PaStreamParameters parameters{};
    parameters.device = output_device_;
    parameters.channelCount = 1;
    parameters.sampleFormat = paFloat32;
    parameters.suggestedLatency = info->defaultLowOutputLatency;
    struct OutputStream {
        PaStream* stream = nullptr;
        bool completed = false;
        ~OutputStream() {
            if (!stream) return;
            if (completed) Pa_StopStream(stream);
            else Pa_AbortStream(stream);
            Pa_CloseStream(stream);
        }
    } output;
    auto result = Pa_OpenStream(&output.stream, nullptr, &parameters, kSampleRate,
                                kFramesPerBuffer, paNoFlag, nullptr, nullptr);
    if (result == paNoError) result = Pa_StartStream(output.stream);
    bool wrote = false;
    if (result == paNoError) {
        while (!tx_cancel_.load()) {
            const auto block = source();
            if (block.empty()) { output.completed = wrote; break; }
            // Even an arbitrary producer is written in short blocks so Stop
            // is checked frequently while the same output stream stays open.
            for (size_t pos = 0; pos < block.size() && !tx_cancel_.load();) {
                const auto count = static_cast<unsigned long>(std::min<size_t>(kFramesPerBuffer, block.size() - pos));
                result = Pa_WriteStream(output.stream, block.data() + pos, count);
                if (result != paNoError) break;
                wrote = true;
                pos += count;
            }
            if (result != paNoError) break;
        }
    }
    if (tx_cancel_.load()) { set_error("Transmission cancelled"); return false; }
    if (result != paNoError) {
        set_error("PortAudio TX failed: " + pa_error(result));
        return false;
    }
    if (!wrote) set_error("No audio to transmit");
    return wrote;
}

int PortAudioIo::callback(const void* input, void*, unsigned long frames,
                          const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags flags,
                          void* user_data) {
    auto* self = static_cast<PortAudioIo*>(user_data);
    if (self && (flags & (paInputOverflow | paInputUnderflow))) ++self->capture_interruptions_;
    if (!self || !input || frames == 0) return paContinue;
    const auto* samples = static_cast<const float*>(input);
    std::vector<float> copy(samples, samples + frames);
    self->captured_samples_.fetch_add(copy.size());
    {
        std::lock_guard lock(self->chunks_mutex_);
        if (self->chunks_.size() >= kMaxQueuedChunks) {
            self->dropped_samples_.fetch_add(copy.size());
        } else {
            self->chunks_.push_back(copy);
        }
    }
    if (self->callback_) self->callback_(std::span<const float>(copy));
    return paContinue;
}

std::vector<std::vector<float>> PortAudioIo::take_input_chunks() {
    std::vector<std::vector<float>> result;
    std::lock_guard lock(chunks_mutex_);
    result.reserve(chunks_.size());
    while (!chunks_.empty()) {
        result.push_back(std::move(chunks_.front()));
        chunks_.pop_front();
    }
    return result;
}

size_t PortAudioIo::captured_samples() const { return captured_samples_.load(); }
size_t PortAudioIo::dropped_samples() const { return dropped_samples_.load(); }

std::string PortAudioIo::last_error() const {
    std::lock_guard lock(error_mutex_);
    return error_;
}

} // namespace fectty

#endif
