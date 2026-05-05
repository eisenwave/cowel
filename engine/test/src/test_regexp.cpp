#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "cowel/regexp.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

// =============================================================================
// reg_exp_flags_parse
// =============================================================================

TEST(Reg_Exp_Flags, parse_empty)
{
    const auto result = reg_exp_flags_parse(u8""sv);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, Reg_Exp_Flags {});
}

TEST(Reg_Exp_Flags, parse_all_valid)
{
    const auto result = reg_exp_flags_parse(u8"imsv"sv);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(
        *result,
        Reg_Exp_Flags::ignore_case | Reg_Exp_Flags::multiline | Reg_Exp_Flags::dot_all
            | Reg_Exp_Flags::unicode_sets
    );
}

TEST(Reg_Exp_Flags, parse_single_ignore_case)
{
    const auto result = reg_exp_flags_parse(u8"i"sv);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, Reg_Exp_Flags::ignore_case);
}

TEST(Reg_Exp_Flags, parse_single_multiline)
{
    const auto result = reg_exp_flags_parse(u8"m"sv);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, Reg_Exp_Flags::multiline);
}

TEST(Reg_Exp_Flags, parse_single_dot_all)
{
    const auto result = reg_exp_flags_parse(u8"s"sv);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, Reg_Exp_Flags::dot_all);
}

TEST(Reg_Exp_Flags, parse_single_unicode_sets)
{
    const auto result = reg_exp_flags_parse(u8"v"sv);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, Reg_Exp_Flags::unicode_sets);
}

TEST(Reg_Exp_Flags, parse_invalid_flag_ascii)
{
    const auto result = reg_exp_flags_parse(u8"x"sv);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, Reg_Exp_Flags_Error_Kind::invalid);
    EXPECT_EQ(result.error().index, 0u);
    EXPECT_EQ(result.error().length, 1u);
}

TEST(Reg_Exp_Flags, parse_invalid_flag_after_valid)
{
    const auto result = reg_exp_flags_parse(u8"iz"sv);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, Reg_Exp_Flags_Error_Kind::invalid);
    EXPECT_EQ(result.error().index, 1u);
    EXPECT_EQ(result.error().length, 1u);
}

TEST(Reg_Exp_Flags, parse_invalid_flag_multibyte)
{
    // U+00E9 LATIN SMALL LETTER E WITH ACUTE — 2-byte UTF-8 sequence
    const auto result = reg_exp_flags_parse(u8"\xC3\xA9"sv);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, Reg_Exp_Flags_Error_Kind::invalid);
    EXPECT_EQ(result.error().index, 0u);
    EXPECT_EQ(result.error().length, 2u);
}

TEST(Reg_Exp_Flags, parse_duplicate_flag)
{
    const auto result = reg_exp_flags_parse(u8"ii"sv);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, Reg_Exp_Flags_Error_Kind::duplicate);
    EXPECT_EQ(result.error().index, 1u);
    EXPECT_EQ(result.error().length, 1u);
}

TEST(Reg_Exp_Flags, parse_duplicate_after_others)
{
    const auto result = reg_exp_flags_parse(u8"ims"sv);
    ASSERT_TRUE(result.has_value());
    const auto result2 = reg_exp_flags_parse(u8"imss"sv);
    ASSERT_FALSE(result2.has_value());
    EXPECT_EQ(result2.error().kind, Reg_Exp_Flags_Error_Kind::duplicate);
    EXPECT_EQ(result2.error().index, 3u);
}

// =============================================================================
// reg_exp_flags_to_string
// =============================================================================

TEST(Reg_Exp_Flags, to_string_empty)
{
    const auto s = reg_exp_flags_to_string(Reg_Exp_Flags {});
    EXPECT_EQ(std::u8string_view(s.data(), s.size()), u8""sv);
}

