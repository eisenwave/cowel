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
bool is_html_tag_name(std::u8string_view str);

/// @brief Returns `true` if `str` is a valid HTML attribute name.
[[nodiscard]]
bool is_html_attribute_name(std::u8string_view str);

/// @brief Returns `true` if the given string requires no wrapping in quotes when it
/// appears as the value in an attribute.
/// For example, `id=123` is a valid HTML attribute with a value and requires
/// no wrapping, but `id="<x>"` requires `<x>` to be surrounded by quotes.
[[nodiscard]]
bool is_html_unquoted_attribute_value(std::u8string_view value);

} // namespace mmml

#endif
