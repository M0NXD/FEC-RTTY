#include "fectty/arq.hpp"
#include "fectty/audio_io.hpp"
#include "fectty/bench_exit.hpp"
#include "fectty/acquisition.hpp"
#include "fectty/clock_drift.hpp"
#include "fectty/channel.hpp"
#include "fectty/timing_tracker.hpp"
#include "fectty/frame.hpp"
#include "fectty/interop.hpp"
#include "fectty/parse.hpp"
#include "fectty/paths.hpp"
#include "fectty/reliable_receiver.hpp"
#include "fectty/session.hpp"
#include "fectty/settings.hpp"
#include "fectty/wav.hpp"
#include <filesystem>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

namespace {
int failures = 0, checks = 0;
void check(bool ok, const char* label) {
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
void write_bytes(const std::filesystem::path& path, std::string_view bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}
std::string utf8_path(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}
} // namespace

int main() {
    check(fectty::gui_bench_exit_code(0, false, false) == 0, "normal receive-only or idle GUI closure is successful");
    check(fectty::gui_bench_exit_code(0, true, false) == 2, "requested send that never starts or is cancelled cannot report success");
    check(fectty::gui_bench_exit_code(0, true, true) == 0, "completed bench send returns success");
    check(fectty::gui_bench_exit_code(2, true, true) == 2, "explicit startup/report failure code is preserved");
    check(fectty::gui_bench_exit_code(3, false, false) == 3, "other event-loop failure codes are preserved");
    unsigned number = 42;
    double real = 0;
    check(!fectty::parse_number("1junk", number) && number == 42, "numeric trailing characters rejected");
    check(!fectty::parse_number("-1", number), "negative device ID rejected");
    check(!fectty::parse_number("nan", real) && !fectty::parse_number("inf", real), "nonfinite levels rejected");
    check(fectty::parse_number("0.04", real) && real == 0.04, "reduced output gain parsed");
    std::vector<fectty::AudioDeviceInfo> devices{{"portaudio:5","Speaker [MME]",false,true}, {"portaudio:6","CABLE Input [MME]",false,true}};
    check(fectty::resolve_saved_audio_device(devices, "portaudio:5", "CABLE Input [MME]", false) == "portaudio:6", "renumbered device follows remembered name rather than stale ID");
    check(fectty::resolve_saved_audio_device(devices, "portaudio:5", "", false).empty(), "legacy numeric device ID cannot silently select speakers");
    check(fectty::resolve_saved_audio_device(devices, "portaudio:6", "CABLE Input [MME]", true).empty(), "saved device must support the requested direction");
    devices.push_back({"portaudio:7","CABLE Input [MME]",false,true});
    check(fectty::resolve_saved_audio_device(devices, "portaudio:6", "CABLE Input [MME]", false).empty(), "ambiguous duplicate device names require operator choice");
    check(fectty::resolve_saved_audio_device(devices, "none", "", true) == "none", "intentional TX-only input survives device reordering");
    auto preamble = fectty::acquisition_waveform();
    check(!fectty::acquire(preamble, {}, 100, 0).locked, "invalid acquisition step cannot loop forever");
    check(fectty::compensate_clock_search(preamble, 80, 0).samples.empty(), "invalid clock search step rejected");
    check(fectty::resample_clock_error(preamble, std::numeric_limits<double>::quiet_NaN()).empty(), "nonfinite clock ratio rejected");
    std::fill(preamble.begin(), preamble.begin() + 23*960, 0.0f);
    check(!fectty::acquire(preamble).locked, "matching short tail cannot masquerade as full acquisition");

    const auto settings_file = fectty::utf8_file_path("audit-\xc3\xa9.ini");
    const auto settings_path = utf8_path(settings_file);
    fectty::AppSettings initial;
    initial.callsign = "ORIGINAL";
    initial.output_volume = 0.123456789123;
    initial.tx_center_hz = 1825.4;
    initial.link_offsets = false;
    initial.waterfall_floor_dbfs = -97;
    initial.audio_input_name = "CABLE Output [MME]";
    initial.audio_output_name = "CABLE Input [MME]";
    check(fectty::save_settings(initial, settings_path), "Unicode settings path save");
    fectty::AppSettings loaded;
    check(fectty::load_settings(loaded, settings_path) && loaded.callsign == initial.callsign &&
          loaded.output_volume == initial.output_volume, "settings full precision round trip");
    check(loaded.tx_center_hz == 1825.4 && !loaded.link_offsets && loaded.waterfall_floor_dbfs == -97,
          "independent TX offset, linkage and waterfall levels persist");
    check(loaded.audio_input_name == initial.audio_input_name && loaded.audio_output_name == initial.audio_output_name,
          "audio endpoint identity names persist");
    for (const std::string_view bad : {"rig_port=65536", "output_volume=nan", "center_hz=1500oops", "station_id=-1", "tx_center_hz=nan", "link_offsets=2", "waterfall_floor_dbfs=-111"}) {
        write_bytes(settings_file, "callsign=CHANGED\n" + std::string(bad) + "\n");
        check(!fectty::load_settings(loaded, settings_path) && loaded.callsign == "ORIGINAL", "invalid settings load is transactional");
    }
    write_bytes(settings_file, "callsign=VALID\r\noutput_volume=0.04\r\n");
    check(fectty::load_settings(loaded, settings_path) && loaded.callsign == "VALID", "CRLF settings accepted");
    write_bytes(settings_file, "center_hz=1625.5\n");
    check(fectty::load_settings(loaded, settings_path) && loaded.tx_center_hz == loaded.center_hz && loaded.center_hz == 1625.5,
          "old single-center settings migrate to matching RX/TX offsets");
    check(fectty::save_settings(initial, settings_path) && fectty::load_settings(loaded, settings_path), "settings safely replace existing file");
    std::filesystem::remove(settings_file);

    const auto wav_file = std::filesystem::path("audit.wav");
    const std::vector<float> samples{0, 0.25f, -0.25f, std::numeric_limits<float>::quiet_NaN()};
    check(fectty::write_wav_mono16(wav_file.string(), samples), "WAV with nonfinite sample sanitised");
    fectty::WavData wav;
    check(fectty::read_wav_mono16(wav_file.string(), wav) && wav.samples.size() == 4 && wav.samples.back() == 0, "WAV finite sample round trip");
    std::ifstream input(wav_file, std::ios::binary);
    std::string valid((std::istreambuf_iterator<char>(input)), {});
    input.close();
    for (const size_t size : {size_t{0}, size_t{11}, size_t{20}, size_t{43}, valid.size()-1}) {
        write_bytes(wav_file, std::string_view(valid).substr(0, size));
        check(!fectty::read_wav_mono16(wav_file.string(), wav) && wav.samples.size() == 4, "truncated WAV rejected without changing output");
    }
    auto corrupt = valid;
    corrupt[40] = corrupt[41] = corrupt[42] = corrupt[43] = char(0xff);
    write_bytes(wav_file, corrupt);
    check(!fectty::read_wav_mono16(wav_file.string(), wav), "oversized WAV chunk rejected before allocation");
    corrupt = valid; corrupt[16] = 1; corrupt[17] = corrupt[18] = corrupt[19] = 0;
    write_bytes(wav_file, corrupt);
    check(!fectty::read_wav_mono16(wav_file.string(), wav), "short WAV format chunk rejected");
    check(!fectty::write_wav_mono16(wav_file.string(), samples, 0), "invalid WAV sample rate rejected");
    check(fectty::write_wav_mono16(wav_file.string(), {}) && fectty::read_wav_mono16(wav_file.string(), wav) && wav.samples.empty(), "empty valid WAV supported");
    std::filesystem::remove(wav_file);

    const std::string text = " \n1234567\xc3\xa9\n\n\xf0\x9f\x93\xbb\n";
    fectty::ModemSession modem;
    const auto entire = modem.transmit(text);
    fectty::ModemTransmitStream stream(text);
    std::vector<float> streamed;
    while (true) {
        auto chunk = stream.next(777);
        if (chunk.empty()) break;
        streamed.insert(streamed.end(), chunk.begin(), chunk.end());
    }
    check(streamed == entire, "streamed TX matches original waveform sample for sample");
    fectty::ReliableReceiver rx;
    auto received = rx.push(streamed); received += rx.finish();
    check(received == text && rx.stats().crc_failures == 0, "UTF8 and trailing blank lines survive modem");
    fectty::ModemTransmitStream empty("");
    check(empty.next().empty(), "empty stream generates no audio");

    const std::string boundary_text = "B TO A \xc3\xa9\n\n";
    fectty::ModemSession boundary_tx;
    const auto boundary_wave = boundary_tx.transmit(boundary_text);
    fectty::ReliableReceiver sync_wait_rx;
    sync_wait_rx.push(std::span<const float>(boundary_wave).first(24000));
    check(sync_wait_rx.stats().acquisitions == 0, "live receiver waits for following sync before committing acquisition");
    auto sync_wait_text = sync_wait_rx.push(std::span<const float>(boundary_wave).subspan(24000)); sync_wait_text += sync_wait_rx.finish();
    check(sync_wait_text == boundary_text && sync_wait_rx.stats().acquisitions == 1,
          "complete sync commits one correct symbol grid");
    auto nearly_complete = boundary_tx.transmit("TIMING");
    nearly_complete.resize(nearly_complete.size() - 32);
    fectty::ReliableReceiver eof_rx;
    auto eof_text = eof_rx.push(nearly_complete); eof_text += eof_rx.finish();
    check(eof_text == "TIMING" && eof_rx.stats().frames_ok == 1,
          "EOF sub-symbol timing guard still requires a valid decoded frame");
    nearly_complete.resize(nearly_complete.size() - 3*960);
    fectty::ReliableReceiver truncated_rx;
    auto truncated_text = truncated_rx.push(nearly_complete); truncated_text += truncated_rx.finish();
    check(truncated_text.empty() && truncated_rx.stats().frames_ok == 0,
          "EOF guard does not invent several missing symbols");
    for (const size_t prefix : {size_t{1}, size_t{73}, size_t{503}, size_t{777}, size_t{1200}, size_t{1535}, size_t{1700}, size_t{2000}, size_t{2047}}) {
        std::vector<float> captured(prefix, 0.0f);
        captured.insert(captured.end(), boundary_wave.begin(), boundary_wave.end());
        captured.insert(captured.end(), 3000, 0.0f);
        fectty::ReliableReceiver boundary_rx;
        std::string decoded;
        for (size_t pos = 0; pos < captured.size(); pos += 2048) {
            decoded += boundary_rx.push(std::span<const float>(captured).subspan(pos, std::min<size_t>(2048, captured.size()-pos)));
        }
        decoded += boundary_rx.finish();
        check(decoded == boundary_text && boundary_rx.stats().frames_ok == 2 && boundary_rx.stats().crc_failures == 0 && boundary_rx.stats().acquisitions == 1,
              "arbitrary live callback start preserves short final UTF8 frame");
    }
    const std::vector<float> silence(48000 * 3, 0.0f);
    const auto noise = fectty::apply_channel(silence, {0.02, 1, 0, 0, 901});
    fectty::ReliableReceiver noise_rx;
    const auto noise_start = std::chrono::steady_clock::now();
    for (size_t pos = 0; pos < noise.size(); pos += 2048) {
        noise_rx.push(std::span<const float>(noise).subspan(pos, std::min<size_t>(2048, noise.size()-pos)));
    }
    noise_rx.finish();
    const auto noise_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-noise_start).count();
    check(noise_rx.stats().frames_ok == 0 && noise_rx.stats().acquisitions == 0, "ordinary noise is never displayed as text");
    std::cout << "3 seconds noisy audio processed in " << noise_ms << " ms\n";

