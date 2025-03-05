#ifndef MMML_STRINGS_HPP
#define MMML_STRINGS_HPP

#include <string_view>

#include "mmml/util/chars.hpp"

namespace mmml {

[[nodiscard]]
inline std::string_view as_string_view(std::u8string_view str)
{
    return { reinterpret_cast<const char*>(str.data()), str.size() };
}

/// @brief Returns `true` if `str` is a valid HTML tag identifier.
/// This includes both builtin tag names (which are purely alphabetic)
/// and custom tag names.
[[nodiscard]]
constexpr bool is_html_tag_name(std::u8string_view str)
{
    // https://html.spec.whatwg.org/dev/custom-elements.html#valid-custom-element-name
    if (str.empty() || !is_ascii_alphanumeric(str[0])) {
        return false;
    }
    // FIXME: this should be using the UTF-32 variant
    for (char8_t c : str) {
        if (!is_html_tag_name_character(c)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]]
constexpr bool is_html_attribute_name(std::u8string_view str)
{
    // https://html.spec.whatwg.org/dev/custom-elements.html#valid-custom-element-name
    if (str.empty()) {
        return false;
    }
    // FIXME: this should be using the UTF-32 variant
    for (char8_t c : str) {
        if (!is_html_attribute_name_character(c)) {
            return false;
        }
    }
    return true;
}

/// @brief Returns `true` if the given string requires wrapping in quotes when it
/// appears as the value in an attribute.
/// For example, `id=123` is a valid HTML attribute with a value and requires
/// no wrapping, but `id="<x>"` requires `<x>` to be surrounded by quotes.
[[nodiscard]]
constexpr bool requires_quotes_in_html_attribute(std::u8string_view value)
{
    return value.find_first_of(u8"\"/'`=<> ") != std::u8string_view::npos;
}

} // namespace mmml

#endif
