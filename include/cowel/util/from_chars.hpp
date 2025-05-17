#ifndef COWEL_FROM_CHARS_HPP
#define COWEL_FROM_CHARS_HPP

#include <charconv>
#include <concepts>
#include <optional>
#include <string_view>
#include <system_error>

#include "cowel/util/strings.hpp"

namespace cowel {

template <typename T>
concept character_extractible = requires(const char* p, T& out) { std::from_chars(p, p, out); };

template <character_extractible T>
[[nodiscard]]
constexpr std::from_chars_result from_chars(std::string_view sv, T& out, int base = 10)
{
    return std::from_chars(sv.data(), sv.data() + sv.size(), out, base);
}

template <character_extractible T>
    requires std::default_initializable<T>
[[nodiscard]]
constexpr std::optional<T> from_chars(std::string_view sv, int base = 10)
{
    std::optional<T> result;
    if (auto r = std::from_chars(sv.data(), sv.data() + sv.size(), result.emplace(), base);
        r.ec != std::errc {}) {
        result.reset();
    }
    return result;
}

template <character_extractible T>
[[nodiscard]]
inline std::from_chars_result from_chars(std::u8string_view sv, T& out, int base = 10)
{
    return from_chars(as_string_view(sv), out, base);
}

template <character_extractible T>
    requires std::default_initializable<T>
[[nodiscard]]
constexpr std::optional<T> from_chars(std::u8string_view sv, int base = 10)
{
    return from_chars<T>(as_string_view(sv), base);
}

} // namespace cowel

#endif
