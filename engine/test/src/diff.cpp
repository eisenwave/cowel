#include "diff.hpp"

#include <charconv>
#include <cstddef>
#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "cowel/util/from_chars.hpp"
#include "cowel/util/strings.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

/// @brief A single line of a diff, together with the `Source_Span` it occupies.
struct Diff_Line {
    std::u8string_view text;
    Source_Span span;
};

/// @brief Splits `text` into lines and records the `Source_Span` of each one.
/// Lines are delimited by a single U+000A END OF LINE code unit, i.e. `u8'\n'`.
/// A trailing `u8'\n'` produces a final empty line after it.
[[nodiscard]]
std::pmr::vector<Diff_Line>
split_diff_lines(const std::u8string_view text, std::pmr::memory_resource* const memory)
{
    std::pmr::vector<Diff_Line> result { memory };
    std::size_t begin = 0;
    std::size_t line = 0;
    while (begin < text.size()) {
        const std::size_t end = text.find(u8'\n', begin);
        const bool has_newline = end != std::u8string_view::npos;
        const std::size_t length = has_newline ? end - begin : text.size() - begin;
        const std::u8string_view line_text = text.substr(begin, length);
        const Source_Span line_span { { line, 0, begin }, length };
        result.push_back({ line_text, line_span });
        ++line;
        if (!has_newline) {
            break;
        }
        begin = end + 1;
    }
    if (!text.empty() && text.ends_with(u8'\n')) {
        const Source_Span line_span { { line, 0, text.size() }, 0 };
        result.push_back({ {}, line_span });
    }
    return result;
}

} // namespace

std::optional<Unified_Diff_Hunk_Header> parse_hunk_header(std::u8string_view header) noexcept
{
    // Consumes `expected` from the front of `header` if it is present.
    const auto consume_text = [&header](const std::u8string_view expected) {
        if (!header.starts_with(expected)) {
            return false;
        }
        header.remove_prefix(expected.size());
        return true;
    };
    // Consumes a decimal number from the front of `header` if one is present.
    const auto consume_number = [&header]() -> std::optional<std::size_t> {
        std::size_t value = 0;
        const std::from_chars_result parsed = from_characters(header, value);
        if (parsed.ec != std::errc {}) {
            return {};
        }
        header.remove_prefix(static_cast<std::size_t>(parsed.ptr - as_string_view(header).data()));
        return value;
    };
    // Consumes an optional `,count`, which defaults to one when it is omitted.
    const auto consume_count = [&header, &consume_number]() -> std::optional<std::size_t> {
        if (!header.starts_with(u8","sv)) {
            return std::size_t { 1 };
        }
        header.remove_prefix(1);
        return consume_number();
    };

    if (!consume_text(u8"@@ -"sv)) {
        return {};
    }
    const std::optional<std::size_t> old_start = consume_number();
    if (!old_start) {
        return {};
    }
    const std::optional<std::size_t> old_count = consume_count();
    if (!old_count) {
        return {};
    }
    if (!consume_text(u8" +"sv)) {
        return {};
    }
    const std::optional<std::size_t> new_start = consume_number();
    if (!new_start) {
        return {};
    }
    const std::optional<std::size_t> new_count = consume_count();
    if (!new_count) {
        return {};
    }
    // The remainder may be empty or a trailing section such as ` @@ int main()`.
    if (!header.empty() && !header.starts_with(u8" @@"sv)) {
        return {};
    }
    return Unified_Diff_Hunk_Header { *old_start, *old_count, *new_start, *new_count };
}

