#include <array>
#include <cstddef>
#include <string_view>

#include <gtest/gtest.h>

#include "cowel/util/code_point_names.hpp"
#include "cowel/util/strings.hpp"

namespace cowel {
namespace {

using namespace std::string_view_literals;

TEST(Code_Point_Names, nearest_matches_empty_output_span)
{
    constexpr std::span<Code_Point_Name_Match> out_matches;
    EXPECT_EQ(nearest_matches_for_codepoint_name(u8"LATIN CAPITAL LETTER A"sv, out_matches), 0);
}

TEST(Code_Point_Names, nearest_matches_exact_name_first)
{
    std::array<Code_Point_Name_Match, 4> out_matches {};
    const std::size_t count
        = nearest_matches_for_codepoint_name(u8"LATIN CAPITAL LETTER A"sv, out_matches);

    ASSERT_GE(count, 1);
    EXPECT_EQ(out_matches[0].name, "LATIN CAPITAL LETTER A"sv);
    EXPECT_EQ(out_matches[0].distance, 0);
    EXPECT_EQ(out_matches[0].value, U'A');
}

TEST(Code_Point_Names, nearest_matches_normalizes_case_and_separators)
{
    std::array<Code_Point_Name_Match, 4> out_matches {};
    const std::size_t count
        = nearest_matches_for_codepoint_name(u8"latin-capital_letter a"sv, out_matches);

    ASSERT_GE(count, 1);
    EXPECT_EQ(out_matches[0].name, "LATIN CAPITAL LETTER A"sv);
    EXPECT_EQ(out_matches[0].distance, 0);
    EXPECT_EQ(out_matches[0].value, U'A');
}

TEST(Code_Point_Names, nearest_matches_sorted_by_distance_then_name)
{
    std::array<Code_Point_Name_Match, 24> out_matches {};
    const std::size_t count = nearest_matches_for_codepoint_name(u8"latin letter"sv, out_matches);

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
    const std::size_t count = nearest_matches_for_codepoint_name(u8"snowmn"sv, out_matches);

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

    const std::size_t large_count
        = nearest_matches_for_codepoint_name(u8"greek small"sv, large_matches);
    const std::size_t small_count
        = nearest_matches_for_codepoint_name(u8"greek small"sv, small_matches);

    ASSERT_EQ(large_count, large_matches.size());
    ASSERT_EQ(small_count, small_matches.size());

    for (std::size_t i = 0; i < small_count; ++i) {
        EXPECT_EQ(small_matches[i].name, large_matches[i].name);
        EXPECT_EQ(small_matches[i].distance, large_matches[i].distance);
        EXPECT_EQ(small_matches[i].value, large_matches[i].value);
    }
}

TEST(Code_Point_Names, names_starting_with_empty_output_span)
{
    constexpr std::span<Fixed_String8<96>> out;
    EXPECT_EQ(code_point_names_starting_with(out, u8"DIGIT"), 0);
}

TEST(Code_Point_Names, names_starting_with_rejects_invalid_or_unmatched_prefixes)
{
    std::array<Fixed_String8<96>, 8> out {};
    EXPECT_EQ(code_point_names_starting_with(out, u8""), 0);
    EXPECT_EQ(code_point_names_starting_with(out, u8" "), 0);
    EXPECT_EQ(code_point_names_starting_with(out, u8"digit"), 0);
    EXPECT_EQ(code_point_names_starting_with(out, u8"THIS PREFIX DOES NOT EXIST"), 0);
}

TEST(Code_Point_Names, names_starting_with_finds_database_names)
{
    std::array<Fixed_String8<96>, 16> out {};
    const std::size_t count = code_point_names_starting_with(out, u8"DIGIT Z");
    ASSERT_GE(count, 1);

    bool found_digit_zero = false;
    for (std::size_t i = 0; i < count; ++i) {
        const std::u8string_view name = out[i];
        EXPECT_TRUE(name.starts_with(u8"DIGIT Z"));
        if (name == u8"DIGIT ZERO") {
            found_digit_zero = true;
        }
        EXPECT_NE(code_point_by_name(name), char32_t(-1));
    }
    EXPECT_TRUE(found_digit_zero);
}

TEST(Code_Point_Names, names_starting_with_respects_capacity_and_prefix_stability)
{
    std::array<Fixed_String8<96>, 8> large {};
    std::array<Fixed_String8<96>, 3> small {};

    const std::size_t large_count = code_point_names_starting_with(large, u8"LATIN CAPITAL LETTER ");
    const std::size_t small_count = code_point_names_starting_with(small, u8"LATIN CAPITAL LETTER ");

    ASSERT_EQ(large_count, large.size());
    ASSERT_EQ(small_count, small.size());
    for (std::size_t i = 0; i < small_count; ++i) {
        EXPECT_EQ(small[i], large[i]);
    }
}

TEST(Code_Point_Names, names_starting_with_includes_hangul_programmatic_names)
{
    std::array<Fixed_String8<96>, 8> out {};
    const std::size_t count = code_point_names_starting_with(out, u8"HANGUL SYLLABLE GA");

    ASSERT_GE(count, 1);
    bool found_ga = false;
    for (std::size_t i = 0; i < count; ++i) {
        const std::u8string_view name = out[i];
        EXPECT_TRUE(name.starts_with(u8"HANGUL SYLLABLE GA"));
        if (name == u8"HANGUL SYLLABLE GA") {
            found_ga = true;
        }
        EXPECT_NE(code_point_by_name(name), char32_t(-1));
    }
    EXPECT_TRUE(found_ga);
    EXPECT_EQ(code_point_names_starting_with(out, u8"HANGUL SYLLABLE ZZ"), 0);
}

TEST(Code_Point_Names, names_starting_with_includes_generated_programmatic_names)
{
    std::array<Fixed_String8<96>, 8> out {};
    const std::size_t count = code_point_names_starting_with(out, u8"CJK UNIFIED IDEOGRAPH-340");

    ASSERT_GE(count, 1);
    bool found_3400 = false;
    for (std::size_t i = 0; i < count; ++i) {
        const std::u8string_view name = out[i];
        EXPECT_TRUE(name.starts_with(u8"CJK UNIFIED IDEOGRAPH-340"));
        if (name == u8"CJK UNIFIED IDEOGRAPH-3400") {
            found_3400 = true;
        }
        EXPECT_NE(code_point_by_name(name), char32_t(-1));
    }
    EXPECT_TRUE(found_3400);
    EXPECT_EQ(code_point_names_starting_with(out, u8"CJK UNIFIED IDEOGRAPH-ZZ"), 0);
}

TEST(Code_Point_Names, code_point_by_name_accepts_aliases_from_all_categories)
{
    EXPECT_EQ(code_point_by_name(u8"LATIN CAPITAL LETTER GHA"), U'\u01A2');
    EXPECT_EQ(code_point_by_name(u8"START OF TEXT"), U'\u0002');
    EXPECT_EQ(code_point_by_name(u8"BYTE ORDER MARK"), U'\uFEFF');
    EXPECT_EQ(code_point_by_name(u8"PADDING CHARACTER"), U'\u0080');
    EXPECT_EQ(code_point_by_name(u8"NBSP"), U'\u00A0');
}

TEST(Code_Point_Names, code_point_name_returns_unicode_name)
{
    EXPECT_EQ(code_point_name(U'A'), u8"LATIN CAPITAL LETTER A"sv);
}

TEST(Code_Point_Names, code_point_name_empty_for_nameless_code_point)
{
    // Control characters have no official Unicode name.
    EXPECT_TRUE(code_point_name(U'\0').empty());
}

TEST(Code_Point_Names, code_point_display_name_returns_official_name)
{
    // Regular named code point: display name is the official Unicode name.
    EXPECT_EQ(code_point_display_name(U'A'), u8"LATIN CAPITAL LETTER A"sv);
}

TEST(Code_Point_Names, code_point_display_name_prefers_correction_alias_over_name)
{
    // U+2118 has the official (incorrect) name "SCRIPT CAPITAL P" and the
    // correction alias "WEIERSTRASS ELLIPTIC FUNCTION", which takes precedence.
    EXPECT_EQ(code_point_display_name(U'\u2118'), u8"WEIERSTRASS ELLIPTIC FUNCTION"sv);
}

TEST(Code_Point_Names, code_point_display_name_falls_back_to_control_alias)
{
    // U+0000 has no official Unicode name; its control alias is "NULL".
    EXPECT_EQ(code_point_display_name(0), u8"NULL"sv);
}

TEST(Code_Point_Names, code_point_display_name_empty_for_unnamed_unaliased_code_point)
{
    // U+FFFF is a noncharacter with no name and no aliases of any kind.
    EXPECT_TRUE(code_point_display_name(U'\uFFFF').empty());
}

TEST(Code_Point_Names, code_point_display_name_returns_single_name_for_multi_alias_code_point)
{
    // U+000A has multiple control aliases ("LINE FEED (LF)", "NEW LINE (NL)",
    // "END OF LINE (EOL)"); the function must return exactly one, not a
    // comma-separated list.
    const auto name = code_point_display_name(char32_t(0x000A));
    EXPECT_FALSE(name.empty());
    EXPECT_EQ(std::u8string_view(name).find(u8','), std::u8string_view::npos);
}

} // namespace
} // namespace cowel
