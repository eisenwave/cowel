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

template <std::size_t capacity>
struct Characters {
    std::array<char, capacity> buffer;
    std::size_t length;

    [[nodiscard]]
    std::string_view as_string() const
    {
        return { buffer.data(), length };
    }
};

template <typename T>
constexpr int approximate_to_chars_decimal_digits_v
    = (std::numeric_limits<T>::digits * 100 / 310) + 1 + std::is_signed_v<T>;

template <character_convertible T>
[[nodiscard]]
constexpr Characters<approximate_to_chars_decimal_digits_v<T>> to_characters(T x)
{
    Characters<approximate_to_chars_decimal_digits_v<T>> chars {};
    auto result = std::to_chars(chars.buffer.data(), chars.buffer.data() + chars.buffer.size(), x);
    MMML_ASSERT(result.ec == std::errc {});
    chars.length = std::size_t(result.ptr - chars.buffer.data());
    return chars;
}

} // namespace mmml

#endif
