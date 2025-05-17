#ifndef COWEL_LEVENSHTEIN_HPP
#define COWEL_LEVENSHTEIN_HPP

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <ranges>

#include "cowel/util/assert.hpp"

namespace cowel {

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
    const auto x_size = std::size_t(std::ranges::size(x));
    const auto y_size = std::size_t(std::ranges::size(y));

    using result_type = std::ranges::range_value_t<Matrix>;

    const std::size_t required_matrix_size = (x_size + 1) * (y_size + 1);
    COWEL_ASSERT(std::size_t(std::ranges::size(m)) >= required_matrix_size);

    if (x_size == 0) {
        return result_type(y_size);
    }
    if (y_size == 0) {
        return result_type(x_size);
    }

    const auto matrix = [=, m_begin = std::ranges::begin(m)]
      (std::size_t i, std::size_t j) -> result_type& {
        const std::size_t index = (i * (y_size + 1)) + j;
        COWEL_DEBUG_ASSERT(index < required_matrix_size);
        return m_begin[std::ranges::range_difference_t<Matrix>(index)];
    };

    for (std::size_t i = 0; i <= x_size; ++i) {
        matrix(i, 0) = result_type(i);
    }
    for (std::size_t j = 0; j <= y_size; ++j) {
        matrix(0, j) = result_type(j);
    }

    const auto x_begin = std::ranges::begin(x);
    const auto y_begin = std::ranges::begin(y);

    for (std::size_t i = 1; i <= x_size; ++i) {
        for (std::size_t j = 1; j <= y_size; ++j) {
            const auto i_minus = std::ranges::range_difference_t<R1>(i - 1);
            const auto j_minus = std::ranges::range_difference_t<R2>(j - 1);
            const auto sub_cost = result_type(x_begin[i_minus] != y_begin[j_minus]);
            matrix(i, j) = std::min({
                matrix(i - 1, j    ) + 1,        // deletion
                matrix(i,     j - 1) + 1,        // insertion
                matrix(i - 1, j - 1) + sub_cost  // substitution
            });
        }
    }

    return matrix(x_size, y_size);
}
// clang-format on

} // namespace cowel

#endif
