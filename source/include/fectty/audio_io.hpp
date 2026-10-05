#pragma once
#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace fectty {
struct AudioDeviceInfo { std::string id; std::string name; bool input=false; bool output=false; };
// Endpoint indices are ephemeral. Restore only a uniquely named endpoint in
// the requested direction; never silently choose a different or ambiguous one.
inline std::string resolve_saved_audio_device(std::span<const AudioDeviceInfo> devices,
    std::string_view saved_id, std::string_view saved_name, bool input) {
    if (input && saved_id == "none") return "none";
    if (saved_name.empty()) return {};
    std::string result;
    for (const auto& device : devices) {
        if ((input ? device.input : device.output) && device.name == saved_name) {
            if (!result.empty()) return {};
            result = device.id;
        }
    }
    return result;
}
using AudioInputCallback=std::function<void(std::span<const float>)>;
using AudioSampleSource=std::function<std::vector<float>()>;
class IAudioIo {
public:
    virtual ~IAudioIo() = default;
    virtual std::vector<AudioDeviceInfo> devices() const = 0;
    virtual bool open(const std::string& input, const std::string& output,
                      double sample_rate, AudioInputCallback cb) = 0;
    virtual void close() = 0;
    virtual bool write(std::span<const float>) = 0;
    // The source ends with an empty block. Implementations keep the device
    // open across blocks so there are no gaps between modem frames.
    virtual bool write_stream(AudioSampleSource) { return false; }
    virtual void cancel_write() {}

    // Backends queue capture chunks so the modem worker can consume them
    // without touching the audio callback thread. Legacy backends may keep
    // the default empty/zero implementations.
    virtual std::vector<std::vector<float>> take_input_chunks() { return {}; }
    virtual size_t captured_samples() const { return 0; }
    virtual size_t dropped_samples() const { return 0; }
    virtual size_t capture_interruptions() const { return 0; }
    virtual std::string last_error() const { return {}; }
};
}