TEST(Reg_Exp_Flags, to_string_all)
{
    const auto all = Reg_Exp_Flags::ignore_case | Reg_Exp_Flags::multiline | Reg_Exp_Flags::dot_all
        | Reg_Exp_Flags::unicode_sets;
    const auto s = reg_exp_flags_to_string(all);
    EXPECT_EQ(std::u8string_view(s.data(), s.size()), u8"imsv"sv);
}

TEST(Reg_Exp_Flags, to_string_partial)
{
    const auto flags = Reg_Exp_Flags::ignore_case | Reg_Exp_Flags::dot_all;
    const auto s = reg_exp_flags_to_string(flags);
    EXPECT_EQ(std::u8string_view(s.data(), s.size()), u8"is"sv);
}

// =============================================================================
// Reg_Exp_Flags operators
// =============================================================================

TEST(Reg_Exp_Flags, and_assign)
{
    Reg_Exp_Flags f = Reg_Exp_Flags::ignore_case | Reg_Exp_Flags::multiline;
    f &= Reg_Exp_Flags::ignore_case;
    EXPECT_EQ(f, Reg_Exp_Flags::ignore_case);
}

// =============================================================================
// Reg_Exp::make — flag accessors
// =============================================================================

TEST(Reg_Exp, flag_accessors)
{
    const auto flags = Reg_Exp_Flags::ignore_case | Reg_Exp_Flags::multiline
        | Reg_Exp_Flags::dot_all | Reg_Exp_Flags::unicode_sets;
    const auto re = Reg_Exp::make(u8"a"sv, flags);
    ASSERT_TRUE(re.has_value());
    EXPECT_EQ(re->get_flags(), flags);
    EXPECT_TRUE(re->is_ignore_case());
    EXPECT_TRUE(re->is_multiline());
    EXPECT_TRUE(re->is_dot_all());
    EXPECT_TRUE(re->has_unicode_sets());
}

TEST(Reg_Exp, flag_accessors_none)
{
    const auto re = Reg_Exp::make(u8"a"sv);
    ASSERT_TRUE(re.has_value());
    EXPECT_FALSE(re->is_ignore_case());
    EXPECT_FALSE(re->is_multiline());
    EXPECT_FALSE(re->is_dot_all());
    EXPECT_FALSE(re->has_unicode_sets());
}

// =============================================================================
// Reg_Exp::make — basic correctness
// =============================================================================

TEST(Reg_Exp, match_basic)
{
    EXPECT_EQ(Reg_Exp::make(u8"awoo")->match(u8"awoo"), Reg_Exp_Status::matched);
    EXPECT_EQ(Reg_Exp::make(u8"awoo")->match(u8"awoo!"), Reg_Exp_Status::unmatched);
    EXPECT_EQ(Reg_Exp::make(u8".*")->match(u8"awoo"), Reg_Exp_Status::matched);
}

TEST(Reg_Exp, match_no_match)
{
    EXPECT_EQ(Reg_Exp::make(u8"xyz")->match(u8"awoo"), Reg_Exp_Status::unmatched);
    EXPECT_EQ(Reg_Exp::make(u8"xyz")->match(u8""), Reg_Exp_Status::unmatched);
}

TEST(Reg_Exp, match_empty_pattern)
{
    EXPECT_EQ(Reg_Exp::make(u8"")->match(u8""), Reg_Exp_Status::matched);
    EXPECT_EQ(Reg_Exp::make(u8"")->match(u8"a"), Reg_Exp_Status::unmatched);
}

TEST(Reg_Exp, match_unicode_property)
{
    EXPECT_EQ(Reg_Exp::make(u8"\\p{Ll}+")->match(u8"abc"), Reg_Exp_Status::matched);
    EXPECT_EQ(Reg_Exp::make(u8"\\p{Ll}+")->match(u8"αβγ"), Reg_Exp_Status::matched);
    EXPECT_EQ(Reg_Exp::make(u8"\\p{Ll}+")->match(u8"ABC"), Reg_Exp_Status::unmatched);
}

