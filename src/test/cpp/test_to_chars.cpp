#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "cowel/util/from_chars.hpp"
#include "cowel/util/to_chars.hpp"

#include "cowel/settings.hpp"

namespace cowel {
namespace {

using namespace std::literals;

TEST(To_Chars, zero)
{
    constexpr Basic_Characters zero = to_characters8(0);
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

TEST(To_Chars, to_chars128)
{
    // 1. Cases where the value fits into 64-bit
    ASSERT_EQ("0"sv, to_characters(Int128(0)).as_string());
    ASSERT_EQ("1"sv, to_characters(Int128(1)).as_string());
    ASSERT_EQ("-1"sv, to_characters(Int128(-1)).as_string());
    ASSERT_EQ("123"sv, to_characters(Int128(123)).as_string());
    ASSERT_EQ("-123"sv, to_characters(Int128(-123)).as_string());

    // 2. Cases where the value does not fit into 64-bit,
    //    but has no more than 19 * 2 decimal digits.
    ASSERT_EQ("18446744073709551616"sv, to_characters(Int128(1) << 64).as_string());
    ASSERT_EQ("-18446744073709551616"sv, to_characters(-(Int128(1) << 64)).as_string());

    // 3. Hardest case: 39 digits, which requires three 64-bit std::to_chars calls.
    ASSERT_EQ(
        "170141183460469231731687303715884105727"sv, //
        to_characters(std::numeric_limits<Int128>::max()).as_string()
    );
    ASSERT_EQ(
        "-170141183460469231731687303715884105728"sv, //
        to_characters(std::numeric_limits<Int128>::min()).as_string()
    );
    ASSERT_EQ(
        "340282366920938463463374607431768211455"sv, //
        to_characters(std::numeric_limits<Uint128>::max()).as_string()
    );
}

TEST(From_Chars, from_chars128_binary)
{
    ASSERT_EQ(Int128(0), from_characters<Int128>("0"sv, 2));
    ASSERT_EQ(Int128(1), from_characters<Int128>("1"sv, 2));
    ASSERT_EQ(Int128(-1), from_characters<Int128>("-1"sv, 2));
    ASSERT_EQ(Int128(123), from_characters<Int128>("1111011"sv, 2));
    ASSERT_EQ(Int128(-123), from_characters<Int128>("-1111011"sv, 2));

    ASSERT_EQ(
        Int128(1) << 64,
        from_characters<Int128>(
            "10000000000000000000000000000000000000000000000000000000000000000"sv, 2
        )
    );
    ASSERT_EQ(
        -(Int128(1) << 64),
        from_characters<Int128>(
            "-10000000000000000000000000000000000000000000000000000000000000000"sv, 2
        )
    );

    ASSERT_EQ(
        std::numeric_limits<Int128>::max(),
        from_characters<Int128>(
            "1111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"sv,
            2
        )
    );
    ASSERT_EQ(
        std::numeric_limits<Int128>::min(),
        from_characters<Int128>(
            "-10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"sv,
            2
        )
    );
    ASSERT_EQ(
        std::numeric_limits<Uint128>::max(),
        from_characters<Uint128>(
            "11111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"sv,
            2
        )
    );
}

TEST(From_Chars, from_chars128_decimal)
{
    // 1. Cases where the value fits into 64-bit
    ASSERT_EQ(Int128(0), from_characters<Int128>("0"sv));
    ASSERT_EQ(Int128(1), from_characters<Int128>("1"sv));
    ASSERT_EQ(Int128(-1), from_characters<Int128>("-1"sv));
    ASSERT_EQ(Int128(123), from_characters<Int128>("123"sv));
    ASSERT_EQ(Int128(-123), from_characters<Int128>("-123"sv));

    // 2. Cases where the value does not fit into 64-bit,
    //    but has no more than 19 * 2 decimal digits.
    ASSERT_EQ(Int128(1) << 64, from_characters<Int128>("18446744073709551616"sv));
    ASSERT_EQ(-(Int128(1) << 64), from_characters<Int128>("-18446744073709551616"sv));

    // 3. Hardest case: 39 digits, which requires three 64-bit std::to_chars calls.
    ASSERT_EQ(
        std::numeric_limits<Int128>::max(),
        from_characters<Int128>("170141183460469231731687303715884105727"sv)
    );
    ASSERT_EQ(
        std::numeric_limits<Int128>::min(),
        from_characters<Int128>("-170141183460469231731687303715884105728"sv)
    );
    ASSERT_EQ(
        std::numeric_limits<Uint128>::max(),
        from_characters<Uint128>("340282366920938463463374607431768211455"sv)
    );
}

TEST(From_Chars, from_chars128_hexadecimal)
{
    ASSERT_EQ(Int128(0), from_characters<Int128>("0"sv, 16));
    ASSERT_EQ(Int128(1), from_characters<Int128>("1"sv, 16));
    ASSERT_EQ(Int128(-1), from_characters<Int128>("-1"sv, 16));
    ASSERT_EQ(Int128(123), from_characters<Int128>("7b"sv, 16));
    ASSERT_EQ(Int128(-123), from_characters<Int128>("-7b"sv, 16));
    ASSERT_EQ(Int128(1ull << 63), from_characters<Int128>("8000000000000000"sv, 16));

    ASSERT_EQ(Int128(1) << 64, from_characters<Int128>("10000000000000000"sv, 16));
    ASSERT_EQ(-(Int128(1) << 64), from_characters<Int128>("-10000000000000000"sv, 16));

    ASSERT_EQ(
        std::numeric_limits<Int128>::max(),
        from_characters<Int128>("7fffffffffffffffffffffffffffffff"sv, 16)
    );
    ASSERT_EQ(
        std::numeric_limits<Int128>::min(),
        from_characters<Int128>("-80000000000000000000000000000000"sv, 16)
    );
    ASSERT_EQ(
        std::numeric_limits<Uint128>::max(),
        from_characters<Uint128>("ffffffffffffffffffffffffffffffff"sv, 16)
    );
}

TEST(From_Chars, from_chars128_too_large)
{
    {
        Uint128 u128;

        const std::from_chars_result bin = from_characters(
            u8"100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"sv,
            u128, 2
        );
        EXPECT_EQ(bin.ec, std::errc::result_out_of_range);

        const std::from_chars_result dec
            = from_characters(u8"340282366920938463463374607431768211456"sv, u128, 10);
        EXPECT_EQ(dec.ec, std::errc::result_out_of_range);

        const std::from_chars_result hex
            = from_characters(u8"100000000000000000000000000000000"sv, u128, 16);
        EXPECT_EQ(hex.ec, std::errc::result_out_of_range);
    }
    {
        Int128 i128;

        const std::from_chars_result bin = from_characters(
            u8"10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"sv,
            i128, 2
        );
        EXPECT_EQ(bin.ec, std::errc::result_out_of_range);

        const std::from_chars_result bin_minus = from_characters(
            u8"-10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001"sv,
            i128, 2
        );
        EXPECT_EQ(bin_minus.ec, std::errc::result_out_of_range);

        const std::from_chars_result dec
            = from_characters(u8"170141183460469231731687303715884105728"sv, i128, 10);
        EXPECT_EQ(dec.ec, std::errc::result_out_of_range);

        const std::from_chars_result dec_minus
            = from_characters(u8"-170141183460469231731687303715884105729"sv, i128, 10);
        EXPECT_EQ(dec_minus.ec, std::errc::result_out_of_range);

        const std::from_chars_result hex
            = from_characters(u8"80000000000000000000000000000000"sv, i128, 16);
        EXPECT_EQ(hex.ec, std::errc::result_out_of_range);

        const std::from_chars_result hex_minus
            = from_characters(u8"-80000000000000000000000000000001"sv, i128, 16);
        EXPECT_EQ(hex_minus.ec, std::errc::result_out_of_range);
    }
}

} // namespace
} // namespace cowel
