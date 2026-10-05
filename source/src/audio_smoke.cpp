#include "fectty/winmm_io.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--cancel-test") {
        fectty::WinMmAudioIo audio;
        const std::string output = argc > 2 ? argv[2] : "winmm:1";
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
    const std::string input = argc > 1 ? argv[1] : "winmm:0";
    const std::string output = argc > 2 ? argv[2] : "winmm:1";
    const int seconds = argc > 3 ? std::max(1, std::atoi(argv[3])) : 15;

    fectty::WinMmAudioIo audio;
    if (!audio.open(input, output, 48000.0, {})) {
        std::cerr << "open failed: " << audio.last_error() << "\n";
        return 1;
    }
    size_t samples = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < deadline) {
        for (auto& chunk : audio.take_input_chunks()) samples += chunk.size();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    audio.close();
    for (auto& chunk : audio.take_input_chunks()) samples += chunk.size();
    std::cout << "samples=" << samples
              << " dropped=" << audio.dropped_samples() << "\n";
    return samples == 0 ? 2 : 0;
}