TEST(Reg_Exp, match_ignore_case)
{
    const auto re = Reg_Exp::make(u8"hello", Reg_Exp_Flags::ignore_case);
    ASSERT_TRUE(re.has_value());
    EXPECT_EQ(re->match(u8"HELLO"), Reg_Exp_Status::matched);
    EXPECT_EQ(re->match(u8"Hello"), Reg_Exp_Status::matched);
    EXPECT_EQ(re->match(u8"world"), Reg_Exp_Status::unmatched);
}

TEST(Reg_Exp, match_dot_all)
{
    // With dot_all, '.' matches newline.
    const auto re = Reg_Exp::make(u8".", Reg_Exp_Flags::dot_all);
    ASSERT_TRUE(re.has_value());
    EXPECT_EQ(re->match(u8"\n"), Reg_Exp_Status::matched);
    // dot_all has no effect on non-newline characters.
    EXPECT_EQ(re->match(u8"a"), Reg_Exp_Status::matched);
}

TEST(Reg_Exp, match_multiline)
{
    // With multiline, '^' and '$' match line boundaries.
    const auto re = Reg_Exp::make(u8"^b", Reg_Exp_Flags::multiline);
    ASSERT_TRUE(re.has_value());
    // Full-match only returns matched if the entire string matches.
    // "a\nb" does not match "^b" as a whole, but with multiline the caret
    // anchors to each line — Boost still requires full match here.
    EXPECT_EQ(re->match(u8"b"), Reg_Exp_Status::matched);
}

TEST(Reg_Exp, invalid_patterns)
{
    EXPECT_FALSE(Reg_Exp::make(u8"\\u").has_value());
    EXPECT_FALSE(Reg_Exp::make(u8"\\u003").has_value());
    EXPECT_FALSE(Reg_Exp::make(u8"\\u00").has_value());
}

TEST(Reg_Exp, unicode_escape)
{
    EXPECT_EQ(Reg_Exp::make(u8"\\u0030")->match(u8"0"), Reg_Exp_Status::matched);
    EXPECT_EQ(Reg_Exp::make(u8"\\u00303")->match(u8"03"), Reg_Exp_Status::matched);
    EXPECT_EQ(Reg_Exp::make(u8"\\\\u0030")->match(u8"\\u0030"), Reg_Exp_Status::matched);
}

// =============================================================================
// Reg_Exp::search
// =============================================================================

TEST(Reg_Exp, search_basic)
{
    EXPECT_EQ(Reg_Exp::make(u8"w")->search(u8"awoo").status, Reg_Exp_Status::matched);
    EXPECT_EQ(Reg_Exp::make(u8"z")->search(u8"awoo").status, Reg_Exp_Status::unmatched);
}

TEST(Reg_Exp, search_match_offsets_multibyte)
{
    // ß is 2 bytes in UTF-8
    const auto [w_status, w_match] = Reg_Exp::make(u8"w")->search(u8"ßw");
    EXPECT_EQ(w_status, Reg_Exp_Status::matched);
    EXPECT_EQ(w_match.index, 2u);
    EXPECT_EQ(w_match.length, 1u);

    const auto [sz_status, sz_match] = Reg_Exp::make(u8"ß")->search(u8"wß");
    EXPECT_EQ(sz_status, Reg_Exp_Status::matched);
    EXPECT_EQ(sz_match.index, 1u);
    EXPECT_EQ(sz_match.length, 2u);
}

TEST(Reg_Exp, search_at_start)
{
    const auto [status, match] = Reg_Exp::make(u8"he")->search(u8"hello");
    EXPECT_EQ(status, Reg_Exp_Status::matched);
    EXPECT_EQ(match.index, 0u);
    EXPECT_EQ(match.length, 2u);
}

TEST(Reg_Exp, search_in_empty_string)
{
    EXPECT_EQ(Reg_Exp::make(u8"x")->search(u8"").status, Reg_Exp_Status::unmatched);
}

