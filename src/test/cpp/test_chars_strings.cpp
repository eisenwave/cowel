#include <gtest/gtest.h>

#include "mmml/util/chars.hpp"

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
    EXPECT_TRUE(is_ascii_lower_alpha(u8'a'));
    EXPECT_TRUE(is_ascii_lower_alpha(u8'c'));
    EXPECT_TRUE(is_ascii_lower_alpha(u8'z'));
    EXPECT_FALSE(is_ascii_lower_alpha(u8'A'));
    EXPECT_FALSE(is_ascii_lower_alpha(u8'C'));
    EXPECT_FALSE(is_ascii_lower_alpha(u8'Z'));
    EXPECT_FALSE(is_ascii_lower_alpha(u8'0'));

    EXPECT_TRUE(is_ascii_lower_alpha(U'a'));
    EXPECT_TRUE(is_ascii_lower_alpha(U'c'));
    EXPECT_TRUE(is_ascii_lower_alpha(U'z'));
    EXPECT_FALSE(is_ascii_lower_alpha(U'A'));
    EXPECT_FALSE(is_ascii_lower_alpha(U'C'));
    EXPECT_FALSE(is_ascii_lower_alpha(U'Z'));
    EXPECT_FALSE(is_ascii_lower_alpha(U'0'));
}

TEST(Chars, is_ascii_upper_alpha)
{
    EXPECT_TRUE(is_ascii_upper_alpha(u8'A'));
    EXPECT_TRUE(is_ascii_upper_alpha(u8'C'));
    EXPECT_TRUE(is_ascii_upper_alpha(u8'Z'));
    EXPECT_FALSE(is_ascii_upper_alpha(u8'z'));
    EXPECT_FALSE(is_ascii_upper_alpha(u8'c'));
    EXPECT_FALSE(is_ascii_upper_alpha(u8'a'));
    EXPECT_FALSE(is_ascii_upper_alpha(u8'0'));

    EXPECT_TRUE(is_ascii_upper_alpha(U'A'));
    EXPECT_TRUE(is_ascii_upper_alpha(U'C'));
    EXPECT_TRUE(is_ascii_upper_alpha(U'Z'));
    EXPECT_FALSE(is_ascii_upper_alpha(U'a'));
    EXPECT_FALSE(is_ascii_upper_alpha(U'c'));
    EXPECT_FALSE(is_ascii_upper_alpha(U'z'));
    EXPECT_FALSE(is_ascii_upper_alpha(U'0'));
}

TEST(Chars, is_ascii_alpha)
{
    EXPECT_TRUE(is_ascii_alpha(u8'a'));
    EXPECT_TRUE(is_ascii_alpha(u8'c'));
    EXPECT_TRUE(is_ascii_alpha(u8'z'));
    EXPECT_TRUE(is_ascii_alpha(u8'A'));
    EXPECT_TRUE(is_ascii_alpha(u8'C'));
    EXPECT_TRUE(is_ascii_alpha(u8'Z'));
    EXPECT_FALSE(is_ascii_alpha(u8'0'));

    EXPECT_TRUE(is_ascii_alpha(U'a'));
    EXPECT_TRUE(is_ascii_alpha(U'c'));
    EXPECT_TRUE(is_ascii_alpha(U'z'));
    EXPECT_TRUE(is_ascii_alpha(U'A'));
    EXPECT_TRUE(is_ascii_alpha(U'C'));
    EXPECT_TRUE(is_ascii_alpha(U'Z'));
    EXPECT_FALSE(is_ascii_alpha(U'0'));
}

TEST(Chars, is_mmml_directive_name_character)
{
    EXPECT_FALSE(is_mmml_directive_name_character(U'\\'));
    EXPECT_FALSE(is_mmml_directive_name_character(U'{'));
    EXPECT_FALSE(is_mmml_directive_name_character(U'}'));
    EXPECT_FALSE(is_mmml_directive_name_character(U','));
    EXPECT_FALSE(is_mmml_directive_name_character(U'['));
    EXPECT_FALSE(is_mmml_directive_name_character(U']'));

    EXPECT_TRUE(is_mmml_directive_name_character(U'a'));
    EXPECT_TRUE(is_mmml_directive_name_character(U'c'));
    EXPECT_TRUE(is_mmml_directive_name_character(U'z'));
    EXPECT_TRUE(is_mmml_directive_name_character(U'A'));
    EXPECT_TRUE(is_mmml_directive_name_character(U'C'));
    EXPECT_TRUE(is_mmml_directive_name_character(U'Z'));
}

} // namespace
} // namespace mmml
