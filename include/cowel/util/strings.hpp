#ifndef COWEL_STRINGS_HPP
#define COWEL_STRINGS_HPP

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include "ulight/impl/lang/cowel.hpp"

#include "cowel/util/assert.hpp"
#include "cowel/util/chars.hpp"
#include "cowel/util/unicode.hpp"

namespace cowel {

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

// see is_ascii_blank
inline constexpr std::u32string_view all_ascii_blank = U"\t\n\f\r\v ";
inline constexpr std::u8string_view all_ascii_blank8 = u8"\t\n\f\r\v ";

// see is_cowel_special_character
inline constexpr std::u32string_view all_cowel_special = U"\\{}[],";
inline constexpr std::u8string_view all_cowel_special8 = u8"\\{}[],";

[[nodiscard]]
inline std::string_view as_string_view(std::u8string_view str)
{
    return { reinterpret_cast<const char*>(str.data()), str.size() };
}

[[nodiscard]]
constexpr std::string_view as_string_view(std::span<const char> text)
{
    return { text.data(), text.size() };
}

[[nodiscard]]
constexpr std::u8string_view as_u8string_view(std::span<const char8_t> text)
{
    return { text.data(), text.size() };
}

[[nodiscard]]
constexpr std::u8string_view as_u8string_view(std::string_view text)
{
    return { reinterpret_cast<const char8_t*>(text.data()), text.size() };
}

namespace detail {

/// @brief Rudimentary version of `std::ranges::all_of` to avoid including all of `<algorithm>`
template <typename R, typename Predicate>
[[nodiscard]]
constexpr bool all_of(R&& r, Predicate predicate) // NOLINT(cppcoreguidelines-missing-std-forward)
{
    for (const auto& e : r) { // NOLINT(readability-use-anyofallof)
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

/// @brief Returns `true` if `str` is a possibly empty ASCII string comprised
/// entirely of blank ASCII characters (`is_ascii_blank`).
[[nodiscard]]
constexpr bool is_ascii_blank(std::u8string_view str)
{
    constexpr auto predicate = [](char8_t x) { return is_ascii_blank(x); };
    return detail::all_of(str, predicate);
}

[[nodiscard]]
constexpr std::size_t length_blank_left(std::u8string_view str)
{
    for (std::size_t i = 0; i < str.size(); ++i) {
        if (!is_ascii_blank(str[i])) {
            return i;
        }
    }
    return str.length();
}

[[nodiscard]]
constexpr std::size_t length_blank_right(std::u8string_view str)
{
    for (std::size_t i = 0; i < str.size(); ++i) {
        if (!is_ascii_blank(str[str.length() - i - 1])) {
            return i;
        }
    }
    return str.length();
}

[[nodiscard]]
constexpr std::u8string_view trim_ascii_blank_left(std::u8string_view str)
{
    return str.substr(length_blank_left(str));
}

[[nodiscard]]
constexpr std::u8string_view trim_ascii_blank_right(std::u8string_view str)
{
    return str.substr(0, str.length() - length_blank_right(str));
}

/// @brief Equivalent to `trim_ascii_blank_right(trim_ascii_blank_left(str))`.
[[nodiscard]]
constexpr std::u8string_view trim_ascii_blank(std::u8string_view str)
{
    return trim_ascii_blank_right(trim_ascii_blank_left(str));
}

template <typename Alloc>
void trim_left(std::vector<char8_t, Alloc>& text)
{
    const std::size_t amount = length_blank_left(as_u8string_view(text));
    COWEL_ASSERT(amount <= text.size());
    text.erase(text.begin(), text.begin() + std::ptrdiff_t(amount));
}

template <typename Alloc>
void trim_right(std::vector<char8_t, Alloc>& text)
{
    const std::size_t amount = length_blank_right(as_u8string_view(text));
    COWEL_ASSERT(amount <= text.size());
    text.erase(text.end() - std::ptrdiff_t(amount), text.end());
}

template <typename Alloc>
void trim(std::vector<char8_t, Alloc>& text)
{
    trim_left(text);
    trim_right(text);
}

/// @brief Returns `true` if `str` is a valid HTML tag identifier.
/// This includes both builtin tag names (which are purely alphabetic)
/// and custom tag names.
[[nodiscard]]
constexpr bool is_html_tag_name(std::u8string_view str)
{
    constexpr auto predicate = [](char32_t x) { return is_html_tag_name_character(x); };

    // https://html.spec.whatwg.org/dev/custom-elements.html#valid-custom-element-name
    return !str.empty() && is_ascii_alpha(str[0]) && utf8::all_of(str.substr(1), predicate);
}

/// @brief Returns `true` if `str` is a valid HTML attribute name.
[[nodiscard]]
constexpr bool is_html_attribute_name(std::u8string_view str)
{
    constexpr auto predicate = [](char32_t x) { return is_html_attribute_name_character(x); };

    // https://html.spec.whatwg.org/dev/syntax.html#syntax-attribute-name
    return !str.empty() && utf8::all_of(str, predicate);
}

/// @brief Returns `true` if the given string requires no wrapping in quotes when it
/// appears as the value in an attribute.
/// For example, `id=123` is a valid HTML attribute with a value and requires
/// no wrapping, but `id="<x>"` requires `<x>` to be surrounded by quotes.
[[nodiscard]]
constexpr bool is_html_unquoted_attribute_value(std::u8string_view str)
{
    constexpr auto predicate = [](char8_t code_unit) {
        return !is_ascii(code_unit) || is_html_ascii_unquoted_attribute_value_character(code_unit);
    };

    // https://html.spec.whatwg.org/dev/syntax.html#unquoted
    return detail::all_of(str, predicate);
}

/// @brief Returns `true` if `str` is a valid COWEL directive name.
[[nodiscard]]
constexpr bool is_directive_name(std::u8string_view str) noexcept
{
    const std::size_t length = ulight::cowel::match_directive_name(str);
    return length != 0 && str.length() == length;
}

namespace detail {

static constexpr ulight::Charset256 sanitized_id_set
    = ulight::is_ascii_alphanumeric_set | ulight::detail::to_charset256(u8"-_.:");

}

template <typename Alloc>
void sanitize_html_id(std::vector<char8_t, Alloc>& id)
{
    trim(id);
    std::erase_if(id, [](char8_t& c) -> bool {
        if (is_ascii_upper_alpha(c)) {
            c = to_ascii_lower(c);
            return false;
        }
        if (is_html_whitespace(c)) {
            c = u8'-';
            return false;
        }
        return !detail::sanitized_id_set.contains(c);
    });
}

} // namespace cowel

#endif
