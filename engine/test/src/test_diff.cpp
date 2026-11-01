#include <array>
#include <random>
#include <string_view>

#include <gtest/gtest.h>

#include "diff.hpp"

namespace cowel {
namespace {

using namespace std::string_view_literals;

TEST(Diff, empty)
{
    // Note that this test (and all subsequent tests) use std::pmr::vector
    // for the test expectations (rather than std::array)
    // because this makes equality comparison possible for EXPECT_EQ.
    static const std::pmr::vector<Edit_Type> expected;

    const std::pmr::vector<Edit_Type> actual = shortest_edit_script({}, {});
    EXPECT_EQ(expected, actual);
}

TEST(Diff, one_line_common)
{
    static constexpr std::array<std::u8string_view, 1> from { u8"awoo" };
    static constexpr auto to = from;
    static const std::pmr::vector<Edit_Type> expected { Edit_Type::common };

    const std::pmr::vector<Edit_Type> actual = shortest_edit_script(from, to);
    EXPECT_EQ(expected, actual);
}

TEST(Diff, one_line_del)
{
    static constexpr std::array<std::u8string_view, 1> from { u8"awoo" };
    static constexpr std::array<std::u8string_view, 0> to;
    static const std::pmr::vector<Edit_Type> expected { Edit_Type::del };

    const std::pmr::vector<Edit_Type> actual = shortest_edit_script(from, to);
    EXPECT_EQ(expected, actual);
}

TEST(Diff, one_line_ins)
{
    static constexpr std::array<std::u8string_view, 0> from;
    static constexpr std::array<std::u8string_view, 1> to { u8"awoo" };
    static const std::pmr::vector<Edit_Type> expected { Edit_Type::ins };

    const std::pmr::vector<Edit_Type> actual = shortest_edit_script(from, to);
    EXPECT_EQ(expected, actual);
}

TEST(Diff, multiple_common_identical)
{
    static constexpr std::array<std::u8string_view, 3> from {
        u8"common",
        u8"common",
        u8"common",
    };
    static constexpr auto to = from;
    static const std::pmr::vector<Edit_Type> expected {
        Edit_Type::common,
        Edit_Type::common,
        Edit_Type::common,
    };

    const std::pmr::vector<Edit_Type> actual = shortest_edit_script(from, to);
    EXPECT_EQ(expected, actual);
}

TEST(Diff, multiple_common_distinct)
{
    static constexpr std::array<std::u8string_view, 3> from {
        u8"common1",
        u8"common2",
        u8"common3",
    };
    static constexpr auto to = from;
    static const std::pmr::vector<Edit_Type> expected {
        Edit_Type::common,
        Edit_Type::common,
        Edit_Type::common,
    };

    const std::pmr::vector<Edit_Type> actual = shortest_edit_script(from, to);
    EXPECT_EQ(expected, actual);
}

TEST(Diff, multiple_del)
{
    static constexpr std::array<std::u8string_view, 3> from {
        u8"del",
        u8"del",
        u8"del",
    };
    static constexpr std::array<std::u8string_view, 0> to;
    static const std::pmr::vector<Edit_Type> expected {
        Edit_Type::del,
        Edit_Type::del,
        Edit_Type::del,
    };

    const std::pmr::vector<Edit_Type> actual = shortest_edit_script(from, to);
    EXPECT_EQ(expected, actual);
}

TEST(Diff, multiple_ins)
{
    static constexpr std::array<std::u8string_view, 0> from;
    static constexpr std::array<std::u8string_view, 3> to {
        u8"ins",
        u8"ins",
        u8"ins",
    };
    static const std::pmr::vector<Edit_Type> expected {
        Edit_Type::ins,
        Edit_Type::ins,
        Edit_Type::ins,
    };

    const std::pmr::vector<Edit_Type> actual = shortest_edit_script(from, to);
    EXPECT_EQ(expected, actual);
}

TEST(Diff, common_then_del)
{
    static constexpr std::array<std::u8string_view, 3> from {
        u8"common",
        u8"del",
        u8"del",
    };
    static constexpr std::array<std::u8string_view, 1> to {
        u8"common",
    };
    static const std::pmr::vector<Edit_Type> expected {
        Edit_Type::common,
        Edit_Type::del,
        Edit_Type::del,
    };

    const std::pmr::vector<Edit_Type> actual = shortest_edit_script(from, to);
    EXPECT_EQ(expected, actual);
}

TEST(Diff, common_then_ins)
{
    static constexpr std::array<std::u8string_view, 1> from {
        u8"common",
    };
    static constexpr std::array<std::u8string_view, 3> to {
        u8"common",
        u8"ins",
        u8"ins",
    };
    static const std::pmr::vector<Edit_Type> expected {
        Edit_Type::common,
        Edit_Type::ins,
        Edit_Type::ins,
    };

    const std::pmr::vector<Edit_Type> actual = shortest_edit_script(from, to);
    EXPECT_EQ(expected, actual);
}

TEST(Diff, del_then_common)
{
    static constexpr std::array<std::u8string_view, 3> from {
        u8"del",
        u8"del",
        u8"common",
    };
    static constexpr std::array<std::u8string_view, 1> to {
        u8"common",
    };
    static const std::pmr::vector<Edit_Type> expected {
        Edit_Type::del,
        Edit_Type::del,
        Edit_Type::common,
    };

    const std::pmr::vector<Edit_Type> actual = shortest_edit_script(from, to);
    EXPECT_EQ(expected, actual);
}

TEST(Diff, ins_then_common)
{
    static constexpr std::array<std::u8string_view, 1> from {
        u8"common",
    };
    static constexpr std::array<std::u8string_view, 3> to {
        u8"ins",
        u8"ins",
        u8"common",
    };
    static const std::pmr::vector<Edit_Type> expected {
        Edit_Type::ins,
        Edit_Type::ins,
        Edit_Type::common,
    };

    const std::pmr::vector<Edit_Type> actual = shortest_edit_script(from, to);
    EXPECT_EQ(expected, actual);
}

TEST(Diff, del_then_ins)
{
    // clang-format off
    static constexpr std::array<std::u8string_view, 2> from {
        u8"del1",
        u8"del2",
    };
    static constexpr std::array<std::u8string_view, 2> to {
        u8"ins1",
        u8"ins2",
    };
    static const std::pmr::vector<Edit_Type> expected {
        Edit_Type::del,
        Edit_Type::del,
        Edit_Type::ins,
        Edit_Type::ins,
    };
    // clang-format on

    const std::pmr::vector<Edit_Type> actual = shortest_edit_script(from, to);
    EXPECT_EQ(expected, actual);
}

TEST(Diff, mixed_operations)
{
    // clang-format off
    static constexpr std::array<std::u8string_view, 4> from {
        u8"common1",
        u8"del1",
        u8"common2",
        u8"del2",
    };
    static constexpr std::array<std::u8string_view, 4> to {
        u8"common1",
        u8"ins1",
        u8"common2",
        u8"ins2",
    };
    static const std::pmr::vector<Edit_Type> expected {
        Edit_Type::common,
        Edit_Type::del,
        Edit_Type::ins,
        Edit_Type::common,
        Edit_Type::del,
        Edit_Type::ins,
    };
    // clang-format on

    const std::pmr::vector<Edit_Type> actual = shortest_edit_script(from, to);
    EXPECT_EQ(expected, actual);
}

TEST(Diff, interleaved_changes)
{
    // clang-format off
    static constexpr std::array<std::u8string_view, 5> from {
        u8"common1",
        u8"del",
        u8"common2",
        u8"del",
        u8"common3",
    };
    static constexpr std::array<std::u8string_view, 5> to {
        u8"common1",
        u8"ins1",
        u8"common2",
        u8"ins2",
        u8"common3",
    };
    static const std::pmr::vector<Edit_Type> expected {
        Edit_Type::common,
        Edit_Type::del, 
        Edit_Type::ins, 
        Edit_Type::common,
        Edit_Type::del, 
        Edit_Type::ins, 
        Edit_Type::common,
    };
    // clang-format on

    const std::pmr::vector<Edit_Type> actual = shortest_edit_script(from, to);
    EXPECT_EQ(expected, actual);
}

TEST(Diff, all_different)
{
    // clang-format off
    static constexpr std::array<std::u8string_view, 3> from {
        u8"del1",
        u8"del2",
        u8"del3",
    };
    static constexpr std::array<std::u8string_view, 3> to {
        u8"ins1",
        u8"ins2",
        u8"ins3",
    };
    static const std::pmr::vector<Edit_Type> expected {
        Edit_Type::del,
        Edit_Type::del,
        Edit_Type::del,
        Edit_Type::ins,
        Edit_Type::ins,
        Edit_Type::ins,
    };
    // clang-format on

    const std::pmr::vector<Edit_Type> actual = shortest_edit_script(from, to);
    EXPECT_EQ(expected, actual);
}

TEST(Diff, del_ins_partition_test)
{
    // clang-format off
    static constexpr std::array<std::u8string_view, 4> from {
        u8"common1",
        u8"del1",
        u8"del2",
        u8"common2",
    };
    static constexpr std::array<std::u8string_view, 4> to {
        u8"common1",
        u8"ins1",
        u8"ins2",
        u8"common2",
    };
    static const std::pmr::vector<Edit_Type> expected {
        Edit_Type::common,
        Edit_Type::del,
        Edit_Type::del,
        Edit_Type::ins,
        Edit_Type::ins,
        Edit_Type::common,
    };
    // clang-format on

    const std::pmr::vector<Edit_Type> actual = shortest_edit_script(from, to);
    EXPECT_EQ(expected, actual);
}

TEST(Diff, prefix_match)
{
    static constexpr std::array<std::u8string_view, 3> from {
        u8"common1",
        u8"common2",
        u8"del",
    };
    static constexpr std::array<std::u8string_view, 2> to {
        u8"common1",
        u8"common2",
    };
    static const std::pmr::vector<Edit_Type> expected {
        Edit_Type::common,
        Edit_Type::common,
        Edit_Type::del,
    };

    const std::pmr::vector<Edit_Type> actual = shortest_edit_script(from, to);
    EXPECT_EQ(expected, actual);
}

TEST(Diff, suffix_match)
{
    static constexpr std::array<std::u8string_view, 3> from {
        u8"del",
        u8"common1",
        u8"common2",
    };
    static constexpr std::array<std::u8string_view, 2> to {
        u8"common1",
        u8"common2",
    };
    static const std::pmr::vector<Edit_Type> expected {
        Edit_Type::del,
        Edit_Type::common,
        Edit_Type::common,
    };

    const std::pmr::vector<Edit_Type> actual = shortest_edit_script(from, to);
    EXPECT_EQ(expected, actual);
}

TEST(Diff, middle_match)
{
    static constexpr std::array<std::u8string_view, 3> from {
        u8"del",
        u8"common",
        u8"del",
    };
    static constexpr std::array<std::u8string_view, 1> to {
        u8"common",
    };
    static const std::pmr::vector<Edit_Type> expected {
        Edit_Type::del,
        Edit_Type::common,
        Edit_Type::del,
    };

    const std::pmr::vector<Edit_Type> actual = shortest_edit_script(from, to);
    EXPECT_EQ(expected, actual);
}

TEST(Diff, complex_scenario)
{
    // clang-format off
    static constexpr std::array<std::u8string_view, 8> from {
        u8"header",
        u8"function1",
        u8"function2",
        u8"function3",
        u8"middle",
        u8"oldcode1",
        u8"oldcode2",
        u8"footer",
    };
    static constexpr std::array<std::u8string_view, 8> to {
        u8"header",
        u8"function1",
        u8"function4",
        u8"function5",
        u8"middle",
        u8"newcode1",
        u8"newcode2",
        u8"footer",
    };
    static const std::pmr::vector<Edit_Type> expected {
        Edit_Type::common, // header
        Edit_Type::common, // function1
        Edit_Type::del, // function2
        Edit_Type::del, // function3
        Edit_Type::ins, // function4
        Edit_Type::ins, // function5
        Edit_Type::common, // middle
        Edit_Type::del, // oldcode1
        Edit_Type::del, // oldcode2
        Edit_Type::ins, // newcode1
        Edit_Type::ins, // newcode2
        Edit_Type::common, // footer
    };
    // clang-format on

    const std::pmr::vector<Edit_Type> actual = shortest_edit_script(from, to);
    EXPECT_EQ(expected, actual);
}

std::size_t apply_edits(
    std::u8string_view* const result,
    const std::span<const Edit_Type> edits,
    const std::span<const std::u8string_view> from,
    const std::span<const std::u8string_view> to
)
{
    std::size_t result_index = 0;
    std::size_t from_index = 0;
    std::size_t to_index = 0;
    for (const auto e : edits) {
        switch (e) {
        case Edit_Type::common: {
            COWEL_ASSERT(from[from_index] == to[to_index]);
            result[result_index++] = from[from_index];
            ++from_index;
            ++to_index;
            break;
        }
        case Edit_Type::del: {
            ++from_index;
            break;
        }
        case Edit_Type::ins: {
            result[result_index++] = to[to_index];
            ++to_index;
            break;
        }
        }
    }
    return result_index;
}

TEST(Diff, fuzz)
{
    static constexpr auto alphabet = u8"abcdefghijklmnopqrstuvwxyz"sv;
    constexpr int samples = 2'000;

    std::u8string_view from_buffer[64];
    std::u8string_view to_buffer[64];
    std::u8string_view result_buffer[64];

    static std::default_random_engine rng { 12345 };
    static std::uniform_int_distribution<std::size_t> size_distr { 0, 64 };
    static std::uniform_int_distribution<unsigned> letter_distr { 0, 25 };

    for (int i = 0; i < samples; ++i) {
        const auto from_size = size_distr(rng);
        const auto to_size = size_distr(rng);

        std::span<std::u8string_view> from { from_buffer, from_size };
        std::span<std::u8string_view> to { to_buffer, to_size };

        for (std::u8string_view& c : from) {
            c = alphabet.substr(letter_distr(rng), 1);
        }
        for (std::u8string_view& c : to) {
            c = alphabet.substr(letter_distr(rng), 1);
        }

        const std::pmr::vector<Edit_Type> edits = shortest_edit_script(from, to);
        ASSERT_LE(edits.size(), from.size() + to.size());

        const std::size_t result_length = apply_edits(result_buffer, edits, from, to);
        const std::span<const std::u8string_view> result_span { result_buffer, result_length };

        EXPECT_EQ(result_length, to_size);
        EXPECT_TRUE(std::ranges::equal(result_span, to));
    }
}

TEST(Diff, parse_hunk_header)
{
    const auto header = parse_hunk_header(u8"@@ -1,3 +1,4 @@ int main()");
    ASSERT_TRUE(header);
    EXPECT_EQ(header->old_start, 1);
    EXPECT_EQ(header->old_count, 3);
    EXPECT_EQ(header->new_start, 1);
    EXPECT_EQ(header->new_count, 4);

    // The count defaults to 1 when it is omitted.
    const auto single = parse_hunk_header(u8"@@ -5 +6 @@");
    ASSERT_TRUE(single);
    EXPECT_EQ(single->old_start, 5);
    EXPECT_EQ(single->old_count, 1);
    EXPECT_EQ(single->new_start, 6);
    EXPECT_EQ(single->new_count, 1);

    EXPECT_FALSE(parse_hunk_header(u8"@@ -x,1 +1,1 @@"));
    EXPECT_FALSE(parse_hunk_header(u8"not a hunk header"));
}

TEST(Diff, unified_diff_parse)
{
    const auto parsed = parse_unified_diff(
        u8R"(--- a/file
+++ b/file
@@ -2,2 +2,3 @@ function
 line2
-old
+new
+extra
)",
        std::pmr::get_default_resource()
    );

