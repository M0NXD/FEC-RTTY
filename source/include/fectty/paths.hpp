#pragma once
#include <filesystem>
#include <string_view>

namespace fectty {
inline std::filesystem::path utf8_file_path(std::string_view path) {
    return std::filesystem::path(std::u8string_view(
        reinterpret_cast<const char8_t*>(path.data()), path.size()));
}
} // namespace fectty
