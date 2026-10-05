#pragma once
#include "fectty/audio_io.hpp"
#ifdef FECTTY_HAS_PORTAUDIO
#include <portaudio.h>
#include <atomic>
#include <deque>
#include <mutex>
#include <string>
namespace fectty {
class PortAudioIo final : public IAudioIo {
    PaStream* input_stream_ = nullptr;
    int output_device_ = paNoDevice;
    AudioInputCallback callback_;
    mutable std::mutex chunks_mutex_;
    std::deque<std::vector<float>> chunks_;
    std::atomic<size_t> captured_samples_{0};
    std::atomic<size_t> dropped_samples_{0};
    std::atomic<size_t> capture_interruptions_{0};
    std::atomic<bool> tx_cancel_{false};
    mutable std::mutex error_mutex_;
    std::string error_;
    bool initialized_ = false;

    void set_error(std::string message);

public:
    PortAudioIo();
    ~PortAudioIo() override;

    static std::vector<AudioDeviceInfo> list_devices();
    std::vector<AudioDeviceInfo> devices() const override;
    bool open(const std::string& input, const std::string& output,
              double sample_rate, AudioInputCallback callback) override;
    void close() override;
    bool write(std::span<const float> samples) override;
    bool write_stream(AudioSampleSource source) override;
    void cancel_write() override;
    std::vector<std::vector<float>> take_input_chunks() override;
    size_t captured_samples() const override;
    size_t dropped_samples() const override;
    size_t capture_interruptions() const override { return capture_interruptions_.load(); }
    std::string last_error() const override;

    static int callback(const void* input, void*, unsigned long frames,
                        const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags,
                        void* user_data);
};
}
#endif