    ASSERT_TRUE(parsed);
    ASSERT_EQ(parsed->hunks.size(), 1);
    const Unified_Diff_Hunk& hunk = parsed->hunks.front();
    EXPECT_EQ(hunk.old_start, 2);
    EXPECT_EQ(hunk.old_count, 2);
    EXPECT_EQ(hunk.new_count, 3);
    const std::pmr::vector<Unified_Diff_Hunk_Line> expected_lines {
        { Edit_Type::common, u8"line2" },
        { Edit_Type::del, u8"old" },
        { Edit_Type::ins, u8"new" },
        { Edit_Type::ins, u8"extra" },
    };
    EXPECT_EQ(hunk.lines, expected_lines);
}

TEST(Diff, unified_diff_apply)
{
    const auto result = apply_unified_diff(
        u8"one\ntwo\nthree\n", u8R"(@@ -1,3 +1,2 @@
-one
+ONE
 two
-three
)",
        std::pmr::get_default_resource()
    );

    ASSERT_TRUE(result);
    EXPECT_EQ(result.value(), u8"ONE\ntwo\n");
}

TEST(Diff, unified_diff_apply_empty)
{
    const auto result = apply_unified_diff(u8"one\ntwo\n", {}, std::pmr::get_default_resource());

    ASSERT_TRUE(result);
    EXPECT_EQ(result.value(), u8"one\ntwo\n");
}

