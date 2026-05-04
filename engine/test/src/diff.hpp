#ifndef COWEL_TEST_DIFF_HPP
#define COWEL_TEST_DIFF_HPP

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include "cowel/util/annotated_string.hpp"
#include "cowel/util/diff.hpp"

#include "cowel/diagnostic_highlight.hpp"

namespace cowel {

/// @brief Splits `str` into lines and appends each line to `out`.
/// Lines are delimited by a single U+000A END OF LINE code unit, i.e. `u8'\n'`.
inline void split_lines(std::pmr::vector<std::u8string_view>& out, const std::u8string_view str)
{
    std::size_t pos = 0;
    std::size_t prev = 0;
    while ((pos = str.find('\n', prev)) != std::u8string_view::npos) {
        out.push_back(str.substr(prev, pos - prev));
        prev = pos + 1;
    }

    out.push_back(str.substr(prev));
}

/// @brief Computes the shortest edit script between the original `from_lines`
/// necessary to produce `to_lines`,
/// and appends the script to `out`.
inline void print_diff(
    Basic_Annotated_String<char8_t, Diagnostic_Highlight>& out,
    const std::span<const std::u8string_view> from_lines,
    const std::span<const std::u8string_view> to_lines
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
    const std::u8string_view from,
    const std::u8string_view to
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
