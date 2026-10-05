#pragma once

#include <charconv>
#include <cmath>
#include <string_view>
#include <type_traits>

namespace fectty {
template<class T>
bool parse_number(std::string_view text, T& result) {
    T value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) return false;
    if constexpr (std::is_floating_point_v<T>) {
        if (!std::isfinite(value)) return false;
    }
    result = value;
    return true;
}
} // namespace fectty
