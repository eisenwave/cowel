#ifndef COWEL_TO_CHARS_HPP
#define COWEL_TO_CHARS_HPP

#include <array>
#include <bit>
#include <charconv>
#include <cstddef>
#include <limits>
#include <span>
#include <string_view>
#include <system_error>
#include <type_traits>

#include "cowel/util/assert.hpp"
#include "cowel/util/chars.hpp"
#include "cowel/util/meta.hpp"

#include "cowel/fwd.hpp"

namespace cowel {

template <typename T>
concept digit_sequence = requires {
    std::numeric_limits<T>::digits;
    std::is_signed_v<T>;
};

template <typename T>
concept to_chars_able = requires(char* p, T x) { std::to_chars(p, p, x); };

template <typename T>
concept character_convertible = digit_sequence<T> && to_chars_able<T>;

template <typename Char, std::size_t capacity>
struct Basic_Characters {
    std::array<Char, capacity> buffer;
    std::size_t length;

    [[nodiscard]]
    constexpr std::span<Char> as_span()
    {
        return { buffer.data(), length };
    }

    [[nodiscard]]
    constexpr std::span<const Char> as_span() const
    {
        return { buffer.data(), length };
    }

    [[nodiscard]]
    constexpr std::basic_string_view<Char> as_string() const
    {
        return { buffer.data(), length };
    }

    [[nodiscard]]
    constexpr operator std::basic_string_view<Char>() const
    {
        return as_string();
    }
};

template <char_like Char = char, character_convertible T>
[[nodiscard]]
constexpr Basic_Characters<Char, std::numeric_limits<T>::digits + 1>
to_characters(const T& x, int base = 10, bool to_upper = false)
{
    COWEL_ASSERT(base >= 2 && base <= 36);

    decltype(to_characters<char>(x, base)) chars {};
    auto* const buffer_start = chars.buffer.data();
    const auto result = std::to_chars(buffer_start, buffer_start + chars.buffer.size(), x, base);
    COWEL_ASSERT(result.ec == std::errc {});
    chars.length = std::size_t(result.ptr - buffer_start);
    if (to_upper) {
        for (char& c : chars.as_span()) {
            c = char(to_ascii_upper(char8_t(c)));
        }
    }

    if constexpr (std::is_same_v<Char, char>) {
        return chars;
    }
    else {
        using result_type = Basic_Characters<Char, std::numeric_limits<T>::digits + 1>;
        return std::bit_cast<result_type>(chars);
    }
}

template <character_convertible T>
[[nodiscard]]
constexpr Characters8<std::numeric_limits<T>::digits + 1>
to_characters8(const T& x, int base = 10, bool to_upper = false)
{
    return to_characters<char8_t>(x, base, to_upper);
}

} // namespace cowel

#endif
