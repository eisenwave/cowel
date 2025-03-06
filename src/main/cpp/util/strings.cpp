#include <algorithm>

#include "mmml/util/strings.hpp"
#include "mmml/util/unicode.hpp"

namespace mmml {

[[nodiscard]]
bool is_html_tag_name(std::u8string_view str)
{
    constexpr auto predicate = [](char32_t x) { return is_html_tag_name_character(x); };

    // https://html.spec.whatwg.org/dev/custom-elements.html#valid-custom-element-name
    return !str.empty() //
        && is_ascii_alphabetic(str[0])
        && std::ranges::all_of(utf8::Code_Point_View { str }, predicate);
}

[[nodiscard]]
bool is_html_attribute_name(std::u8string_view str)
{
    constexpr auto predicate = [](char32_t x) { return is_html_attribute_name_character(x); };

    // https://html.spec.whatwg.org/dev/syntax.html#syntax-attribute-name
    return !str.empty() //
        && std::ranges::all_of(utf8::Code_Point_View { str }, predicate);
}

[[nodiscard]]
bool is_html_unquoted_attribute_value(std::u8string_view value)
{
    constexpr auto predicate
        = [](char8_t x) { return is_html_unquoted_attribute_value_character(x); };

    // https://html.spec.whatwg.org/dev/syntax.html#unquoted
    return std::ranges::all_of(value, predicate);
}

} // namespace mmml
