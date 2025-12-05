#ifndef COWEL_CHARS_HPP
#define COWEL_CHARS_HPP

#include "ulight/impl/ascii_chars.hpp"
#include "ulight/impl/lang/cowel_chars.hpp"
#include "ulight/impl/lang/html_chars.hpp"
#include "ulight/impl/unicode_chars.hpp"

namespace cowel {

using ulight::code_point_max;
using ulight::code_point_max_ascii;
using ulight::is_ascii;
using ulight::is_ascii_alpha;
using ulight::is_ascii_alphanumeric;
using ulight::is_ascii_digit;
using ulight::is_ascii_hex_digit;
using ulight::is_ascii_lower_alpha;
using ulight::is_ascii_upper_alpha;
using ulight::is_cowel_allowed_after_backslash;
using ulight::is_cowel_argument_name;
using ulight::is_cowel_ascii_argument_name;
using ulight::is_cowel_ascii_directive_name;
using ulight::is_cowel_directive_name;
using ulight::is_cowel_directive_name_start;
using ulight::is_cowel_escapeable;
using ulight::is_cowel_special;
using ulight::is_cowel_unquoted_string;
using ulight::is_html_ascii_unquoted_attribute_value_character;
using ulight::is_html_attribute_name_character;
using ulight::is_html_min_raw_passthrough_character;
using ulight::is_html_tag_name_character;
using ulight::is_html_whitespace;
using ulight::is_scalar_value;
using ulight::private_use_area_max;
using ulight::supplementary_pua_a_max;
using ulight::supplementary_pua_a_min;
using ulight::supplementary_pua_b_max;
using ulight::supplementary_pua_b_min;
using ulight::to_ascii_lower;
using ulight::to_ascii_upper;

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

} // namespace cowel

#endif
