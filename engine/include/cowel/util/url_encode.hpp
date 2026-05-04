#ifndef COWEL_URL_ENCODE_HPP
#define COWEL_URL_ENCODE_HPP

#include <cstddef>
#include <iterator>
#include <string_view>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/chars.hpp"
#include "cowel/util/html.hpp"

namespace cowel {

/// @brief Returns `true` if `c` is a "reserved character" in a URL.
/// That is, a character which may have special meaning within a URL.
///
/// Note that this does not include control characters and many other characters which
/// have to be percent-encoded in all circumstances.
inline constexpr auto is_url_reserved = Charset256(u8"!#$&'()*+,/:;=?@[]");

/// @brief Returns `true` if `c` is an "unreserved character" in a URL.
/// That is, a character which does not need to be percent-encoded.
inline constexpr auto is_url_unreserved = is_ascii_alphanumeric | Charset256(u8"-_~");

/// @brief Returns `true` if `c` is a character that is always percent-encoded in URLs.
/// That is, control characters, double quotes, whitespace, etc.
inline constexpr auto is_url_always_encoded = ~(is_url_unreserved | is_url_reserved);

namespace detail {

[[nodiscard]]
constexpr char8_t to_ascii_digit(int value)
{
    return value < 10 ? char8_t(int(u8'0') + value) //
                      : char8_t(int(u8'a') + (value - 10));
}

} // namespace detail

/// @brief URL-encodes a given string `str`.
/// No Unicode-decode of `str` is performed;
/// instead, non-ASCII code units are all written directly, with no special encoding.
/// For any ASCII code units `c` where `filter(c)` yields `true`,
/// code units are percent-encoded.
/// @param out The output iterator, accepting `char8_t`.
/// @param str The string to encode.
/// @param filter Returns `true` for code units that should be URL-encoded.
template <std::output_iterator<char8_t> Output, typename F>
    requires std::is_invocable_r_v<bool, F, char8_t>
void url_encode_ascii_if(Output out, std::u8string_view str, F filter)
{
    for (char8_t c : str) {
        if (!is_ascii(c) || !filter(c)) {
            *out++ = c;
            continue;
        }
        *out++ = u8'%';
        *out++ = detail::to_ascii_digit((c >> 4) & 0xf);
        *out++ = detail::to_ascii_digit((c >> 0) & 0xf);
    }
}

template <string_or_char_consumer Output, typename F>
    requires std::is_invocable_r_v<bool, F, char8_t>
void url_encode_ascii_if(Output out, std::u8string_view str, F filter)
{
    for (char8_t c : str) {
        if (!is_ascii(c) || !filter(c)) {
            out(c);
            continue;
        }
        out(u8'%');
        out(detail::to_ascii_digit((c >> 4) & 0xf));
        out(detail::to_ascii_digit((c >> 0) & 0xf));
    }
}

template <string_or_char_consumer Output, typename F>
    requires std::is_invocable_r_v<bool, F, char8_t>
void url_encode_ascii_if(Output out, Char_Sequence8 str, F filter)
{
    if (str.empty()) {
        return;
    }
    if (const std::u8string_view sv = str.as_string_view(); !sv.empty()) {
        url_encode_ascii_if(out, sv, std::move(filter));
        return;
    }
    char8_t buffer[default_char_sequence_buffer_size];
    while (!str.empty()) {
        const std::size_t n = str.extract(buffer);
        url_encode_ascii_if(out, std::u8string_view { buffer, n }, filter);
    }
}

} // namespace cowel

#endif