TEST(Diff, unified_diff_apply_multiple_hunks)
{
    const auto result = apply_unified_diff(
        u8"a\nb\nc\nd\ne\nf\n", u8R"(@@ -1,2 +1,2 @@
-a
+A
 b
@@ -5,2 +5,3 @@
 e
-f
+F
+G
)",
        std::pmr::get_default_resource()
    );

    ASSERT_TRUE(result);
    EXPECT_EQ(result.value(), u8"A\nb\nc\nd\ne\nF\nG\n");
}

TEST(Diff, unified_diff_apply_insertion_at_beginning_and_end)
{
    const auto result = apply_unified_diff(
        u8"a\nb\n", u8R"(@@ -0,0 +1,1 @@
+start
@@ -2,0 +3,1 @@
+end
)",
        std::pmr::get_default_resource()
    );

    ASSERT_TRUE(result);
    EXPECT_EQ(result.value(), u8"start\na\nb\nend\n");
}

TEST(Diff, unified_diff_rejects_bad_header)
{
    const auto result
        = parse_unified_diff(u8"@@ -x,1 +1,1 @@\n-a\n+a\n", std::pmr::get_default_resource());

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, Unified_Diff_Error_Kind::invalid_hunk_header);
    EXPECT_EQ(result.error().span.begin, 0);
    EXPECT_EQ(result.error().message, u8"Invalid unified diff hunk header.");
}

