#ifndef COWEL_DIFF_HPP
#define COWEL_DIFF_HPP

#include <algorithm>
#include <cstddef>
#include <memory_resource>
#include <span>
#include <string_view>
#include <vector>

#include "cowel/util/assert.hpp"

namespace cowel {

enum struct Edit_Type : signed char {
    /// @brief Delete an element in the source sequence.
    /// Advance by one element in the source sequence.
    del = -1,
    /// @brief Keep the element in the source sequence.
    /// Advance by one element in both sequences.
    common = 0,
    /// @brief Insert the element from the target sequence into the source sequence.
    /// Advance by one element in the target sequence.
    ins = 1,
};

namespace detail {

/// @brief Computes the last row of the Needleman-Wunsch score matrix.
/// This uses O(to.size()) space instead of O(from.size() * to.size()).
/// The cost function is:
/// - deletion=1
/// - insertion=1
/// - match=0
/// - replacement=infinity
inline void nw_score_row(
    const std::span<const std::u8string_view> from,
    const std::span<const std::u8string_view> to,
    const std::span<std::size_t> out_row
)
{
    COWEL_ASSERT(out_row.size() == to.size() + 1);

    for (std::size_t j = 0; j <= to.size(); ++j) {
        out_row[j] = j;
    }

    for (std::size_t i = 1; i <= from.size(); ++i) {
        std::size_t prev_diag = out_row[0];
        out_row[0] = i; // Cost of deleting i elements from `from`.

        for (std::size_t j = 1; j <= to.size(); ++j) {
            const std::size_t old_val = out_row[j];
            const std::size_t match_cost = from[i - 1] == to[j - 1] ? prev_diag : std::size_t(-1);
            const std::size_t del_cost = out_row[j] + 1;
            const std::size_t ins_cost = out_row[j - 1] + 1;
            out_row[j] = std::min({ match_cost, del_cost, ins_cost });
            prev_diag = old_val;
        }
    }
}

/// @brief Hirschberg's algorithm implementation that appends the edit script to `out`.
/// Uses O(min(from.size(), to.size())) space via divide-and-conquer.
inline void hirschberg_impl(
    const std::span<const std::u8string_view> from,
    const std::span<const std::u8string_view> to,
    std::pmr::vector<Edit_Type>& out,
    const std::span<std::size_t> scratch
)
{
    // The following three base cases are necessary to prevent infinite recursion.
    if (from.empty()) {
        out.insert(out.end(), to.size(), Edit_Type::ins);
        return;
    }
    if (to.empty()) {
        out.insert(out.end(), from.size(), Edit_Type::del);
        return;
    }
    if (from.size() == 1) {
        const auto it = std::ranges::find(to, from[0]);
        if (it == to.end()) {
            out.push_back(Edit_Type::del);
            out.insert(out.end(), to.size(), Edit_Type::ins);
            return;
        }
        const std::size_t match_idx = static_cast<std::size_t>(it - to.begin());
        out.insert(out.end(), match_idx, Edit_Type::ins);
        out.push_back(Edit_Type::common);
        out.insert(out.end(), to.size() - match_idx - 1, Edit_Type::ins);
        return;
    }

    const std::size_t x_mid = from.size() / 2;
    const auto from_left = from.subspan(0, x_mid);
    const auto from_right = from.subspan(x_mid);

    // Compute forward scores for from_left aligned with to.
    const auto score_l = scratch.subspan(0, to.size() + 1);
    nw_score_row(from_left, to, score_l);

    // Compute backward scores for from_right aligned with to (both reversed).
    const auto score_r = scratch.subspan(to.size() + 1, to.size() + 1);
    // Create reversed views by computing scores from the ends.
    // We need to reverse both from_right and to, then reverse the result.
    // Instead, we compute NWScore(rev(from_right), rev(to)) directly.

    // Initialize: cost of inserting j elements from reversed `to`.
    for (std::size_t j = 0; j <= to.size(); ++j) {
        score_r[j] = j;
    }
    // Process from_right in reverse order.
    for (std::size_t i = 1; i <= from_right.size(); ++i) {
        std::size_t prev_diag = score_r[0];
        score_r[0] = i;
        for (std::size_t j = 1; j <= to.size(); ++j) {
            const std::size_t old_val = score_r[j];
            // from_right[from_right.size() - i] is the i-th element when reversed.
            // to[to.size() - j] is the j-th element when reversed.
            const std::size_t match_cost = from_right[from_right.size() - i] == to[to.size() - j]
                ? prev_diag
                : std::size_t(-1);
            const std::size_t del_cost = score_r[j] + 1;
            const std::size_t ins_cost = score_r[j - 1] + 1;
            score_r[j] = std::min({ match_cost, del_cost, ins_cost });
            prev_diag = old_val;
        }
    }

    // Find the optimal split point in `to`
    // by finding argmin(score_l[j] + score_r[to.size() - j]).
    const auto y_mid = [&] -> std::size_t {
        std::size_t result = 0;
        auto min_score = std::size_t(-1);
        for (std::size_t j = 0; j <= to.size(); ++j) {
            const std::size_t combined = score_l[j] + score_r[to.size() - j];
            if (combined < min_score) {
                min_score = combined;
                result = j;
            }
        }
        return result;
    }();

    const auto to_left = to.subspan(0, y_mid);
    const auto to_right = to.subspan(y_mid);

    hirschberg_impl(from_left, to_left, out, scratch);
    hirschberg_impl(from_right, to_right, out, scratch);
}

} // namespace detail

/// @brief Uses
/// [Hirschberg's algorithm](https://en.wikipedia.org/wiki/Hirschberg%27s_algorithm)
/// to compute the Shortest Edit Script to convert sequence `from` into sequence `to`.
/// This is a space-efficient version of the
/// [Needleman-Wunsch algorithm](https://en.wikipedia.org/wiki/Needleman%E2%80%93Wunsch_algorithm).
inline std::pmr::vector<Edit_Type> shortest_edit_script(
    const std::span<const std::u8string_view> from,
    const std::span<const std::u8string_view> to,
    std::pmr::memory_resource* const memory = std::pmr::get_default_resource()
)
{
    std::pmr::vector<Edit_Type> out { memory };

    if (from.empty() && to.empty()) {
        return out;
    }

    std::pmr::vector<std::size_t> two_scratch_rows { (to.size() + 1) * 2, memory };
    detail::hirschberg_impl(from, to, out, two_scratch_rows);

    // At this stage, we technically have a valid shortest edit script.
    // However, the script sometimes contains insertions first, sometimes deletions first.
    // This is bad for human-readability; users typically expect deletions to come first.
    // To solve this, we partition deletions before insertions
    // within all block consisting of deletions and insertions.

    auto it = out.begin();
    while (it != out.end()) {
        // Find the next block of insertions/deletions.
        const auto next_mod_begin = std::ranges::find_if(it, out.end(), [](const Edit_Type t) {
            return t != Edit_Type::common;
        });
        const auto next_mod_end = std::ranges::find(next_mod_begin, out.end(), Edit_Type::common);
        // Partition the block so that deletions all precede insertions.
        std::ranges::partition(next_mod_begin, next_mod_end, [](Edit_Type t) {
            return t == Edit_Type::del;
        });
        it = next_mod_end;
    }

    return out;
}

} // namespace cowel

#endif
