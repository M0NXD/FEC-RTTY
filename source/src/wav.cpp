#include "fectty/wav.hpp"
#include "fectty/paths.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string_view>

namespace fectty {
namespace {
void u16(std::ostream& out, uint16_t value) {
    const char bytes[2] = {char(value), char(value >> 8)};
    out.write(bytes, 2);
}
void u32(std::ostream& out, uint32_t value) {
    const char bytes[4] = {char(value), char(value >> 8), char(value >> 16), char(value >> 24)};
    out.write(bytes, 4);
}
uint16_t r16(std::istream& in) {
    unsigned char bytes[2]{};
    in.read(reinterpret_cast<char*>(bytes), 2);
    return uint16_t(bytes[0] | (uint16_t(bytes[1]) << 8));
}
uint32_t r32(std::istream& in) {
    unsigned char bytes[4]{};
    in.read(reinterpret_cast<char*>(bytes), 4);
    return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) |
           (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
}
} // namespace

bool write_wav_mono16(const std::string& path, std::span<const float> samples, uint32_t rate) {
    if (!rate || rate > std::numeric_limits<uint32_t>::max() / 2 ||
        samples.size() > (std::numeric_limits<uint32_t>::max() - 36ull) / 2) return false;
    std::ofstream out(utf8_file_path(path), std::ios::binary);
    if (!out) return false;
    const auto data_bytes = static_cast<uint32_t>(samples.size() * 2);
    out.write("RIFF", 4); u32(out, 36 + data_bytes); out.write("WAVEfmt ", 8);
    u32(out, 16); u16(out, 1); u16(out, 1); u32(out, rate); u32(out, rate * 2);
    u16(out, 2); u16(out, 16); out.write("data", 4); u32(out, data_bytes);
    for (const float sample : samples) {
        const auto value = std::isfinite(sample) ? std::clamp(sample, -1.0f, 1.0f) : 0.0f;
        u16(out, uint16_t(int16_t(std::lrint(value * 32767.0f))));
    }
    out.close();
    return !out.fail();
}

bool read_wav_mono16(const std::string& path, WavData& result) {
    std::ifstream in(utf8_file_path(path), std::ios::binary | std::ios::ate);
    if (!in) return false;
    const auto file_size = in.tellg();
    in.seekg(0);
    char id[4]{};
    if (!in.read(id, 4) || std::string_view(id, 4) != "RIFF") return false;
    const uint64_t end = uint64_t(r32(in)) + 8;
    if (file_size < 0 || end < 12 || end > static_cast<uint64_t>(file_size)) return false;
    if (!in.read(id, 4) || std::string_view(id, 4) != "WAVE") return false;
    bool have_format = false, have_data = false;
    uint32_t rate = 0;
    std::vector<char> data;
    while (static_cast<uint64_t>(in.tellg()) < end) {
        const auto start = static_cast<uint64_t>(in.tellg());
        if (end - start < 8 || !in.read(id, 4)) return false;
        const uint32_t size = r32(in);
        const uint64_t padded_size = uint64_t(size) + (size & 1u);
        if (!in || padded_size > end - start - 8) return false;
        const std::string_view name(id, 4);
        if (name == "fmt ") {
            if (have_format || size < 16) return false;
            const auto format = r16(in), channels = r16(in);
            rate = r32(in);
            const auto byte_rate = r32(in);
            const auto align = r16(in), bits = r16(in);
            if (!in || format != 1 || channels != 1 || bits != 16 || align != 2 ||
                !rate || uint64_t(rate) * 2 != byte_rate) return false;
            have_format = true;
        } else if (name == "data") {
            if (have_data || (size & 1u)) return false;
            data.resize(size);
            if (!in.read(data.data(), size)) return false;
            have_data = true;
        }
        in.seekg(static_cast<std::streamoff>(start + 8 + padded_size));
        if (!in) return false;
    }
    if (!have_format || !have_data) return false;
    WavData decoded;
    decoded.sample_rate = rate;
    decoded.samples.resize(data.size() / 2);
    for (size_t k = 0; k < decoded.samples.size(); ++k) {
        const auto value = uint16_t(uint8_t(data[2*k]) | (uint16_t(uint8_t(data[2*k+1])) << 8));
        decoded.samples[k] = float(int16_t(value)) / 32768.0f;
    }
    result = std::move(decoded);
    return true;
}
} // namespace fectty
