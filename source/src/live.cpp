#ifdef _WIN32

#include "fectty/arq.hpp"
#include "fectty/acquisition.hpp"
#include "fectty/frame_receiver.hpp"
#include "fectty/reliable_receiver.hpp"
#include "fectty/session.hpp"
#include "fectty/level_meter.hpp"
#include "fectty/wav.hpp"
#include "fectty/parse.hpp"

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <limits>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr DWORD kSampleRate = 48000;
constexpr WORD kChannels = 1;
constexpr WORD kBits = 16;
constexpr size_t kBufferSamples = 2048;
constexpr size_t kBufferCount = 16;

WAVEFORMATEX pcm_format() {
    WAVEFORMATEX f{};
    f.wFormatTag = WAVE_FORMAT_PCM;
    f.nChannels = kChannels;
    f.nSamplesPerSec = kSampleRate;
    f.wBitsPerSample = kBits;
    f.nBlockAlign = WORD(f.nChannels * f.wBitsPerSample / 8);
    f.nAvgBytesPerSec = f.nSamplesPerSec * f.nBlockAlign;
    f.cbSize = 0;
    return f;
}

class Capture {
    HWAVEIN handle_ = nullptr;
    std::vector<std::vector<int16_t>> buffers_;
    std::vector<WAVEHDR> headers_;
    std::mutex mutex_;
    std::deque<std::vector<float>> chunks_;
    std::atomic<bool> stopping_{false};
    std::atomic<unsigned> callbacks_{0};
    std::atomic<size_t> dropped_samples_{0};

    static void CALLBACK callback(HWAVEIN, UINT message, DWORD_PTR instance,
                                  DWORD_PTR param1, DWORD_PTR) {
        auto* self = reinterpret_cast<Capture*>(instance);
        if (!self || message != WIM_DATA || self->stopping_.load()) return;
        self->callbacks_.fetch_add(1);
        auto* header = reinterpret_cast<WAVEHDR*>(param1);
        const size_t count = header->dwBytesRecorded / sizeof(int16_t);
        if (count != 0) {
            std::vector<float> chunk(count);
            const auto* samples = reinterpret_cast<const int16_t*>(header->lpData);
            for (size_t i = 0; i < count; ++i) {
                chunk[i] = static_cast<float>(samples[i]) / 32768.0f;
            }
            {
                std::lock_guard lock(self->mutex_);
                // Keep callback-side memory bounded if DSP processing is
                // temporarily slower than the audio device.
                if (self->chunks_.size() >= 1024) {
                    self->dropped_samples_.fetch_add(count);
                } else {
                    self->chunks_.push_back(std::move(chunk));
                }
            }
        }
        if (!self->stopping_.load()) {
            header->dwBytesRecorded = 0;
            waveInAddBuffer(self->handle_, header, sizeof(WAVEHDR));
        }
        self->callbacks_.fetch_sub(1);
    }

public:
    ~Capture() { close(); }