TEST(Diff, unified_diff_rejects_bad_body)
{
    const auto result
        = parse_unified_diff(u8"@@ -1,1 +1,1 @@\n?a\n", std::pmr::get_default_resource());

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, Unified_Diff_Error_Kind::invalid_hunk_line);
    EXPECT_EQ(result.error().span.line, 1);
}

TEST(Diff, unified_diff_rejects_bad_counts)
{
    const auto result
        = parse_unified_diff(u8"@@ -1,2 +1,1 @@\n-a\n+a\n", std::pmr::get_default_resource());

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, Unified_Diff_Error_Kind::hunk_count_mismatch);
}

TEST(Diff, unified_diff_rejects_bad_line_numbers)
{
    const auto result = apply_unified_diff(
        u8"a\n", u8"@@ -3,1 +3,1 @@\n-a\n+b\n", std::pmr::get_default_resource()
    );

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, Unified_Diff_Error_Kind::hunk_out_of_range);
    EXPECT_EQ(result.error().span.begin, 0);
}

TEST(Diff, unified_diff_rejects_missing_source_newline)
{
    const auto result = apply_unified_diff(u8"one", {}, std::pmr::get_default_resource());

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, Unified_Diff_Error_Kind::source_missing_trailing_newline);
}

TEST(Diff, unified_diff_rejects_source_mismatch)
{
    const auto result = apply_unified_diff(
        u8"a\n", u8"@@ -1,1 +1,1 @@\n-old\n+new\n", std::pmr::get_default_resource()
    );

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, Unified_Diff_Error_Kind::source_mismatch);
    EXPECT_EQ(result.error().message, u8"Unified diff context does not match the source code.");
}

TEST(Diff, unified_diff_rejects_overlapping_hunks)
{
    const auto result = apply_unified_diff(
        u8"a\nb\n", u8"@@ -1,1 +1,1 @@\n-a\n+A\n@@ -1,1 +1,1 @@\n-b\n+B\n",
        std::pmr::get_default_resource()
    );

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, Unified_Diff_Error_Kind::hunk_out_of_order);
}

} // namespace
} // namespace cowel
