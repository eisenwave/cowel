#ifndef COWEL_STRING_OR_CHAR_CONSUMER_HPP
#define COWEL_STRING_OR_CHAR_CONSUMER_HPP

#include <string_view>

namespace cowel {

/// @brief Satisfied if the call operator of `F` accepts both `std::u8string_view` and `char8_t`.
template <typename F>
concept string_or_char_consumer = requires(F& f, const std::u8string_view s, const char8_t c) {
    f(s);
    f(c);
};

/// @brief Adapts a string-like container to satisfy `string_or_char_consumer`.
/// Calling the object with a `std::u8string_view` appends it to `str`;
/// calling it with a `char8_t` pushes the character.
template <typename S>
struct U8String_Consumer {
    S& str;

    void operator()(const std::u8string_view s) const
    {
        str.append(s);
    }

    void operator()(const char8_t c) const
    {
        str.push_back(c);
    }
};

template <typename S>
U8String_Consumer(S&) -> U8String_Consumer<S>;

} // namespace cowel

#endif
