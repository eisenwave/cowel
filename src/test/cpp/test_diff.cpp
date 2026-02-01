#include <array>
#include <string_view>

#include <gtest/gtest.h>

#include "cowel/util/diff.hpp"

namespace cowel {
namespace {

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

} // namespace
} // namespace cowel
