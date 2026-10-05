#include "fectty/channel.hpp"
#include "fectty/session.hpp"
#include "fectty/portaudio_io.hpp"
#include "fectty/winmm_io.hpp"
#include <iostream>
#include <memory>

// Explicit hardware bench helper, not an automatic CTest. Never keys a rig.
int main(int argc, char** argv) {
    if (argc != 3) { std::cerr << "Usage: fectty-noise-prefix-bench BACKEND:N clean|continuous\n"; return 2; }
    const std::string output_id = argv[1], scenario = argv[2];
    if (scenario != "clean" && scenario != "continuous") return 2;
    std::unique_ptr<fectty::IAudioIo> output;
    if (output_id.starts_with("winmm:")) output = std::make_unique<fectty::WinMmAudioIo>();
#ifdef FECTTY_HAS_PORTAUDIO
    else if (output_id.starts_with("portaudio:")) output = std::make_unique<fectty::PortAudioIo>();
#endif
    if (!output) { std::cerr << "Backend unavailable\n"; return 2; }
    // Numeric endpoints can move. Refuse to send a bench waveform to a stale
    // number that is now speakers or a radio rather than the requested cable.
    std::string name;
    for (const auto& device : output->devices())
        if (device.id == output_id && device.output) name = device.name;
    if (name.find("CABLE Input") == std::string::npos) {
        std::cerr << "Choose the current VB-Audio CABLE Input output; refusing endpoint [" << name << "]\n";
        return 2;
    }
    auto audio = fectty::apply_channel(std::vector<float>(48000, 0), {0.02, 1, 0, 0, 901});
    fectty::ModemSession tx;
    auto burst = tx.transmit("PREFIX TEST");
    for (auto& sample : burst) sample *= 0.25f;
    if (scenario == "continuous") burst = fectty::apply_channel(burst, {0.02, 1, 0, 0, 902});
    audio.insert(audio.end(), burst.begin(), burst.end());
    audio.insert(audio.end(), 4800, 0);
    if (!output->open("none", output_id, 48000, {})) { std::cerr << output->last_error() << '\n'; return 2; }
    const bool success = output->write(audio);
    std::cout << "scenario=" << scenario << " device=[" << name << "] samples=" << audio.size()
              << " tx_success=" << success << " error=[" << output->last_error() << "]\n";
    return success ? 0 : 2;
}