    // A noise-only test or noise added only to the burst misses the important
    // live-radio case: a burst arriving after the decoder has searched noise.
    const std::string noise_message = "PREFIX TEST";
    fectty::ModemSession noise_tx;
    const auto noise_wave = noise_tx.transmit(noise_message);
    size_t largest_buffer = 0;
    for (const size_t prefix_size : {size_t{48000}, size_t{48503}}) {
        const auto prefix_noise = fectty::apply_channel(std::vector<float>(prefix_size, 0.0f), {0.02, 1, 0, 0, 901});
        for (const bool continuous_noise : {false, true}) {
            auto prefixed = prefix_noise;
            const auto burst = continuous_noise ? fectty::apply_channel(noise_wave, {0.02, 1, 0, 0, 902}) : noise_wave;
            prefixed.insert(prefixed.end(), burst.begin(), burst.end());
            prefixed.insert(prefixed.end(), 4800, 0.0f);
            for (const size_t block_size : {size_t{2048}, size_t{48000}, prefixed.size()}) {
                fectty::ReliableReceiver prefixed_rx;
                std::string decoded;
                for (size_t pos = 0; pos < prefixed.size(); pos += block_size) {
                    decoded += prefixed_rx.push(std::span<const float>(prefixed).subspan(pos, std::min(block_size, prefixed.size()-pos)));
                    largest_buffer = std::max(largest_buffer, prefixed_rx.stats().samples_buffered);
                }
                decoded += prefixed_rx.finish();
                check(decoded == noise_message && prefixed_rx.stats().frames_ok == 2 &&
                      prefixed_rx.stats().acquisitions == 1 && prefixed_rx.stats().crc_failures == 0 &&
                      prefixed_rx.stats().sequence_gaps == 0,
                      "noise before a clean/noisy burst survives arbitrary alignment and caller block sizes");
            }
        }
    }
    check(largest_buffer <= 5*48000, "large prefixed inputs do not retain the entire recording");

