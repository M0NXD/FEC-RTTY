#pragma once

#include "fectty/audio_io.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace fectty {

// Windows WinMM audio adapter used by the desktop session and the local bench
// path. It keeps the callback boundary small: audio callbacks hand samples to
// the caller, while transmit writes are deliberately blocking and can be run
// by a worker owned by the GUI.
class WinMmAudioIo final : public IAudioIo {
    class Impl;
    std::unique_ptr<Impl> impl_;

public:
    WinMmAudioIo();
    ~WinMmAudioIo() override;

    static std::vector<AudioDeviceInfo> list_devices();

    std::vector<AudioDeviceInfo> devices() const override;
    bool open(const std::string& input, const std::string& output,
              double sample_rate, AudioInputCallback callback) override;
    void close() override;
    bool write(std::span<const float> samples) override;
    bool write_stream(AudioSampleSource source) override;
    void cancel_write() override;

    std::vector<std::vector<float>> take_input_chunks();
    size_t captured_samples() const;
    size_t dropped_samples() const;

    std::string last_error() const;
};

} // namespace fectty
