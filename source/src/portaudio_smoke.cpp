#include "fectty/session.hpp"
#include "fectty/portaudio_io.hpp"
#include "fectty/reliable_receiver.hpp"

#ifdef FECTTY_HAS_PORTAUDIO

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <thread>

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--cancel-test") {
        fectty::PortAudioIo audio;
        const std::string output = argc > 2 ? argv[2] : "portaudio:5";
        if (!audio.open("none", output, 48000, {})) {
            std::cerr << audio.last_error() << '\n'; return 1;
        }
        std::atomic<bool> producing{false};
        bool success = true;
        std::thread writer([&] {
            success = audio.write_stream([&] { producing = true; return std::vector<float>(2048, 0.0f); });
        });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!producing.load() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(5));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        const auto start = std::chrono::steady_clock::now();
        audio.cancel_write();
        writer.join();
        const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        const auto error = audio.last_error();
        const bool reopened = audio.open("none", output, 48000, {}) && audio.write(std::vector<float>(2048, 0.0f));
        std::cout << "cancel_ms=" << milliseconds << " tx_success=" << success << " error=" << error << " reopened=" << reopened << '\n';
        return producing.load() && !success && milliseconds < 1500 && error == "Transmission cancelled" && reopened ? 0 : 2;
    }
    if (argc > 1 && std::string(argv[1]) == "--list") {
        for (const auto& device : fectty::PortAudioIo::list_devices()) {
            std::cout << (device.input ? "IN " : "   ")
                      << (device.output ? "OUT " : "    ")
                      << device.id << ": " << device.name << "\n";
        }
        return 0;
    }

    const std::string input = argc > 1 ? argv[1] : "none";
    const std::string output = argc > 2 ? argv[2] : "";
    const int seconds = argc > 3 ? std::max(1, std::atoi(argv[3])) : 3;

    fectty::PortAudioIo audio;
    if (!audio.open(input, output, 48000.0, {})) {
        std::cerr << "open failed: " << audio.last_error() << "\n";
        return 1;
    }
    const auto devices_while_open = fectty::PortAudioIo::list_devices();

    fectty::ModemSession session;
    constexpr std::string_view message = "PORTAUDIO SMOKE";
    auto samples = session.transmit(std::string(message));
    for (auto& sample : samples) sample = static_cast<float>(sample * 0.15);
    const bool transmitted = audio.write(samples);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    size_t captured = 0;
    fectty::ReliableReceiver receiver;
    std::string decoded;
    while (std::chrono::steady_clock::now() < deadline) {
        for (auto& chunk : audio.take_input_chunks()) {
            captured += chunk.size();
            decoded += receiver.push(std::span<const float>(chunk));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    for (auto& chunk : audio.take_input_chunks()) {
        captured += chunk.size();
        decoded += receiver.push(std::span<const float>(chunk));
    }
    decoded += receiver.finish();
    const auto dropped = audio.dropped_samples();
    audio.close();

    std::cout << "transmitted=" << (transmitted ? "yes" : "no")
              << " devices=" << devices_while_open.size()
              << " captured=" << captured << " dropped=" << dropped
              << " decoded=\"" << decoded << "\"\n";
    const bool decoded_ok = input == "none" || decoded == message;
    return transmitted && dropped == 0 && decoded_ok ? 0 : 2;
}

#else
int main() { return 1; }
#endif
