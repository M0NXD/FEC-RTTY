#include "fectty/winmm_io.hpp"
#include "fectty/parse.hpp"

#ifdef _WIN32

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <limits>
#include <mutex>
#include <span>
#include <thread>

namespace fectty {
namespace {

constexpr DWORD kSampleRate = 48000;
constexpr WORD kChannels = 1;
constexpr WORD kBits = 16;
constexpr size_t kBufferSamples = 2048;
constexpr size_t kBufferCount = 16;

std::string utf8_name(const wchar_t* name) {
    if (!name || !*name) return {};
    const int length = WideCharToMultiByte(CP_UTF8, 0, name, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1) return {};
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, name, -1, result.data(), length, nullptr, nullptr);
    result.resize(static_cast<size_t>(length - 1));
    return result;
}

std::string mm_error(MMRESULT result) {
    char buffer[256]{};
    if (waveOutGetErrorTextA(result, buffer, sizeof(buffer)) == MMSYSERR_NOERROR) {
        return std::string(buffer);
    }
    return "WinMM error " + std::to_string(result);
}

bool parse_device_id(const std::string& id, UINT& device) {
    if (id.empty()) {
        device = WAVE_MAPPER;
        return true;
    }
    constexpr std::string_view prefix = "winmm:";
    if (!id.starts_with(prefix)) return false;
    return parse_number(std::string_view(id).substr(prefix.size()), device);
}

WAVEFORMATEX pcm_format() {
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = kChannels;
    format.nSamplesPerSec = kSampleRate;
    format.wBitsPerSample = kBits;
    format.nBlockAlign = WORD(format.nChannels * format.wBitsPerSample / 8);
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    return format;
}

struct Playback {
    std::atomic<bool> done{false};

    static void CALLBACK callback(HWAVEOUT, UINT message, DWORD_PTR instance,
                                  DWORD_PTR, DWORD_PTR) {
        if (message == WOM_DONE) {
            static_cast<Playback*>(reinterpret_cast<void*>(instance))->done = true;
        }
    }
};

} // namespace

class WinMmAudioIo::Impl {
public:
    HWAVEIN input_handle = nullptr;
    std::vector<std::vector<int16_t>> buffers;
    std::vector<WAVEHDR> headers;
    std::string output_id;
    std::atomic<bool> stopping{false};
    std::atomic<bool> tx_cancel{false};
    std::atomic<unsigned> callbacks{0};
    mutable std::mutex chunks_mutex;
    std::deque<std::vector<float>> chunks;
    std::atomic<size_t> captured_samples{0};
    std::atomic<size_t> dropped_samples{0};
    mutable std::mutex error_mutex;
    std::string error;

    static void CALLBACK input_callback(HWAVEIN, UINT message, DWORD_PTR instance,
                                        DWORD_PTR param1, DWORD_PTR) {
        auto* self = reinterpret_cast<Impl*>(instance);
        if (!self || message != WIM_DATA) return;
        self->callbacks.fetch_add(1);
        if (self->stopping.load()) {
            self->callbacks.fetch_sub(1);
            return;
        }

        auto* header = reinterpret_cast<WAVEHDR*>(param1);
        const size_t count = header->dwBytesRecorded / sizeof(int16_t);
        if (count != 0) {
            self->captured_samples.fetch_add(count);
            const auto* samples = reinterpret_cast<const int16_t*>(header->lpData);
            std::vector<float> converted(count);
            for (size_t i = 0; i < count; ++i) {
                converted[i] = static_cast<float>(samples[i]) / 32768.0f;
            }
            std::lock_guard lock(self->chunks_mutex);
            if (self->chunks.size() >= 1024) {
                self->dropped_samples.fetch_add(count);
            } else {
                self->chunks.push_back(std::move(converted));
            }
        }

        if (!self->stopping.load()) {
            header->dwBytesRecorded = 0;
            const auto queued = waveInAddBuffer(self->input_handle, header, sizeof(WAVEHDR));
            if (queued != MMSYSERR_NOERROR) {
                self->set_error("RX buffer requeue failed: " + mm_error(queued));
            }
        }
        self->callbacks.fetch_sub(1);
    }

