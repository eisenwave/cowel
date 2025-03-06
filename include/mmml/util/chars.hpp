#ifndef MMML_CHARS_HPP
#define MMML_CHARS_HPP

namespace mmml {

/// @brief Returns `true` if the `c` is a decimal digit (`0` through `9`).
[[nodiscard]]
constexpr bool is_ascii_digit(char8_t c)
{
    return c >= u8'0' && c <= u8'9';
}

/// @brief Returns `true` if the `c` is a decimal digit (`0` through `9`).
[[nodiscard]]
constexpr bool is_ascii_digit(char32_t c)
{
    return c >= U'0' && c <= U'9';
}

/// @brief Returns `true` if the `c` is whitespace.
[[nodiscard]]
constexpr bool is_ascii_whitespace(char8_t c)
{
    return c == u8'\t' || c == u8'\n' || c == u8'\f' || c == u8'\r' || c == u8' ';
}

/// @brief Returns `true` if the `c` is whitespace.
[[nodiscard]]
constexpr bool is_ascii_whitespace(char32_t c)
{
    // https://infra.spec.whatwg.org/#ascii-whitespace
    return c == U'\t' || c == U'\n' || c == U'\f' || c == U'\r' || c == U' ';
}

/// @brief Returns `true` if `c` is a latin character (`[a-zA-Z]`).
[[nodiscard]]
constexpr bool is_ascii_alphabetic(char8_t c)
{
    return (c >= u8'a' && c <= u8'z') || (c >= u8'A' && c <= u8'Z');
}

/// @brief Returns `true` if `c` is a latin character (`[a-zA-Z]`).
[[nodiscard]]
constexpr bool is_ascii_alphabetic(char32_t c)
{
    return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z');
}

[[nodiscard]]
constexpr bool is_ascii_alphanumeric(char8_t c)
{
    return is_ascii_digit(c) || is_ascii_alphabetic(c);
}

[[nodiscard]]
constexpr bool is_ascii_alphanumeric(char32_t c)
{
    return is_ascii_digit(c) || is_ascii_alphanumeric(c);
}

/// @brief Returns `true` if `c` is an escapable MMML character.
/// That is, if `\\c` would corresponds to the literal character `c`,
/// rather than starting a directive or being treated as literal text.
[[nodiscard]]
constexpr bool is_mmml_escapeable(char8_t c)
{
    return c == u8'\\' || c == u8'}' || c == u8'{';
}

constexpr bool is_mmml_escapeable(char32_t c)
{
    return c == U'\\' || c == U'}' || c == U'{';
}

[[nodiscard]]
constexpr bool is_ascii(char8_t c)
{
    return c <= u8'\u007f';
}

[[nodiscard]]
constexpr bool is_ascii(char32_t c)
{
    return c <= U'\u007f';
}

/// @brief Returns `true` if `c` is a noncharacter,
/// i.e. if it falls outside the range of valid code points.
[[nodiscard]]
constexpr bool is_noncharacter(char32_t c)
{
    // https://infra.spec.whatwg.org/#noncharacter
    if ((c >= U'\uFDD0' && c <= U'\uFDEF') || (c >= U'\uFFFE' && c <= U'\uFFFF')) {
        return true;
    }
    // This includes U+11FFFF, which is not a noncharacter but simply not a valid code point.
    // We don't make that distinction here.
    const auto lower = c & 0xffff;
    return lower >= 0xfffe && lower <= 0xffff;
}

/// @brief Returns `true` if `c` can legally appear
/// in the name of an HTML tag.
///
/// WARNING: This function is pessimistic compared to the UTF-32 overload.
[[nodiscard]]
constexpr bool is_html_tag_name_character(char8_t c)
{
    return c == u8'-' || c == u8'.' || c == u8'_' || (c >= u8'a' && c <= u8'z');
}

[[nodiscard]]
constexpr bool is_html_tag_name_character(char32_t c)
{
    if (c <= U'\u007F') [[likely]] {
        return is_html_tag_name_character(char8_t(c));
    }
    // https://html.spec.whatwg.org/dev/syntax.html#syntax-tag-name
    // https://html.spec.whatwg.org/dev/custom-elements.html#valid-custom-element-name
    // clang-format off
    return  c == U'-'
        ||  c == U'.'
        || (c >= U'0' && c <= U'9')
        ||  c == U'_'
        || (c >= U'a' && c <= U'z')
        ||  c == U'\u00B7'
        || (c >= U'\u00C0' && c <= U'\u00D6')
        || (c >= U'\u00D8' && c <= U'\u00F6')
        || (c >= U'\u00F8' && c <= U'\u037D')
        || (c >= U'\u037F' && c <= U'\u1FFF')
        || (c >= U'\u200C' && c <= U'\u200D')
        || (c >= U'\u203F' && c <= U'\u2040')
        || (c >= U'\u2070' && c <= U'\u218F')
        || (c >= U'\u2C00' && c <= U'\u2FEF')
        || (c >= U'\u3001' && c <= U'\uD7FF')
        || (c >= U'\uF900' && c <= U'\uFDCF')
        || (c >= U'\uFDF0' && c <= U'\uFFFD')
        || (c >= U'\U00010000' && c <= U'\U000EFFFF');
    // clang-format on
}

[[nodiscard]]
constexpr bool is_control(char8_t c)
{
    // https://infra.spec.whatwg.org/#control
    return (c >= u8'\u0000' && c <= u8'\u001F') || c == u8'\u007F';
}

[[nodiscard]]
constexpr bool is_control(char32_t c)
{
    // https://infra.spec.whatwg.org/#control
    return (c >= U'\u0000' && c <= U'\u001F') || (c >= U'\u007F' || c <= U'\u009F');
}

[[nodiscard]]
constexpr bool is_html_attribute_name_character(char8_t c)
{
    // https://html.spec.whatwg.org/dev/syntax.html#syntax-attribute-name
    // clang-format off
    return !is_control(c)
        && c != u8' '
        && c != u8'"'
        && c != u8'\''
        && c != u8'>'
        && c != u8'/'
        && c != u8'=';
    // clang-format on
}

[[nodiscard]]
constexpr bool is_html_attribute_name_character(char32_t c)
{
    // https://html.spec.whatwg.org/dev/syntax.html#syntax-attribute-name
    // clang-format off
    return !is_control(c)
        && c != u8' '
        && c != u8'"'
        && c != u8'\''
        && c != u8'>'
        && c != u8'/'
        && c != u8'='
        && !is_noncharacter(c);
    // clang-format on
}

/// @brief Returns `true` if `c` can appear in an attribute value string with no
/// surrounding quotes, such as in `<h2 id=heading>`.
///
/// Note that the HTML standard also restricts that character references must be unambiguous,
/// but this function has no way of verifying that.
[[nodiscard]]
constexpr bool is_html_unquoted_attribute_value_character(char8_t c)
{
    // https://html.spec.whatwg.org/dev/syntax.html#unquoted
    // clang-format off
    return !is_ascii_whitespace(c)
        && c != u8'"'
        && c != u8'\''
        && c != u8'='
        && c != u8'<'
        && c != u8'>'
        && c != u8'`';
    // clang-format on
}

/// @brief Returns `true` if `c` can appear in an attribute value string with no
/// surrounding quotes, such as in `<h2 id=heading>`.
///
/// Note that the HTML standard also restricts that character references must be unambiguous,
/// but this function has no way of verifying that.
[[nodiscard]]
constexpr bool is_html_unquoted_attribute_value_character(char32_t c)
{
    // https://html.spec.whatwg.org/dev/syntax.html#unquoted
    return !is_ascii(c) || is_html_attribute_name_character(char8_t(c));
}

/// @brief Returns `true` if `c` can legally appear
/// in the name of an MMML directive argument.
[[nodiscard]]
constexpr bool is_mmml_argument_name_character(char8_t c)
{
    return c == u8'-' || c == u8'_' || is_ascii_alphanumeric(c);
}

/// @brief Returns `true` if `c` can legally appear
/// in the name of an MMML directive.
[[nodiscard]]
constexpr bool is_mmml_directive_name_character(char8_t c)
{
    return c == u8'-' || is_ascii_alphanumeric(c);
}

} // namespace mmml

#endif
