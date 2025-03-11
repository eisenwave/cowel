#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "mmml/util/to_chars.hpp"

namespace mmml {
namespace {

using namespace std::literals;

TEST(To_Chars, zero)
{
    Basic_Characters zero = to_characters8(0);
    ASSERT_EQ(zero, u8"0"sv);
}

TEST(To_Chars, small_numbers)
{
    for (int i = -1000; i <= 1000; ++i) {
        const std::string expected = std::to_string(i);
        const auto actual = to_characters(i);
        ASSERT_EQ(std::string_view(expected), std::string_view(actual));
    }
}

} // namespace
} // namespace mmml