    fectty::ModemSession first_sender, second_sender;
    auto separated = first_sender.transmit("FIRST");
    const auto noise_gap = fectty::apply_channel(std::vector<float>(48000, 0.0f), {0.02, 1, 0, 0, 903});
    separated.insert(separated.end(), noise_gap.begin(), noise_gap.end());
    const auto second_burst = second_sender.transmit("SECOND");
    separated.insert(separated.end(), second_burst.begin(), second_burst.end());
    separated.insert(separated.end(), 4800, 0.0f);
    fectty::ReliableReceiver separated_rx;
    auto separated_text = separated_rx.push(separated); separated_text += separated_rx.finish();
    check(separated_text == "FIRSTSECOND" && separated_rx.stats().frames_ok == 2 &&
          separated_rx.stats().acquisitions == 2 && separated_rx.stats().crc_failures == 0 &&
          separated_rx.stats().sequence_gaps == 0, "new preamble resets per-burst sequence even without a silent gap");

    fectty::Frame control; control.type = fectty::FrameType::Control; control.payload = {4, 2, 1};
    fectty::Frame addressed; addressed.flags = fectty::kArqAddressedFlag; addressed.payload = {2,1,0,0,1,'X'};
    fectty::Frame future; future.version = 1; future.payload = {'Y'};
    fectty::Frame plain; plain.payload = {'O','K','\n'};
    std::vector<float> mixed;
    for (auto frame : {control, addressed, future, plain}) {
        const auto wave = modem.transmit_frame(frame, mixed.empty());
        mixed.insert(mixed.end(), wave.begin(), wave.end());
    }
    fectty::ReliableReceiver mixed_rx;
    auto visible = mixed_rx.push(mixed); visible += mixed_rx.finish();
    check(visible == "OK\n" && mixed_rx.take_frames().size() == 4, "only compatible unaddressed Text is displayed; control frames remain inspectable");
    fectty::ModemSession known_rx;
    check(known_rx.receive_known_frames(mixed, {3,6,1,3}) == "OK\n", "known-frame text API also filters envelopes");

