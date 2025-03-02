#ifndef MMML_CHARS_HPP
#define MMML_CHARS_HPP

namespace mmml {

/// @brief Returns `true` if the `c` is a decimal digit (`0` through `9`).
[[nodiscard]]
constexpr bool is_decimal_digit(char8_t c)
{
    return c >= u8'0' && c <= u8'9';
}

/// @brief Returns `true` if the `c` is whitespace.
[[nodiscard]]
constexpr bool is_space(char8_t c)
{
    return c == u8' ' || c == u8'\n' || c == u8'\t' || c == u8'\r';
}

/// @brief Returns `true` if `c` is a latin character (`[a-zA-Z]`).
[[nodiscard]]
constexpr bool is_ascii_alphabetic(char8_t c)
{
    return (c >= u8'a' && c <= u8'z') || (c >= u8'A' && c <= u8'Z');
}

/// @brief Returns `is_decimal_digit(c) || is_latin(c)`.
[[nodiscard]]
constexpr bool is_ascii_alphanumeric(char8_t c)
{
    return is_decimal_digit(c) || is_ascii_alphanumeric(c);
}

/// @brief Returns `true` if `c` is an escapable MMML character.
/// That is, if `\\c` would corresponds to the literal character `c`,
/// rather than starting a directive or being treated as literal text.
[[nodiscard]]
constexpr bool is_mmml_escapeable(char8_t c)
{
    return c == u8'\\' || c == u8'}' || c == u8'{';
}

/// @brief Returns `true` if `c` can legally appear
/// in the name of an HTML tag.
[[nodiscard]]
constexpr bool is_html_tag_name_character(char8_t c)
{
    // https://html.spec.whatwg.org/dev/syntax.html#syntax-tag-name
    // https://html.spec.whatwg.org/dev/custom-elements.html#valid-custom-element-name
    // FIXME: Technically, we should be using char32_t to cover the various other supported code
    // points, but we don't care about that for now.
    return c == u8'-' || c == u8'.' || c == u8'_' || is_ascii_alphanumeric(c);
}

[[nodiscard]]
constexpr bool is_html_attribute_name_character(char8_t c)
{
    // https://html.spec.whatwg.org/dev/syntax.html#syntax-attribute-name
    // FIXME: this implementation is needlessly restrictive
    //        attribute names can be pretty much anything
    return is_html_tag_name_character(c);
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
