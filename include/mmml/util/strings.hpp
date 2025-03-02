#ifndef MMML_STRINGS_HPP
#define MMML_STRINGS_HPP

#include <string_view>

#include "mmml/util/chars.hpp"

namespace mmml {

/// @brief Returns `true` if `str` is a valid HTML tag identifier.
/// This includes both builtin tag names (which are purely alphabetic)
/// and custom tag names.
[[nodiscard]]
constexpr bool is_html_tag_name(std::string_view str)
{
    // https://html.spec.whatwg.org/dev/custom-elements.html#valid-custom-element-name
    if (str.empty() || !is_ascii_alphanumeric(char8_t(str[0]))) {
        return false;
    }
    for (char c : str) {
        if (!is_html_tag_name_character(char8_t(c))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]]
constexpr bool is_html_attribute_name(std::string_view str)
{
    // https://html.spec.whatwg.org/dev/custom-elements.html#valid-custom-element-name
    if (str.empty()) {
        return false;
    }
    for (char c : str) {
        if (!is_html_attribute_name_character(char8_t(c))) {
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
constexpr bool requires_quotes_in_html_attribute(std::string_view value)
{
    return value.find_first_of("\"/'`=<> ") != std::string_view::npos;
}

} // namespace mmml

#endif
