#ifndef COWEL_STRINGIFY_HPP
#define COWEL_STRINGIFY_HPP

#include <concepts>
#include <string>
#include <string_view>

#include "cowel/util/to_chars.hpp"

namespace cowel {

template <typename T>
struct Stringify;

template <std::convertible_to<std::u8string_view> T>
struct Stringify<T> {
    void append(std::u8string& out, const T& value) const
    {
        out += std::u8string_view(value);
    }
    void append(std::pmr::u8string& out, const T& value) const
    {
        out += std::u8string_view(value);
    }
};

template <typename T>
    requires(std::is_arithmetic_v<T>)
struct Stringify<T> {
    static_assert(!std::is_const_v<T> && !std::is_volatile_v<T>);
    void append(std::u8string& out, const T& value) const
    {
        out += std::u8string_view(to_characters8(value));
    }
    void append(std::pmr::u8string& out, const T& value) const
    {
        out += std::u8string_view(to_characters8(value));
    }
};

template <typename T>
concept formattable = requires { Stringify<T> {}; };

} // namespace cowel

#endif
