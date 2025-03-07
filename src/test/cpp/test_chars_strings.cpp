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

} // namespace
} // namespace mmml