    bool open(UINT device) {
        const auto format = pcm_format();
        const MMRESULT result = waveInOpen(
            &handle_, device, &format,
            reinterpret_cast<DWORD_PTR>(&Capture::callback),
            reinterpret_cast<DWORD_PTR>(this), CALLBACK_FUNCTION);
        if (result != MMSYSERR_NOERROR) {
            std::cerr << "waveInOpen failed: " << result << "\n";
            handle_ = nullptr;
            return false;
        }
        buffers_.resize(kBufferCount);
        headers_.resize(kBufferCount);
        for (size_t i = 0; i < kBufferCount; ++i) {
            buffers_[i].resize(kBufferSamples);
            headers_[i] = {};
            headers_[i].lpData = reinterpret_cast<LPSTR>(buffers_[i].data());
            headers_[i].dwBufferLength = DWORD(buffers_[i].size() * sizeof(int16_t));
            if (waveInPrepareHeader(handle_, &headers_[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR ||
                waveInAddBuffer(handle_, &headers_[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
                std::cerr << "waveIn buffer setup failed\n";
                close();
                return false;
            }
        }
        stopping_ = false;
        return waveInStart(handle_) == MMSYSERR_NOERROR;
    }

    std::vector<std::vector<float>> take_chunks() {
        std::lock_guard lock(mutex_);
        std::vector<std::vector<float>> result;
        result.reserve(chunks_.size());
        while (!chunks_.empty()) {
            result.push_back(std::move(chunks_.front()));
            chunks_.pop_front();
        }
        return result;
    }

    size_t dropped_samples() const { return dropped_samples_.load(); }

    void close() {
        if (!handle_) return;
        stopping_ = true;
        waveInReset(handle_);
        while (callbacks_.load() != 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        for (auto& header : headers_) waveInUnprepareHeader(handle_, &header, sizeof(WAVEHDR));
        waveInClose(handle_);
        handle_ = nullptr;
        buffers_.clear();
        headers_.clear();
    }
};

class Output {
    HWAVEOUT handle_ = nullptr;
    std::atomic<bool> done_{false};

    static void CALLBACK callback(HWAVEOUT, UINT message, DWORD_PTR instance,
                                  DWORD_PTR, DWORD_PTR) {
        if (message == WOM_DONE) {
            reinterpret_cast<Output*>(instance)->done_ = true;
        }
    }

public:
    ~Output() { close(); }

    bool play(UINT device, std::span<const float> samples, double gain) {
        const auto format = pcm_format();
        const MMRESULT opened = waveOutOpen(
            &handle_, device, &format,
            reinterpret_cast<DWORD_PTR>(&Output::callback),
            reinterpret_cast<DWORD_PTR>(this), CALLBACK_FUNCTION);
        if (opened != MMSYSERR_NOERROR) {
            std::cerr << "waveOutOpen failed: " << opened << "\n";
            handle_ = nullptr;
            return false;
        }
        std::vector<int16_t> pcm(samples.size());
        for (size_t i = 0; i < samples.size(); ++i) {
            const double value = std::clamp(static_cast<double>(samples[i]) * gain, -1.0, 1.0);
            pcm[i] = static_cast<int16_t>(value * 32767.0);
        }
        WAVEHDR header{};
        header.lpData = reinterpret_cast<LPSTR>(pcm.data());
        header.dwBufferLength = DWORD(pcm.size() * sizeof(int16_t));
        done_ = false;
        if (waveOutPrepareHeader(handle_, &header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR ||
            waveOutWrite(handle_, &header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            std::cerr << "waveOut buffer setup failed\n";
            waveOutReset(handle_);
            waveOutUnprepareHeader(handle_, &header, sizeof(WAVEHDR));
            close();
            return false;
        }
        const auto playback_seconds = std::max<long long>(
            120, static_cast<long long>(samples.size() / kSampleRate) + 60);
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::seconds(playback_seconds);
        while (!done_.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        const bool completed = done_.load();
        waveOutReset(handle_);
        waveOutUnprepareHeader(handle_, &header, sizeof(WAVEHDR));
        close();
        if (!completed) std::cerr << "waveOut playback timed out\n";
        return completed;
    }

    void close() {
        if (handle_) {
            waveOutReset(handle_);
            waveOutClose(handle_);
            handle_ = nullptr;
        }
    }
};

void list_devices() {
    std::cout << "Input devices:\n";
    const UINT inputs = waveInGetNumDevs();
    for (UINT i = 0; i < inputs; ++i) {
        WAVEINCAPS caps{};
        if (waveInGetDevCaps(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            std::wcout << "  " << i << ": " << caps.szPname << "\n";
        }
    }
    std::cout << "Output devices:\n";
    const UINT outputs = waveOutGetNumDevs();
    for (UINT i = 0; i < outputs; ++i) {
        WAVEOUTCAPS caps{};
        if (waveOutGetDevCaps(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            std::wcout << "  " << i << ": " << caps.szPname << "\n";
        }
    }
}

bool parse_uint(const std::string& value, UINT& result) {
    return fectty::parse_number(value, result);
}

void usage() {
    std::cout << "FEC-RTTY live Windows audio bench\n"
              << "  fectty-live --list\n"
              << "  fectty-live --receive --input N --seconds N\n"
              << "  fectty-live --send TEXT --output N [--gain 0.5]\n"
              << "CAT/PTT are deliberately disabled; rig control is NullRig.\n";
}

int receive(UINT input, int seconds) {
    Capture capture;
    if (!capture.open(input)) return 1;
    const fectty::FskConfig config{};
    fectty::ReliableReceiver receiver(config);
    std::vector<float> captured;
    std::cout << "RX listening on input " << input << " for " << seconds << " seconds\n";
    auto consume_chunks = [&](std::vector<std::vector<float>> chunks) {
        const auto print_addressed_frames = [&]() {
            for (const auto& frame : receiver.take_frames()) {
                const auto data = fectty::decode_arq_data(frame);
                if (data) {
                    std::cout << "RX: "
                              << std::string(data->payload.begin(),
                                            data->payload.end())
                              << "\n"
                              << std::flush;
                }
            }
        };
        for (auto& chunk : chunks) {
            captured.insert(captured.end(), chunk.begin(), chunk.end());
            const auto text = receiver.push(std::span<const float>(chunk));
            if (!text.empty()) std::cout << "RX: " << text << "\n" << std::flush;
            print_addressed_frames();
        }
    };
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < deadline) {
        consume_chunks(capture.take_chunks());
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    consume_chunks(capture.take_chunks());
    capture.close();
    consume_chunks(capture.take_chunks());
    const auto final_text = receiver.finish();
    if (!final_text.empty()) std::cout << "RX: " << final_text << "\n" << std::flush;
    for (const auto& frame : receiver.take_frames()) {
        const auto data = fectty::decode_arq_data(frame);
        if (data) {
            std::cout << "RX: "
                      << std::string(data->payload.begin(), data->payload.end())
                      << "\n"
                      << std::flush;
        }
    }
    const auto level = fectty::measure_level(captured);
    fectty::write_wav_mono16("fectty-live-rx.wav", captured, kSampleRate);
    const auto& stats = receiver.stats();
    std::cout << "Captured samples: " << captured.size()
              << " peak: " << level.peak
              << " rms: " << level.rms << "\n";
    std::cout << "Frames valid: " << stats.frames_ok << "\n"
              << "CRC failures: " << stats.crc_failures << "\n"
              << "Sequence gaps: " << stats.sequence_gaps << "\n"
              << "Acquisitions: " << stats.acquisitions << "\n"
              << "Reacquisitions: " << stats.reacquisitions << "\n"
              << "Frequency offset: " << stats.last_frequency_offset_hz << " Hz\n"
              << "Receive state: " << fectty::reliable_rx_state_name(stats.state) << "\n"
              << "Samples buffered: " << stats.samples_buffered << "\n"
              << "Dropped samples: " << capture.dropped_samples() << "\n";
    return stats.frames_ok != 0 ? 0 : 2;
}

int send(UINT output, const std::string& text, double gain) {
    fectty::ModemSession session;
    const auto samples = session.transmit(text);
    fectty::write_wav_mono16("fectty-live-tx.wav", samples, kSampleRate);
    std::cout << "TX sending " << text.size() << " bytes (" << samples.size()
              << " samples) on output " << output << "\n";
    Output player;
    if (!player.play(output, samples, gain)) return 1;
    std::cout << "TX complete\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--list") {
        list_devices();
        return 0;
    }
    if (argc < 2) {
        usage();
        return 1;
    }

    std::string send_text;
    UINT input = WAVE_MAPPER;
    UINT output = WAVE_MAPPER;
    int seconds = 30;
    double gain = 0.5;
    bool receiving = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--receive") receiving = true;
        else if (arg == "--send" && i + 1 < argc) send_text = argv[++i];
        else if (arg == "--input") {
            if (i + 1 >= argc || !parse_uint(argv[++i], input)) return 1;
        }
        else if (arg == "--output") {
            if (i + 1 >= argc || !parse_uint(argv[++i], output)) return 1;
        }
        else if (arg == "--seconds") {
            if (i + 1 >= argc) return 1;
            if (!fectty::parse_number(argv[++i], seconds) || seconds < 1 || seconds > 86400) return 1;
        }
        else if (arg == "--gain") {
            if (i + 1 >= argc) return 1;
            if (!fectty::parse_number(argv[++i], gain) || gain < 0 || gain > 1) return 1;
        }
        else {
            usage();
            return 1;
        }
    }
    const bool sending = !send_text.empty();
    if (receiving == sending) {
        usage();
        return 1;
    }
    return receiving ? receive(input, seconds) : send(output, send_text, gain);
}

#else
int main() { return 0; }
#endif