Result<Unified_Diff, Unified_Diff_Error>
parse_unified_diff(const std::u8string_view patch, std::pmr::memory_resource* const memory)
{
    std::pmr::vector<Unified_Diff_Hunk> hunks { memory };
    const std::pmr::vector<Diff_Line> lines = split_diff_lines(patch, memory);

    for (std::size_t i = 0; i < lines.size();) {
        const Diff_Line header = lines[i];
        if (!header.text.starts_with(u8"@@"sv)) {
            ++i;
            continue;
        }

        const std::optional<Unified_Diff_Hunk_Header> parsed_header
            = parse_hunk_header(header.text);
        if (!parsed_header) {
            return Unified_Diff_Error {
                Unified_Diff_Error_Kind::invalid_hunk_header,
                header.span,
                u8"Invalid unified diff hunk header."sv,
            };
        }

        Unified_Diff_Hunk hunk { memory };
        hunk.span = header.span;
        hunk.old_start = parsed_header->old_start;
        hunk.old_count = parsed_header->old_count;
        hunk.new_count = parsed_header->new_count;
        ++i;

        std::size_t consumed = 0;
        std::size_t produced = 0;
        while (i < lines.size()) {
            const Diff_Line body_line = lines[i];
            // A blank line terminates the hunk body.
            // The last line of a newline-terminated patch is always blank.
            if (body_line.text.empty() || body_line.text.starts_with(u8"@@"sv)) {
                break;
            }
            const char8_t marker = body_line.text.front();
            Edit_Type edit = Edit_Type::common;
            if (marker == u8'-') {
                edit = Edit_Type::del;
                ++consumed;
            }
            else if (marker == u8'+') {
                edit = Edit_Type::ins;
                ++produced;
            }
            else if (marker == u8' ') {
                ++consumed;
                ++produced;
            }
            else {
                return Unified_Diff_Error {
                    Unified_Diff_Error_Kind::invalid_hunk_line,
                    body_line.span,
                    u8"Invalid unified diff hunk body line."sv,
                };
            }
            const std::u8string_view line_text = body_line.text.substr(1);
            hunk.lines.push_back({ edit, line_text });
            ++i;
        }

        if (consumed != hunk.old_count || produced != hunk.new_count) {
            return Unified_Diff_Error {
                Unified_Diff_Error_Kind::hunk_count_mismatch,
                header.span,
                u8"Unified diff hunk counts do not match its body."sv,
            };
        }
        hunks.push_back(std::move(hunk));
    }
    return Unified_Diff { std::move(hunks) };
}

Result<std::pmr::u8string, Unified_Diff_Error> apply_unified_diff(
    const std::u8string_view source,
    const std::u8string_view patch,
    std::pmr::memory_resource* const memory
)
{
    if (!source.ends_with(u8'\n')) {
        return Unified_Diff_Error {
            Unified_Diff_Error_Kind::source_missing_trailing_newline,
            { {}, source.size() },
            u8"Source code must end with a newline."sv,
        };
    }

    const Result<Unified_Diff, Unified_Diff_Error> parsed = parse_unified_diff(patch, memory);
    if (!parsed) {
        return parsed.error();
    }

    const auto source_lines = split_diff_lines(source, memory);
    const std::size_t source_line_count = source_lines.size() - 1;
    std::pmr::u8string result { memory };
    bool first_line = true;
    const auto append_line = [&result, &first_line](const std::u8string_view line) {
        if (!first_line) {
            result.push_back(u8'\n');
        }
        result.append(line);
        first_line = false;
    };

    std::size_t source_index = 0;
    for (const Unified_Diff_Hunk& hunk : parsed.value().hunks) {
        if (hunk.old_count != 0 && hunk.old_start == 0) {
            return Unified_Diff_Error {
                Unified_Diff_Error_Kind::hunk_out_of_range,
                hunk.span,
                u8"Unified diff hunk starts outside the source code."sv,
            };
        }
        const std::size_t hunk_index = hunk.old_count == 0 ? hunk.old_start : hunk.old_start - 1;
        if (hunk_index < source_index) {
            return Unified_Diff_Error {
                Unified_Diff_Error_Kind::hunk_out_of_order,
                hunk.span,
                u8"Unified diff hunks overlap or are out of order."sv,
            };
        }
        if (hunk_index > source_line_count || hunk.old_count > source_line_count - hunk_index) {
            return Unified_Diff_Error {
                Unified_Diff_Error_Kind::hunk_out_of_range,
                hunk.span,
                u8"Unified diff hunk extends beyond the source code."sv,
            };
        }

        std::size_t check_index = hunk_index;
        for (const Unified_Diff_Hunk_Line& line : hunk.lines) {
            if (line.edit == Edit_Type::ins) {
                continue;
            }
            if (source_lines[check_index].text != line.text) {
                return Unified_Diff_Error {
                    Unified_Diff_Error_Kind::source_mismatch,
                    hunk.span,
                    u8"Unified diff context does not match the source code."sv,
                };
            }
            ++check_index;
        }

        while (source_index < hunk_index) {
            append_line(source_lines[source_index++].text);
        }
        for (const Unified_Diff_Hunk_Line& line : hunk.lines) {
            if (line.edit != Edit_Type::del) {
                append_line(line.text);
            }
        }
        source_index = hunk_index + hunk.old_count;
    }
    while (source_index < source_lines.size()) {
        append_line(source_lines[source_index++].text);
    }
    return result;
}

} // namespace cowel
