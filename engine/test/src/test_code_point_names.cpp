#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "cowel/util/code_point_names.hpp"
#include "cowel/util/strings.hpp"

namespace cowel {
namespace {

TEST(Code_Point_Names, nearest_matches_empty_output_span)
{
    constexpr std::span<Code_Point_Name_Match> out_matches;
    EXPECT_EQ(nearest_matches_for_codepoint_name(u8"LATIN CAPITAL LETTER A", out_matches), 0);
}

TEST(Code_Point_Names, nearest_matches_exact_name_first)
{
    std::array<Code_Point_Name_Match, 4> out_matches {};
    const std::size_t count
        = nearest_matches_for_codepoint_name(u8"LATIN CAPITAL LETTER A", out_matches);

    ASSERT_GE(count, 1);
    EXPECT_EQ(out_matches[0].name, "LATIN CAPITAL LETTER A");
    EXPECT_EQ(out_matches[0].distance, 0);
    EXPECT_EQ(out_matches[0].value, U'A');
}

TEST(Code_Point_Names, nearest_matches_normalizes_case_and_separators)
{
    std::array<Code_Point_Name_Match, 4> out_matches {};
    const std::size_t count
        = nearest_matches_for_codepoint_name(u8"latin-capital_letter a", out_matches);

    ASSERT_GE(count, 1);
    EXPECT_EQ(out_matches[0].name, "LATIN CAPITAL LETTER A");
    EXPECT_EQ(out_matches[0].distance, 0);
    EXPECT_EQ(out_matches[0].value, U'A');
}

TEST(Code_Point_Names, nearest_matches_sorted_by_distance_then_name)
{
    std::array<Code_Point_Name_Match, 24> out_matches {};
    const std::size_t count = nearest_matches_for_codepoint_name(u8"latin letter", out_matches);

    ASSERT_EQ(count, out_matches.size());
    for (std::size_t i = 1; i < count; ++i) {
        const Code_Point_Name_Match& previous = out_matches[i - 1];
        const Code_Point_Name_Match& current = out_matches[i];

        EXPECT_TRUE(previous.distance <= current.distance);
        if (previous.distance == current.distance) {
            EXPECT_TRUE(previous.name < current.name);
        }
    }
}

TEST(Code_Point_Names, nearest_matches_values_round_trip_with_exact_lookup)
{
    std::array<Code_Point_Name_Match, 12> out_matches {};
    const std::size_t count = nearest_matches_for_codepoint_name(u8"snowmn", out_matches);

    ASSERT_EQ(count, out_matches.size());
    for (std::size_t i = 0; i < count; ++i) {
        const std::u8string_view name = as_u8string_view(std::string_view { out_matches[i].name });
        EXPECT_EQ(code_point_by_name(name), out_matches[i].value);
    }
}

TEST(Code_Point_Names, nearest_matches_capacity_limits_and_prefix_is_stable)
{
    std::array<Code_Point_Name_Match, 6> large_matches {};
    std::array<Code_Point_Name_Match, 3> small_matches {};

    const std::size_t large_count = nearest_matches_for_codepoint_name(u8"greek small", large_matches);
    const std::size_t small_count = nearest_matches_for_codepoint_name(u8"greek small", small_matches);

    ASSERT_EQ(large_count, large_matches.size());
    ASSERT_EQ(small_count, small_matches.size());

    for (std::size_t i = 0; i < small_count; ++i) {
        EXPECT_EQ(small_matches[i].name, large_matches[i].name);
        EXPECT_EQ(small_matches[i].distance, large_matches[i].distance);
        EXPECT_EQ(small_matches[i].value, large_matches[i].value);
    }
}

} // namespace
} // namespace cowel
