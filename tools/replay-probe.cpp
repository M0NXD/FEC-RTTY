#include "fectty/reliable_receiver.hpp"
#include "fectty/acquisition.hpp"
#include "fectty/wav.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    fectty::WavData wav;
    if (std::string(argv[1]) == "--synthetic") {
        fectty::ModemSession a, b;
        wav.samples.assign(503, 0.0f);
        auto first = a.transmit("ONE "), second = b.transmit("TWO");
        wav.samples.insert(wav.samples.end(), first.begin(), first.end());
        wav.samples.insert(wav.samples.end(), 2000, 0.0f);
        wav.samples.insert(wav.samples.end(), second.begin(), second.end());
        const size_t second_start = 503 + first.size() + 2000;
        for (int delta : {-2000, -80, -20, 0, 40, 400}) {
            const auto acquired = fectty::acquire(std::span<const float>(wav.samples).subspan(second_start + delta, 24000));
            std::cout << "second_delta=" << delta << " lock=" << acquired.locked << " score=" << acquired.score << " frequency=" << acquired.frequency_offset_hz << '\n';
        }
        fectty::ReliableReceiver whole;
        auto result = whole.push(wav.samples); result += whole.finish();
        std::cout << "whole_text=[" << result << "] frames=" << whole.stats().frames_ok << " crc=" << whole.stats().crc_failures << " gaps=" << whole.stats().sequence_gaps << '\n';
    } else if (!fectty::read_wav_mono16(argv[1], wav) || wav.sample_rate != 48000) return 2;
    size_t start = 0, end = wav.samples.size();
    while (start < end && std::abs(wav.samples[start]) < 0.001f) ++start;
    while (end > start && std::abs(wav.samples[end-1]) < 0.001f) --end;
    std::cout << "active_start=" << start << " active_end=" << end << " active_samples=" << end-start << '\n';
    size_t quiet = 0;
    for (size_t i = start; i < end; ++i) {
        if (std::abs(wav.samples[i]) < 0.001f) ++quiet;
        else {
            if (quiet >= 24) std::cout << "quiet inside burst at=" << i-start-quiet << " samples=" << quiet << '\n';
            quiet = 0;
        }
    }
    fectty::ReliableReceiver rx;
    std::string text;
    size_t acquisitions = 0, frames = 0, crcs = 0;
    for (size_t pos = 0; pos < wav.samples.size(); pos += 2048) {
        text += rx.push(std::span<const float>(wav.samples).subspan(pos, std::min<size_t>(2048, wav.samples.size()-pos)));
        rx.take_frames();
        const auto& stats = rx.stats();
        if (stats.acquisitions != acquisitions || stats.frames_ok != frames || stats.crc_failures != crcs) {
            acquisitions = stats.acquisitions; frames = stats.frames_ok; crcs = stats.crc_failures;
            std::cout << "at=" << pos << " acquisitions=" << acquisitions << " frames=" << frames << " crc=" << crcs
                      << " offset_hz=" << stats.last_frequency_offset_hz << " buffered=" << stats.samples_buffered << '\n';
        }
    }
    text += rx.finish();
    std::cout << "text_hex=";
    for (unsigned char c : text) std::cout << std::hex << std::setw(2) << std::setfill('0') << unsigned(c);
    std::cout << std::dec << "\nframes=" << rx.stats().frames_ok << " crc=" << rx.stats().crc_failures << " gaps=" << rx.stats().sequence_gaps << '\n';
    return 0;
}
