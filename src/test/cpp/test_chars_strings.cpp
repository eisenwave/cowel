#include <gtest/gtest.h>

#include "mmml/util/chars.hpp"
#include "mmml/util/strings.hpp"

namespace mmml {
namespace {

TEST(Chars, is_ascii_digit)
{
    EXPECT_FALSE(is_ascii_digit(u8'a'));
    for (char8_t digit = u8'0'; digit <= u8'9'; ++digit) {
        EXPECT_TRUE(is_ascii_digit(u8'0'));
    }

    EXPECT_FALSE(is_ascii_digit(U'a'));
    for (char32_t digit = U'0'; digit <= U'9'; ++digit) {
        EXPECT_TRUE(is_ascii_digit(U'0'));
    }
}

TEST(Chars, is_ascii_lower_alpha)
{
    for (char8_t c : all_ascii_lower_alpha8) {
        EXPECT_TRUE(is_ascii_lower_alpha(c));
    }
    for (char8_t c : all_ascii_upper_alpha8) {
        EXPECT_FALSE(is_ascii_lower_alpha(c));
    }
    for (char8_t c : all_ascii_digit8) {
        EXPECT_FALSE(is_ascii_lower_alpha(c));
    }

    for (char32_t c : all_ascii_lower_alpha) {
        EXPECT_TRUE(is_ascii_lower_alpha(c));
    }
    for (char32_t c : all_ascii_upper_alpha) {
        EXPECT_FALSE(is_ascii_lower_alpha(c));
    }
    for (char32_t c : all_ascii_digit) {
        EXPECT_FALSE(is_ascii_lower_alpha(c));
    }
}

TEST(Chars, is_ascii_upper_alpha)
{
    for (char8_t c : all_ascii_lower_alpha8) {
        EXPECT_FALSE(is_ascii_upper_alpha(c));
    }
    for (char8_t c : all_ascii_upper_alpha8) {
        EXPECT_TRUE(is_ascii_upper_alpha(c));
    }
    for (char8_t c : all_ascii_digit8) {
        EXPECT_FALSE(is_ascii_upper_alpha(c));
    }

    for (char32_t c : all_ascii_lower_alpha) {
        EXPECT_FALSE(is_ascii_upper_alpha(c));
    }
    for (char32_t c : all_ascii_upper_alpha) {
        EXPECT_TRUE(is_ascii_upper_alpha(c));
    }
    for (char32_t c : all_ascii_digit) {
        EXPECT_FALSE(is_ascii_upper_alpha(c));
    }
}

TEST(Chars, is_ascii_alpha)
{
    for (char8_t c : all_ascii_alpha8) {
        EXPECT_TRUE(is_ascii_alpha(c));
    }
    for (char8_t c : all_ascii_digit8) {
        EXPECT_FALSE(is_ascii_upper_alpha(c));
    }

    for (char32_t c : all_ascii_alpha) {
        EXPECT_TRUE(is_ascii_alpha(c));
    }
    for (char32_t c : all_ascii_digit) {
        EXPECT_FALSE(is_ascii_upper_alpha(c));
    }
}

TEST(Chars, is_mmml_directive_name_character)
{
    for (char32_t c : all_mmml_special) {
        EXPECT_FALSE(is_mmml_directive_name_character(c));
    }
    for (char32_t c : all_ascii_alpha) {
        EXPECT_TRUE(is_mmml_directive_name_character(c));
    }
    for (char32_t c : all_ascii_digit) {
        EXPECT_TRUE(is_mmml_directive_name_character(c));
    }
}

} // namespace
} // namespace mmml
