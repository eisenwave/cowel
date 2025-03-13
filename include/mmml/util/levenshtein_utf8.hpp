#ifndef MMML_LEVENSHTEIN_UTF8_HPP
#define MMML_LEVENSHTEIN_UTF8_HPP

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "mmml/util/levenshtein.hpp"
#include "mmml/util/unicode.hpp"

namespace mmml {

namespace detail {

[[nodiscard]]
inline std::pmr::u32string to_utf32(std::u8string_view str, std::pmr::memory_resource* memory)
{
    std::pmr::u32string result { memory };
    result.reserve(str.size());
    std::ranges::copy(utf8::Code_Point_View { str }, std::back_inserter(result));
    return result;
}

// These two functions are intended to limit instantiations to these two functions in general.

[[nodiscard]] [[gnu::always_inline]]
constexpr std::size_t levenshtein_distance32(
    std::u32string_view x,
    std::u32string_view y,
    std::span<std::size_t> matrix_data
)
{
    return levenshtein_distance(x, y, matrix_data);
}

[[nodiscard]] [[gnu::always_inline]]
constexpr std::size_t levenshtein_distance8(
    std::u8string_view x,
    std::u8string_view y,
    std::span<std::size_t> matrix_data
)
{
    return levenshtein_distance(x, y, matrix_data);
}

} // namespace detail

// https://en.wikipedia.org/wiki/Levenshtein_distance

/// @brief Computes the Levenshtein distance between the code points of two UTF-8 strings.
[[nodiscard]]
inline std::size_t code_point_levenshtein_distance(
    std::u8string_view x,
    std::u8string_view y,
    std::pmr::memory_resource* memory
)
{
    const std::pmr::u32string x32 = detail::to_utf32(x, memory);
    const std::pmr::u32string y32 = detail::to_utf32(y, memory);
    const std::size_t required_matrix_size = (x32.size() + 1) * (y32.size() + 1);
    std::pmr::vector<std::size_t> matrix { required_matrix_size, memory };

    return detail::levenshtein_distance32(x32, y32, matrix);
}

/// @brief Computes the Levenshtein distance between the code units of two UTF-8 strings.
/// This approach is typically useful if the strings are known to store only ASCII characters,
/// in which case the result is equivalent to `code_point_levenshtein_distance`.
[[nodiscard]]
inline std::size_t code_unit_levenshtein_distance(
    std::u8string_view x,
    std::u8string_view y,
    std::pmr::memory_resource* memory
)
{
    const std::size_t required_matrix_size = (x.size() + 1) * (y.size() + 1);
    std::pmr::vector<std::size_t> matrix { required_matrix_size, memory };

    return detail::levenshtein_distance8(x, y, matrix);
}

} // namespace mmml

#endif
