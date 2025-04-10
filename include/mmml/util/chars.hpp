#ifndef MMML_CHARS_HPP
#define MMML_CHARS_HPP

#include "ulight/impl/chars.hpp"

namespace mmml {

using ulight::code_point_max;
using ulight::code_point_max_ascii;
using ulight::is_ascii;
using ulight::is_ascii_alpha;
using ulight::is_ascii_alphanumeric;
using ulight::is_ascii_digit;
using ulight::is_ascii_hex_digit;
using ulight::is_ascii_lower_alpha;
using ulight::is_ascii_upper_alpha;
using ulight::is_html_ascii_unquoted_attribute_value_character;
using ulight::is_html_attribute_name_character;
using ulight::is_html_min_raw_passthrough_character;
using ulight::is_html_tag_name_character;
using ulight::is_html_whitespace;
using ulight::is_mmml_argument_name_character;
using ulight::is_mmml_ascii_argument_name_character;
using ulight::is_mmml_ascii_directive_name_character;
using ulight::is_mmml_directive_name_character;
using ulight::is_mmml_escapeable;
using ulight::is_mmml_special_character;
using ulight::is_scalar_value;
using ulight::private_use_area_max;
using ulight::supplementary_pua_a_max;
using ulight::supplementary_pua_a_min;
using ulight::supplementary_pua_b_max;
using ulight::supplementary_pua_b_min;

/// @brief Returns true if `c` is a blank character.
/// This matches the C locale definition,
/// and includes vertical tabs,
/// unlike `is_ascii_whitespace`.
[[nodiscard]]
constexpr bool is_ascii_blank(char8_t c)
{
    return is_html_whitespace(c) || c == u8'\v';
}

/// @brief Returns true if `c` is a blank character.
/// This matches the C locale definition,
/// and includes vertical tabs,
/// unlike `is_ascii_whitespace`.
[[nodiscard]]
constexpr bool is_ascii_blank(char32_t c)
{
    return is_html_whitespace(c) || c == U'\v';
}

} // namespace mmml

#endif
