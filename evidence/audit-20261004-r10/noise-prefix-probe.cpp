#include "fectty/reliable_receiver.hpp"
#include "fectty/channel.hpp"
#include <algorithm>
#include <iostream>
#include <span>

int main() {
    const std::string expected = "PREFIX TEST";
    fectty::ModemSession tx;
    const auto waveform = tx.transmit(expected);
    for (const std::string scenario : {"clean", "silent-prefix", "noisy-prefix", "noisy-continuous"}) {
      auto audio = scenario == "clean" ? std::vector<float>{} : std::vector<float>(48000, 0);
      if (scenario == "noisy-prefix" || scenario == "noisy-continuous")
        audio = fectty::apply_channel(audio, {0.02, 1, 0, 0, 901});
      const auto burst = scenario == "noisy-continuous"
        ? fectty::apply_channel(waveform, {0.02, 1, 0, 0, 902}) : waveform;
      audio.insert(audio.end(), burst.begin(), burst.end());
      audio.insert(audio.end(), 4800, 0);
      for (const size_t block : {size_t{2048}, size_t{48000}, audio.size()}) {
        fectty::ReliableReceiver rx;
        std::string result;
        for (size_t offset = 0; offset < audio.size(); offset += block)
            result += rx.push(std::span<const float>(audio).subspan(offset, std::min(block, audio.size()-offset)));
        result += rx.finish();
        std::cout << "scenario=" << scenario << " block=" << block << " exact=" << (result==expected)
                  << " frames=" << rx.stats().frames_ok << " crc=" << rx.stats().crc_failures
                  << " text=[" << result << "]\n";
      }
    }
}
