#ifndef MMML_TO_CHARS_HPP
#define MMML_TO_CHARS_HPP

#include <array>
#include <charconv>
#include <limits>
#include <type_traits>

#include "mmml/util/assert.hpp"

namespace mmml {

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
    std::basic_string_view<Char> as_string() const
    {
        return { buffer.data(), length };
    }
};

template <typename T>
constexpr int approximate_to_chars_decimal_digits_v
    = (std::numeric_limits<T>::digits * 100 / 310) + 1 + std::is_signed_v<T>;

template <typename Char = char, character_convertible T>
[[nodiscard]]
constexpr Basic_Characters<Char, approximate_to_chars_decimal_digits_v<T>> to_characters(const T& x)
{
    Basic_Characters<Char, approximate_to_chars_decimal_digits_v<T>> chars {};
    auto result = std::to_chars(chars.buffer.data(), chars.buffer.data() + chars.buffer.size(), x);
    MMML_ASSERT(result.ec == std::errc {});
    chars.length = std::size_t(result.ptr - chars.buffer.data());
    return chars;
}

template <character_convertible T>
[[nodiscard]]
constexpr Characters8<approximate_to_chars_decimal_digits_v<T>> to_characters8(const T& x)
{
    return to_characters<char8_t>(x);
}

} // namespace mmml

#endif
