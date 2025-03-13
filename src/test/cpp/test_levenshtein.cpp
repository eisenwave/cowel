#include <cstddef>
#include <memory_resource>
#include <random>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "mmml/util/levenshtein_utf8.hpp"
#include "mmml/util/unicode.hpp"

namespace mmml {
namespace {

TEST(Levenshtein, empty)
{
    constexpr std::u8string_view x;
    constexpr std::u8string_view y;
    std::pmr::monotonic_buffer_resource memory;

    EXPECT_EQ(code_unit_levenshtein_distance(x, y, &memory), 0);
}

TEST(Levenshtein, create)
{
    constexpr std::u8string_view x;
    constexpr std::u8string_view y = u8"abcdefg";
    std::pmr::monotonic_buffer_resource memory;

    EXPECT_EQ(code_unit_levenshtein_distance(x, y, &memory), 7);
}

TEST(Levenshtein, zero_distance)
{
    constexpr std::u8string_view x = u8"abcdefg";
    constexpr std::u8string_view y = u8"abcdefg";
    std::pmr::monotonic_buffer_resource memory;

    EXPECT_EQ(code_unit_levenshtein_distance(x, y, &memory), 0);
}

TEST(Levenshtein, pure_prepend)
{
    constexpr std::u8string_view x = u8"abc";
    constexpr std::u8string_view y = u8"12345abc";
    std::pmr::monotonic_buffer_resource memory;

    EXPECT_EQ(code_unit_levenshtein_distance(x, y, &memory), 5);
}

TEST(Levenshtein, pure_append)
{
    constexpr std::u8string_view x = u8"abc";
    constexpr std::u8string_view y = u8"abc12345";
    std::pmr::monotonic_buffer_resource memory;

    EXPECT_EQ(code_unit_levenshtein_distance(x, y, &memory), 5);
}

TEST(Levenshtein, insert)
{
    constexpr std::u8string_view x = u8"abcd";
    constexpr std::u8string_view y = u8"a1b2c3d";
    std::pmr::monotonic_buffer_resource memory;

    EXPECT_EQ(code_unit_levenshtein_distance(x, y, &memory), 3);
}

TEST(Levenshtein, utf8)
{
    constexpr std::u8string_view empty;
    constexpr std::u8string_view stuff = u8"∮ E⋅da = Q,  n → ∞, ∑ f(i) = ∏ g(i)";
    constexpr std::size_t expected = utf8::code_points_unchecked(stuff);

    static_assert(stuff.size() == 47);
    static_assert(expected == 35);

    std::pmr::monotonic_buffer_resource memory;
    const std::size_t actual = code_point_levenshtein_distance(empty, stuff, &memory);
    ASSERT_EQ(expected, actual);
}

// Verifies that for ASCII strings,
// computing distances between code points and code units is equivalent,
// and that distance computations is commutative.
TEST(Levenshtein, ascii_commutative_fuzzing)
{
    constexpr int iterations = 100;

    std::pmr::monotonic_buffer_resource memory;

    std::default_random_engine rng { 12345 };
    std::uniform_int_distribution<unsigned> distr { 0, 127 };

    for (int i = 0; i < iterations; ++i) {
        memory.release();

        std::pmr::u8string x { &memory };
        std::pmr::u8string y { &memory };

        const unsigned x_size = distr(rng);
        const unsigned y_size = distr(rng);
        x.resize(x_size);
        y.resize(y_size);

        for (char8_t& c : x) {
            c = char8_t(distr(rng));
        }
        for (char8_t& c : y) {
            c = char8_t(distr(rng));
        }

        const auto xy_code_unit = code_unit_levenshtein_distance(x, y, &memory);
        const auto xy_code_point = code_point_levenshtein_distance(x, y, &memory);
        const auto yx_code_unit = code_unit_levenshtein_distance(y, x, &memory);
        const auto yx_code_point = code_point_levenshtein_distance(y, x, &memory);

        EXPECT_EQ(xy_code_unit, xy_code_point);
        EXPECT_EQ(xy_code_point, yx_code_unit);
        EXPECT_EQ(yx_code_unit, yx_code_point);
    }
}

} // namespace
} // namespace mmml
