#include <cstddef>
#include <gtest/gtest.h>
#include <memory_resource>
#include <span>
#include <string_view>

#include "mmml/util/typo.hpp"

namespace mmml {
namespace {

TEST(Typo, empty)
{
    constexpr std::span<std::u8string_view> haystack;
    constexpr std::u8string_view needle = u8"abc";

    std::pmr::monotonic_buffer_resource memory;

    constexpr Distant<std::size_t> expected {};
    EXPECT_FALSE(expected);

    const Distant actual = closest_match(haystack, needle, &memory);
    EXPECT_FALSE(expected);

    EXPECT_EQ(expected, actual);
}

TEST(Typo, exact_match)
{
    constexpr std::u8string_view haystack[] { u8"abc", u8"12345", u8"xyz" };
    constexpr std::u8string_view needle = u8"12345";

    std::pmr::monotonic_buffer_resource memory;

    constexpr Distant<std::size_t> expected { .value = 1, .distance = 0 };
    const Distant<std::size_t> actual = closest_match(haystack, needle, &memory);
    EXPECT_EQ(expected, actual);
}

TEST(Typo, fuzzy_match)
{
    constexpr std::u8string_view haystack[] { u8"abc", u8"12345", u8"xyz" };
    constexpr std::u8string_view needle = u8"1234";

    std::pmr::monotonic_buffer_resource memory;

    constexpr Distant<std::size_t> expected { .value = 1, .distance = 1 };
    const Distant<std::size_t> actual = closest_match(haystack, needle, &memory);
    EXPECT_EQ(expected, actual);
}

} // namespace
} // namespace mmml
