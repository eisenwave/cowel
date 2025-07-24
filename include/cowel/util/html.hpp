#ifndef COWEL_HTML_HPP
#define COWEL_HTML_HPP

#include <concepts>
#include <cstddef>
#include <string_view>

#include "ulight/impl/ascii_algorithm.hpp"

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/html_entities.hpp"

#include "cowel/settings.hpp"

namespace cowel {

template <typename F>
concept string_or_char_consumer = requires(F& f, std::u8string_view s, char8_t c) {
    f(s);
    f(c);
};

template <string_or_char_consumer Out, std::invocable<char8_t> Predicate>
void append_html_escaped(Out& out, std::u8string_view text, Predicate p)
{
    while (!text.empty()) {
        const std::size_t safe_length = ulight::ascii::length_if_not(text, p);
        if (safe_length != 0) {
            out(text.substr(0, safe_length));
            text.remove_prefix(safe_length);
            if (text.empty()) {
                break;
            }
        }
        out(html_entity_of(text.front()));
        text.remove_prefix(1);
    }
}

template <string_or_char_consumer Out, std::invocable<char8_t> Predicate>
void append_html_escaped(Out& out, Char_Sequence8 text, Predicate p)
{
    if (text.is_contiguous()) {
        append_html_escaped(out, text.as_string_view(), std::move(p));
        return;
    }
    char8_t buffer[default_char_sequence_buffer_size];
    while (!text.empty()) {
        const std::size_t n = text.extract(buffer);
        append_html_escaped(out, std::u8string_view { buffer, n }, p);
    }
}

namespace detail {

struct Charset_Contains_Predicate {
    std::u8string_view charset;

    constexpr bool operator()(char8_t c) const noexcept
    {
        return charset.contains(c);
    }
};

} // namespace detail

/// @brief Appends text to the vector where code units in `charset`
/// are replaced with their corresponding HTML entities.
/// For example, if `charset` includes `&`, `&amp;` is appended in its stead.
///
/// `charset` shall be a subset of the entities supported by `html_entity_of`.
template <string_or_char_consumer Out>
void append_html_escaped_of(Out& out, std::u8string_view text, std::u8string_view charset)
{
    append_html_escaped(out, text, detail::Charset_Contains_Predicate { charset });
}

template <string_or_char_consumer Out>
void append_html_escaped_of(Out& out, Char_Sequence8 text, std::u8string_view charset)
{
    append_html_escaped(out, text, detail::Charset_Contains_Predicate { charset });
}

} // namespace cowel

#endif
