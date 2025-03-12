#ifndef MMML_LEVENSHTEIN_HPP
#define MMML_LEVENSHTEIN_HPP

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <ranges>

#include "mmml/util/assert.hpp"

namespace mmml {

// https://en.wikipedia.org/wiki/Levenshtein_distance

// clang-format off
template <
    std::ranges::random_access_range R1,
    std::ranges::random_access_range R2,
    std::ranges::random_access_range Matrix
>
  requires std::equality_comparable_with<std::ranges::range_value_t<R1>, std::ranges::range_value_t<R2>>
        && std::integral<std::ranges::range_value_t<Matrix>>
[[nodiscard]]
constexpr std::ranges::range_value_t<Matrix> levenshtein_distance(
    const R1& x,
    const R2& y,
    Matrix&& m // NOLINT(cppcoreguidelines-missing-std-forward)
) {
    const auto x_size = std::ranges::size(x);
    const auto y_size = std::ranges::size(y);

    using x_size_type = decltype(+x_size);
    using y_size_type = decltype(+y_size);
    using result_type = std::ranges::range_value_t<Matrix>;

    const std::size_t required_matrix_size = std::size_t((x_size + 1) * (y_size + 1));
    MMML_ASSERT(std::ranges::size(m) >= required_matrix_size);

    if (x_size == 0) {
        return result_type(y_size);
    }
    if (y_size == 0) {
        return result_type(x_size);
    }

    const auto matrix = [=, m_begin = std::ranges::begin(m)]
      (x_size_type i, y_size_type j) -> result_type& {
        using diff_type = std::ranges::range_difference_t<Matrix>;
        const auto index = ((diff_type(i) * diff_type(y_size + 1)) + diff_type(j));
        MMML_DEBUG_ASSERT(std::size_t(index) < required_matrix_size);
        return m_begin[index];
    };

    for (x_size_type i = 0; i <= x_size; ++i) {
        matrix(i, 0) = result_type(i);
    }
    for (y_size_type j = 0; j <= y_size; ++j) {
        matrix(0, j) = result_type(j);
    }

    for (x_size_type i = 1; i <= x_size; ++i) {
        for (y_size_type j = 1; j <= y_size; ++j) {
            matrix(i, j) = std::min({
                matrix(i - 1, j    ) + 1,                                // deletion
                matrix(i,     j - 1) + 1,                                // insertion
                matrix(i - 1, j - 1) + result_type(y[j - 1] != x[i - 1]) // substitution
            });
        }
    }

    return matrix(x_size, y_size);
}
// clang-format on

} // namespace mmml

#endif
