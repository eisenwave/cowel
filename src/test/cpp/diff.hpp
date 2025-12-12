#ifndef COWEL_TEST_DIFF_HPP
#define COWEL_TEST_DIFF_HPP

#include <algorithm>
#include <cstddef>
#include <memory_resource>
#include <span>
#include <string_view>
#include <vector>

#include "cowel/util/annotated_string.hpp"
#include "cowel/util/assert.hpp"

#include "cowel/diagnostic_highlight.hpp"
#include "cowel/fwd.hpp"

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

/// @brief Uses the
/// [Needleman-Wunsch algorithm](https://en.wikipedia.org/wiki/Needleman%E2%80%93Wunsch_algorithm)
/// to compute the Shortest Edit Script to convert sequence `from` into sequence `to`.
inline std::pmr::vector<Edit_Type> shortest_edit_script(
    std::span<const std::u8string_view> from,
    std::span<const std::u8string_view> to,
    std::pmr::memory_resource* memory
)
{
    std::pmr::vector<std::size_t> f_data((from.size() + 1) * (to.size() + 1), memory);
    const auto F = [&](std::size_t i, std::size_t j) -> std::size_t& {
        const std::size_t index = (i * (to.size() + 1)) + j;
        COWEL_DEBUG_ASSERT(index < f_data.size());
        return f_data[index];
    };

    for (std::size_t i = 0; i <= from.size(); ++i) {
        F(i, 0) = i;
    }
    for (std::size_t j = 0; j <= to.size(); ++j) {
        F(0, j) = j;
    }
    for (std::size_t i = 1; i <= from.size(); ++i) {
        for (std::size_t j = 1; j <= to.size(); ++j) {
            // The costs here are essentially the same as for Levenshtein distance computation,
            // except that if the two strings mismatch at a given index,
            // we consider the cost to be infinite.
            // This way, the output is made of pure insertions and deletions;
            // substitutions don't exist.
            F(i, j) = std::min(
                {
                    from[i - 1] == to[j - 1] ? F(i - 1, j - 1) : std::size_t(-1), // common
                    F(i - 1, j) + 1, // deletion
                    F(i, j - 1) + 1, // insertion
                }
            );
        }
    }

    std::size_t i = from.size();
    std::size_t j = to.size();

    std::pmr::vector<Edit_Type> out { memory };
    while (i != 0 || j != 0) {
        if (i != 0 && j != 0 && from[i - 1] == to[j - 1]) {
            out.push_back(Edit_Type::common);
            --i;
            --j;
        }
        else if (i != 0 && F(i, j) == F(i - 1, j) + 1) {
            out.push_back(Edit_Type::del);
            --i;
        }
        else {
            out.push_back(Edit_Type::ins);
            --j;
        }
    }

    std::ranges::reverse(out);
    auto it = out.begin();
    while (it != out.end()) {
        // Find the next block of insertions/deletions.
        const auto next_mod_begin = std::ranges::find_if(it, out.end(), [](Edit_Type t) {
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

inline void split_lines(std::pmr::vector<std::u8string_view>& out, std::u8string_view str)
{
    std::size_t pos = 0;
    std::size_t prev = 0;
    while ((pos = str.find('\n', prev)) != std::u8string_view::npos) {
        out.push_back(str.substr(prev, pos - prev));
        prev = pos + 1;
    }

    out.push_back(str.substr(prev));
}

inline void print_diff(
    Basic_Annotated_String<char8_t, Diagnostic_Highlight>& out,
    std::span<const std::u8string_view> from_lines,
    std::span<const std::u8string_view> to_lines
)
{
    const std::pmr::vector<Edit_Type> edits
        = shortest_edit_script(from_lines, to_lines, out.get_memory());
    std::size_t from_index = 0;
    std::size_t to_index = 0;
    for (const auto e : edits) {
        switch (e) {
        case Edit_Type::common: {
            out.build(Diagnostic_Highlight::diff_common)
                .append(u8' ')
                .append(from_lines[from_index])
                .append(u8'\n');
            ++from_index;
            ++to_index;
            break;
        }
        case Edit_Type::del: {
            out.build(Diagnostic_Highlight::diff_del) //
                .append(u8'-')
                .append(from_lines[from_index])
                .append(u8'\n');
            ++from_index;
            break;
        }
        case Edit_Type::ins: {
            out.build(Diagnostic_Highlight::diff_ins) //
                .append(u8'+')
                .append(to_lines[to_index])
                .append(u8'\n');
            ++to_index;
            break;
        }
        }
    }
}

inline void print_lines_diff(
    Basic_Annotated_String<char8_t, Diagnostic_Highlight>& out,
    std::u8string_view from,
    std::u8string_view to
)
{
    std::pmr::vector<std::u8string_view> from_lines { out.get_memory() };
    std::pmr::vector<std::u8string_view> to_lines { out.get_memory() };
    split_lines(from_lines, from);
    split_lines(to_lines, to);

    print_diff(out, from_lines, to_lines);
}

} // namespace cowel

#endif
