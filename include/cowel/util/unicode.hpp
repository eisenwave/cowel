#ifndef COWEL_UNICODE_HPP
#define COWEL_UNICODE_HPP

#include <cstddef>
#include <string_view>

#include "ulight/impl/unicode.hpp"
#include "ulight/impl/unicode_algorithm.hpp"

namespace cowel::utf8 {

using ulight::utf8::all_of;
using ulight::utf8::any_not_of;
using ulight::utf8::any_of;
using ulight::utf8::Code_Point_And_Length;
using ulight::utf8::Code_Point_Iterator;
using ulight::utf8::Code_Point_Iterator_Sentinel;
using ulight::utf8::Code_Point_View;
using ulight::utf8::code_points_unchecked;
using ulight::utf8::Code_Units_And_Length;
using ulight::utf8::decode;
using ulight::utf8::decode_and_length;
using ulight::utf8::decode_and_length_or_replacement;
using ulight::utf8::decode_and_length_unchecked;
using ulight::utf8::decode_unchecked;
using ulight::utf8::encode8_unchecked;
using ulight::utf8::Error_Code;
using ulight::utf8::error_code_message;
using ulight::utf8::find_if;
using ulight::utf8::find_if_not;
using ulight::utf8::is_valid;
using ulight::utf8::length_if;
using ulight::utf8::length_if_not;
using ulight::utf8::none_of;
using ulight::utf8::sequence_length;
using ulight::utf8::Unicode_Error;
using ulight::utf8::Unicode_Error_Handling;

/// @brief Returns the length of `str`, in code points.
/// Any illegal code units are counted as one code point,
/// which is consistent with treating them as a U+FFFD REPLACEMENT CHARACTER.
[[nodiscard]]
constexpr std::size_t count_code_points_or_replacement(std::u8string_view str) noexcept
{
    std::size_t result = 0;
    while (!str.empty()) {
        const auto [_, length] = decode_and_length_or_replacement(str);
        str.remove_prefix(std::size_t(length));
        ++result;
    }
    return result;
}

/// @brief Returns the length of `str`, in code units, when encoded.
[[nodiscard]]
constexpr std::size_t count_code_units_unchecked(std::u32string_view str)
{
    std::size_t result = 0;
    for (const char32_t c : str) {
        const auto [_, code_units] = utf8::encode8_unchecked(c);
        result += std::size_t(code_units);
    }
    return result;
}

} // namespace cowel::utf8

#endif