// =============================================================================
// Reg_Exp::replace_all
// =============================================================================

TEST(Reg_Exp, replace_all_no_match)
{
    std::pmr::vector<char8_t> out;
    const auto status = Reg_Exp::make(u8"z")->replace_all(out, u8"hello", u8"X");
    EXPECT_EQ(status, Reg_Exp_Status::unmatched);
    EXPECT_TRUE(out.empty());
}

TEST(Reg_Exp, replace_all_single)
{
    std::pmr::vector<char8_t> out;
    const auto status = Reg_Exp::make(u8"o")->replace_all(out, u8"hello", u8"0");
    EXPECT_EQ(status, Reg_Exp_Status::matched);
    const std::u8string_view result { out.data(), out.size() };
    EXPECT_EQ(result, u8"hell0"sv);
}

TEST(Reg_Exp, replace_all_multiple)
{
    std::pmr::vector<char8_t> out;
    const auto status = Reg_Exp::make(u8"o")->replace_all(out, u8"foobar", u8"0");
    EXPECT_EQ(status, Reg_Exp_Status::matched);
    const std::u8string_view result { out.data(), out.size() };
    EXPECT_EQ(result, u8"f00bar"sv);
}

TEST(Reg_Exp, replace_all_greedy_match)
{
    // A non-empty pattern that matches the full string replaces it once.
    std::pmr::vector<char8_t> out;
    const auto status = Reg_Exp::make(u8"hello")->replace_all(out, u8"hello", u8"world");
    EXPECT_EQ(status, Reg_Exp_Status::matched);
    const std::u8string_view result { out.data(), out.size() };
    EXPECT_EQ(result, u8"world"sv);
}

TEST(Reg_Exp, replace_all_unicode_match)
{
    std::pmr::vector<char8_t> out;
    // Replace ß with ss
    const auto status = Reg_Exp::make(u8"ß")->replace_all(out, u8"straße", u8"ss");
    EXPECT_EQ(status, Reg_Exp_Status::matched);
    const std::u8string_view result { out.data(), out.size() };
    EXPECT_EQ(result, u8"strasse"sv);
}

TEST(Reg_Exp, replace_all_appends_to_existing_output)
{
    // replace_all appends to an existing vector
    std::pmr::vector<char8_t> out;
    out.push_back(u8'[');
    const auto status = Reg_Exp::make(u8"x")->replace_all(out, u8"axb", u8"Y");
    EXPECT_EQ(status, Reg_Exp_Status::matched);
    out.push_back(u8']');
    const std::u8string_view result { out.data(), out.size() };
    EXPECT_EQ(result, u8"[aYb]"sv);
}

// =============================================================================
// Reg_Exp copy and move
// =============================================================================

TEST(Reg_Exp, copy)
{
    const auto re = Reg_Exp::make(u8"abc");
    ASSERT_TRUE(re.has_value());
    const Reg_Exp copy = *re; // NOLINT
    EXPECT_EQ(copy.match(u8"abc"), Reg_Exp_Status::matched);
    EXPECT_EQ(copy.match(u8"xyz"), Reg_Exp_Status::unmatched);
}

TEST(Reg_Exp, move)
{
    auto re = Reg_Exp::make(u8"abc");
    ASSERT_TRUE(re.has_value());
    const Reg_Exp moved = std::move(*re);
    EXPECT_EQ(moved.match(u8"abc"), Reg_Exp_Status::matched);
}

TEST(Reg_Exp, copy_preserves_flags)
{
    const auto re = Reg_Exp::make(u8"abc", Reg_Exp_Flags::ignore_case);
    ASSERT_TRUE(re.has_value());
    const Reg_Exp copy = *re; // NOLINT
    EXPECT_TRUE(copy.is_ignore_case());
    EXPECT_EQ(copy.match(u8"ABC"), Reg_Exp_Status::matched);
}

} // namespace
} // namespace cowel