    fectty::ModemSession damaged_tx;
    auto damaged = damaged_tx.transmit("12345678");
    std::fill(damaged.begin() + (25 + 16) * 960, damaged.end(), 0.2f);
    fectty::ReliableReceiver damaged_rx;
    damaged_rx.push(damaged); damaged_rx.finish();
    check(damaged_rx.stats().frames_ok == 0 && damaged_rx.stats().crc_failures == 1,
          "damaged final frame reports CRC failure exactly once");
    fectty::ModemSession recovery_tx;
    const auto recovery = recovery_tx.transmit("RECOVER");
    damaged.insert(damaged.end(), recovery.begin(), recovery.end());
    fectty::ReliableReceiver recovery_rx;
    auto recovered = recovery_rx.push(damaged); recovered += recovery_rx.finish();
    check(recovered == "RECOVER" && recovery_rx.stats().crc_failures == 1 && recovery_rx.stats().sequence_gaps == 0,
          "fresh adjacent burst recovers after damaged last frame");

    check(fectty::simulate_two_station("OFFSET", 0.01, 25, 0, 42).ok, "physical frequency offset decodes");
    check(fectty::simulate_two_station("OFFSET", 0.01, 0, 25, 42).ok, "physical clock offset decodes");
    check(fectty::simulate_two_station("OFFSET", 0.01, 25, 25, 42).ok, "combined small frequency and clock offsets decode");
    check(!fectty::simulate_two_station("OFFSET", 0.01, 1000, 0, 42).ok, "out-of-range frequency impairment is actually applied");
    std::cout << checks << " audit checks; failures=" << failures << '\n';
    return failures ? 1 : 0;
}
