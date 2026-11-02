#ifndef COWEL_TEST_DIFF_HPP
#define COWEL_TEST_DIFF_HPP

#include <cstddef>
#include <memory_resource>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cowel/util/annotated_string.hpp"
#include "cowel/util/diff.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/source_position.hpp"

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

/// @brief The kind of error that can occur while parsing or applying a unified diff.
enum struct Unified_Diff_Error_Kind : unsigned char {
    source_missing_trailing_newline,
    invalid_hunk_header,
    invalid_hunk_line,
    stray_hunk_line,
    hunk_count_mismatch,
    hunk_out_of_order,
    hunk_out_of_range,
    source_mismatch,
};

/// @brief An error that occurred while parsing or applying a unified diff.
struct Unified_Diff_Error {
    Unified_Diff_Error_Kind kind;
    Source_Span span;
    std::u8string_view message;
};

/// @brief The parsed contents of a unified diff hunk header such as
/// `@@ -1,3 +1,4 @@ some context`.
struct Unified_Diff_Hunk_Header {
    /// @brief The 1-based start line in the original ("old") file, as written in the header.
    /// A value of zero represents the empty range before the first line.
    std::size_t old_start = 0;
    /// @brief The number of lines the hunk covers in the original file.
    std::size_t old_count = 0;
    /// @brief The 1-based start line in the modified ("new") file, as written in the header.
    std::size_t new_start = 0;
    /// @brief The number of lines the hunk covers in the modified file.
    std::size_t new_count = 0;
};

/// @brief A single body line of a unified diff hunk.
struct Unified_Diff_Hunk_Line {
    Edit_Type edit;
    /// @brief The contents of the line,
    /// with the leading ` `, `-`, or `+` marker stripped.
    std::u8string_view text;

    bool operator==(const Unified_Diff_Hunk_Line&) const = default;
};

/// @brief A single hunk of a unified diff, i.e. a header followed by its body lines.
struct Unified_Diff_Hunk {
    Source_Span span {};
    std::size_t old_start = 0;
    std::size_t old_count = 0;
    std::size_t new_count = 0;
    std::pmr::vector<Unified_Diff_Hunk_Line> lines;

    explicit Unified_Diff_Hunk(std::pmr::memory_resource* const memory) noexcept
        : lines { memory }
    {
    }
};

/// @brief A unified diff consisting of zero or more hunks.
struct Unified_Diff {
    std::pmr::vector<Unified_Diff_Hunk> hunks;
};

/// @brief Parses the contents of a unified diff hunk header line,
/// for example `@@ -1,3 +1,4 @@ int main()`.
/// @return the parsed header, or `std::nullopt` if `header` is not a valid hunk header.
[[nodiscard]]
std::optional<Unified_Diff_Hunk_Header> parse_hunk_header(std::u8string_view header) noexcept;

/// @brief Parses `patch` as a unified diff.
[[nodiscard]]
Result<Unified_Diff, Unified_Diff_Error>
parse_unified_diff(std::u8string_view patch, std::pmr::memory_resource* memory);

/// @brief Applies `patch` (a unified diff) to `source` and returns the result.
[[nodiscard]]
Result<std::pmr::u8string, Unified_Diff_Error> apply_unified_diff(
    std::u8string_view source,
    std::u8string_view patch,
    std::pmr::memory_resource* memory
);

} // namespace cowel

#endif