    void set_error(std::string message) {
        std::lock_guard lock(error_mutex);
        error = std::move(message);
    }

    void close_input() {
        if (!input_handle) {
            buffers.clear();
            headers.clear();
            std::lock_guard lock(chunks_mutex);
            chunks.clear();
            return;
        }
        stopping = true;
        waveInReset(input_handle);
        while (callbacks.load() != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        for (auto& header : headers) {
            waveInUnprepareHeader(input_handle, &header, sizeof(WAVEHDR));
        }
        waveInClose(input_handle);
        input_handle = nullptr;
        buffers.clear();
        headers.clear();
        {
            std::lock_guard lock(chunks_mutex);
            chunks.clear();
        }
    }

    ~Impl() { close_input(); }
};

WinMmAudioIo::WinMmAudioIo() : impl_(std::make_unique<Impl>()) {}

WinMmAudioIo::~WinMmAudioIo() { close(); }

std::vector<AudioDeviceInfo> WinMmAudioIo::list_devices() {
    std::vector<AudioDeviceInfo> result;
    for (UINT i = 0; i < waveInGetNumDevs(); ++i) {
        WAVEINCAPSW caps{};
        if (waveInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            auto name = utf8_name(caps.szPname);
            if (name.empty()) name = "WinMM input " + std::to_string(i);
            result.push_back({"winmm:" + std::to_string(i), std::move(name), true, false});
        }
    }
    for (UINT i = 0; i < waveOutGetNumDevs(); ++i) {
        WAVEOUTCAPSW caps{};
        if (waveOutGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            auto name = utf8_name(caps.szPname);
            if (name.empty()) name = "WinMM output " + std::to_string(i);
            result.push_back({"winmm:" + std::to_string(i), std::move(name), false, true});
        }
    }
    return result;
}

std::vector<AudioDeviceInfo> WinMmAudioIo::devices() const { return list_devices(); }

bool WinMmAudioIo::open(const std::string& input, const std::string& output,
                        double sample_rate, AudioInputCallback callback) {
    close();
    (void)callback;
    impl_->set_error({});
    impl_->captured_samples = 0;
    impl_->dropped_samples = 0;
    impl_->tx_cancel = false;
    if (!std::isfinite(sample_rate) || std::abs(sample_rate - static_cast<double>(kSampleRate)) > 0.5) {
        impl_->set_error("WinMM modem audio requires 48,000 Hz");
        return false;
    }

    UINT input_device = WAVE_MAPPER;
    UINT output_device = WAVE_MAPPER;
    const bool input_disabled = input == "none";
    if ((!input_disabled && !parse_device_id(input, input_device)) ||
        !parse_device_id(output, output_device)) {
        impl_->set_error("Audio device IDs must use the winmm:N format");
        return false;
    }
    const auto output_format = pcm_format();
    const auto output_check = waveOutOpen(nullptr, output_device, &output_format,
                                          0, 0, WAVE_FORMAT_QUERY);
    if (output_check != MMSYSERR_NOERROR) {
        impl_->set_error("TX device unavailable: " + mm_error(output_check));
        return false;
    }
    impl_->output_id = output;
    if (input_disabled) return true;

    const auto format = pcm_format();
    const auto result = waveInOpen(
        &impl_->input_handle, input_device, &format,
        reinterpret_cast<DWORD_PTR>(&Impl::input_callback),
        reinterpret_cast<DWORD_PTR>(impl_.get()), CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) {
        impl_->input_handle = nullptr;
        impl_->set_error("RX open failed: " + mm_error(result));
        return false;
    }

    impl_->buffers.resize(kBufferCount);
    impl_->headers.resize(kBufferCount);
    for (size_t i = 0; i < kBufferCount; ++i) {
        impl_->buffers[i].resize(kBufferSamples);
        impl_->headers[i] = {};
        impl_->headers[i].lpData = reinterpret_cast<LPSTR>(impl_->buffers[i].data());
        impl_->headers[i].dwBufferLength = DWORD(impl_->buffers[i].size() * sizeof(int16_t));
        if (waveInPrepareHeader(impl_->input_handle, &impl_->headers[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR ||
            waveInAddBuffer(impl_->input_handle, &impl_->headers[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            impl_->set_error("RX buffer setup failed");
            close();
            return false;
        }
    }
    impl_->stopping = false;
    const auto started = waveInStart(impl_->input_handle);
    if (started != MMSYSERR_NOERROR) {
        impl_->set_error("RX start failed: " + mm_error(started));
        close();
        return false;
    }
    return true;
}

void WinMmAudioIo::close() {
    if (!impl_) return;
    impl_->close_input();
    impl_->output_id.clear();
}

bool WinMmAudioIo::write(std::span<const float> samples) {
    size_t offset = 0;
    return write_stream([&]() {
        const size_t count = std::min(kBufferSamples, samples.size() - offset);
        std::vector<float> block(samples.begin() + offset, samples.begin() + offset + count);
        offset += count;
        return block;
    });
}

void WinMmAudioIo::cancel_write() { impl_->tx_cancel = true; }

bool WinMmAudioIo::write_stream(AudioSampleSource source) {
    if (!impl_ || !source) return false;
    impl_->set_error({});
    if (impl_->tx_cancel.load()) {
        impl_->set_error("Transmission cancelled");
        return false;
    }

    UINT output_device = WAVE_MAPPER;
    if (!parse_device_id(impl_->output_id, output_device)) {
        impl_->set_error("TX output device is not a WinMM device");
        return false;
    }
    const auto format = pcm_format();
    // Keep a small ring of buffers queued. This bounds memory, preserves the
    // continuous waveform, and lets Stop interrupt a long message promptly.
    struct OutputBuffers {
        HWAVEOUT handle = nullptr;
        bool completed = false;
        std::vector<std::vector<int16_t>> pcm{8, std::vector<int16_t>(kBufferSamples)};
        std::vector<WAVEHDR> headers{8};
        ~OutputBuffers() {
            if (!handle) return;
            if (!completed) waveOutReset(handle);
            for (auto& header : headers) {
                if (header.dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(handle, &header, sizeof(header));
            }
            waveOutClose(handle);
        }
    } output;
    const auto opened = waveOutOpen(
        &output.handle, output_device, &format, 0, 0, CALLBACK_NULL);
    if (opened != MMSYSERR_NOERROR) {
        impl_->set_error("TX open failed: " + mm_error(opened));
        return false;
    }
    const auto paused = waveOutPause(output.handle);
    if (paused != MMSYSERR_NOERROR) {
        impl_->set_error("TX prefill failed: " + mm_error(paused));
        return false;
    }

    std::vector<float> block;
    size_t position = 0, slot = 0;
    bool ended = false, wrote = false, started = false;
    size_t queued = 0;
    auto progress = std::chrono::steady_clock::now();
    while (!impl_->tx_cancel.load()) {
        auto& header = output.headers[slot];
        if (!(header.dwFlags & WHDR_PREPARED) || (header.dwFlags & WHDR_DONE)) {
            if (header.dwFlags & WHDR_PREPARED) {
                waveOutUnprepareHeader(output.handle, &header, sizeof(header));
                header = {};
                progress = std::chrono::steady_clock::now();
            }
            if (position == block.size() && !ended) {
                block = source();
                position = 0;
                ended = block.empty();
            }
            if (!ended) {
                const size_t count = std::min(kBufferSamples, block.size() - position);
                auto& pcm = output.pcm[slot];
                for (size_t i = 0; i < count; ++i) {
                    const auto sample = block[position + i];
                    pcm[i] = static_cast<int16_t>((std::isfinite(sample)
                        ? std::clamp(sample, -1.0f, 1.0f) : 0.0f) * 32767.0f);
                }
                position += count;
                header.lpData = reinterpret_cast<LPSTR>(pcm.data());
                header.dwBufferLength = static_cast<DWORD>(count * sizeof(int16_t));
                auto result = waveOutPrepareHeader(output.handle, &header, sizeof(header));
                if (result == MMSYSERR_NOERROR) result = waveOutWrite(output.handle, &header, sizeof(header));
                if (result != MMSYSERR_NOERROR) {
                    impl_->set_error("TX buffer setup failed: " + mm_error(result));
                    return false;
                }
                wrote = true;
                ++queued;
            }
            slot = (slot + 1) % output.headers.size();
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        if (!started && wrote && (queued >= output.headers.size() || ended)) {
            const auto result = waveOutRestart(output.handle);
            if (result != MMSYSERR_NOERROR) {
                impl_->set_error("TX start failed: " + mm_error(result));
                return false;
            }
            started = true;
            progress = std::chrono::steady_clock::now();
        }
        if (ended && std::none_of(output.headers.begin(), output.headers.end(), [](const WAVEHDR& h) {
            return (h.dwFlags & WHDR_PREPARED) && !(h.dwFlags & WHDR_DONE);
        })) {
            if (!wrote) impl_->set_error("No audio to transmit");
            output.completed = wrote;
            return wrote;
        }
        if (ended) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        if (std::chrono::steady_clock::now() - progress > std::chrono::seconds(10)) {
            impl_->set_error("TX playback timed out");
            return false;
        }
    }
    impl_->set_error("Transmission cancelled");
    return false;
}

std::vector<std::vector<float>> WinMmAudioIo::take_input_chunks() {
    std::vector<std::vector<float>> result;
    if (!impl_) return result;
    std::lock_guard lock(impl_->chunks_mutex);
    result.reserve(impl_->chunks.size());
    while (!impl_->chunks.empty()) {
        result.push_back(std::move(impl_->chunks.front()));
        impl_->chunks.pop_front();
    }
    return result;
}

size_t WinMmAudioIo::dropped_samples() const {
    return impl_ ? impl_->dropped_samples.load() : 0;
}

size_t WinMmAudioIo::captured_samples() const {
    return impl_ ? impl_->captured_samples.load() : 0;
}

std::string WinMmAudioIo::last_error() const {
    if (!impl_) return {};
    std::lock_guard lock(impl_->error_mutex);
    return impl_->error;
}

} // namespace fectty

#else

namespace fectty {
class WinMmAudioIo::Impl {};
WinMmAudioIo::WinMmAudioIo() : impl_(std::make_unique<Impl>()) {}
WinMmAudioIo::~WinMmAudioIo() = default;
std::vector<AudioDeviceInfo> WinMmAudioIo::list_devices() { return {}; }
std::vector<AudioDeviceInfo> WinMmAudioIo::devices() const { return {}; }
bool WinMmAudioIo::open(const std::string&, const std::string&, double, AudioInputCallback) { return false; }
void WinMmAudioIo::close() {}
bool WinMmAudioIo::write(std::span<const float>) { return false; }
bool WinMmAudioIo::write_stream(AudioSampleSource) { return false; }
void WinMmAudioIo::cancel_write() {}
std::vector<std::vector<float>> WinMmAudioIo::take_input_chunks() { return {}; }
size_t WinMmAudioIo::captured_samples() const { return 0; }
size_t WinMmAudioIo::dropped_samples() const { return 0; }
std::string WinMmAudioIo::last_error() const { return "WinMM is only available on Windows"; }
} // namespace fectty

#endif
