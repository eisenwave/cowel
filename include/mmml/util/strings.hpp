#ifndef MMML_STRINGS_HPP
#define MMML_STRINGS_HPP

#include <string_view>

#include "mmml/util/chars.hpp"
#include "mmml/util/unicode.hpp"

namespace mmml {

// see is_ascii_digit
inline constexpr std::u32string_view all_ascii_digit = U"0123456789";
inline constexpr std::u8string_view all_ascii_digit8 = u8"0123456789";

// see is_ascii_lower_alpha
inline constexpr std::u32string_view all_ascii_lower_alpha = U"abcdefghijklmnopqrstuvwxyz";
inline constexpr std::u8string_view all_ascii_lower_alpha8 = u8"abcdefghijklmnopqrstuvwxyz";

// see is_ascii_upper_alpha
inline constexpr std::u32string_view all_ascii_upper_alpha = U"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
inline constexpr std::u8string_view all_ascii_upper_alpha8 = u8"ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// see is_ascii_alpha
inline constexpr std::u32string_view all_ascii_alpha
    = U"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
inline constexpr std::u8string_view all_ascii_alpha8
    = u8"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

// see is_ascii_alphanumeric
inline constexpr std::u32string_view all_ascii_alphanumeric
    = U"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
inline constexpr std::u8string_view all_ascii_alphanumeric8
    = u8"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

// see is_ascii_whitespace
inline constexpr std::u32string_view all_ascii_whitespace = U"\t\n\f\r ";
inline constexpr std::u8string_view all_ascii_whitespace8 = u8"\t\n\f\r ";

// see is_mmml_escapeable
inline constexpr std::u32string_view all_mmml_escapeable = U"\\{}";
inline constexpr std::u8string_view all_mmml_escapeable8 = u8"\\{}";

// see is_mmml_special_character
inline constexpr std::u32string_view all_mmml_special = U"\\{}[],";
inline constexpr std::u8string_view all_mmml_special8 = u8"\\{}[],";

[[nodiscard]]
inline std::string_view as_string_view(std::u8string_view str)
{
    return { reinterpret_cast<const char*>(str.data()), str.size() };
}

namespace detail {

/// @brief Rudimentary version of `std::ranges::all_of` to avoid including all of `<algorithm>`
template <typename R, typename Predicate>
[[nodiscard]]
constexpr bool all_of(R&& r, Predicate predicate)
{
    for (const auto& e : r) {
        if (!predicate(e)) {
            return false;
        }
    }
    return true;
}

} // namespace detail

/// @brief Returns `true` if `str` is a possibly empty ASCII string.
[[nodiscard]]
constexpr bool is_ascii(std::u8string_view str)
{
    constexpr auto predicate = [](char8_t x) { return is_ascii(x); };
    return detail::all_of(str, predicate);
}

/// @brief Returns `true` if `str` is a valid HTML tag identifier.
/// This includes both builtin tag names (which are purely alphabetic)
/// and custom tag names.
[[nodiscard]]
bool is_html_tag_name(std::u8string_view str)
{
    constexpr auto predicate = [](char32_t x) { return is_html_tag_name_character(x); };

    // https://html.spec.whatwg.org/dev/custom-elements.html#valid-custom-element-name
    return !str.empty() //
        && is_ascii_alpha(str[0]) && detail::all_of(utf8::Code_Point_View { str }, predicate);
}

/// @brief Returns `true` if `str` is a valid HTML attribute name.
[[nodiscard]]
bool is_html_attribute_name(std::u8string_view str)
{
    constexpr auto predicate = [](char32_t x) { return is_html_attribute_name_character(x); };

    // https://html.spec.whatwg.org/dev/syntax.html#syntax-attribute-name
    return !str.empty() //
        && detail::all_of(utf8::Code_Point_View { str }, predicate);
}

/// @brief Returns `true` if the given string requires no wrapping in quotes when it
/// appears as the value in an attribute.
/// For example, `id=123` is a valid HTML attribute with a value and requires
/// no wrapping, but `id="<x>"` requires `<x>` to be surrounded by quotes.
[[nodiscard]]
bool is_html_unquoted_attribute_value(std::u8string_view str)
{
    constexpr auto predicate
        = [](char8_t x) { return is_html_unquoted_attribute_value_character(x); };

    // https://html.spec.whatwg.org/dev/syntax.html#unquoted
    return detail::all_of(str, predicate);
}

} // namespace mmml

#endif
