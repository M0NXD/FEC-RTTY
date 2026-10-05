#include "fectty/channel.hpp"
#include "fectty/session.hpp"
#include "fectty/winmm_io.hpp"
#include <iostream>

int main() {
    auto audio = fectty::apply_channel(std::vector<float>(48000, 0), {0.02, 1, 0, 0, 901});
    fectty::ModemSession tx;
    auto wave = tx.transmit("PREFIX TEST");
    for (auto& sample : wave) sample *= 0.25f;
    audio.insert(audio.end(), wave.begin(), wave.end());
    audio.insert(audio.end(), 4800, 0);
    fectty::WinMmAudioIo output;
    if (!output.open("none", "winmm:1", 48000, {})) { std::cerr << output.last_error() << '\n'; return 2; }
    const bool success = output.write(audio);
    std::cout << "VB-Audio noisy-prefix TX success=" << success << " error=[" << output.last_error() << "]\n";
    return success ? 0 : 2;
}
