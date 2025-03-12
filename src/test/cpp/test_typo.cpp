#include <gtest/gtest.h>
#include <memory_resource>
#include <string_view>

#include "mmml/util/typo.hpp"

namespace mmml {
namespace {

TEST(Typo, empty)
{
    constexpr std::span<std::u8string_view> haystack;
    constexpr std::u8string_view needle = u8"abc";

    std::pmr::monotonic_buffer_resource memory;

    constexpr Typo_Result expected {};
    EXPECT_FALSE(expected);

    const Typo_Result actual = closest_match(haystack, needle, &memory);
    EXPECT_FALSE(expected);

    EXPECT_EQ(expected, actual);
}

} // namespace
} // namespace mmml
