#include <ostream>
#include <string_view>

#include <gtest/gtest.h>

#include "cowel/util/math.hpp"

#include "cowel/big_int.hpp"
#include "cowel/big_int_ops.hpp"

namespace cowel {

// NOLINTNEXTLINE(misc-use-internal-linkage)
std::ostream& operator<<(std::ostream&, const Big_Int&);

std::ostream& operator<<(std::ostream& out, const Big_Int& x)
{
    x.print_to([&](const std::string_view string) { out << string; });
    return out;
}

namespace {

// TODO: put this in a test file
static_assert(twos_width(-4) == 3);
static_assert(twos_width(-3) == 3);
static_assert(twos_width(-2) == 2);
static_assert(twos_width(-1) == 1);
static_assert(twos_width(0) == 1);
static_assert(twos_width(1) == 2);
static_assert(twos_width(2) == 3);
static_assert(twos_width(3) == 3);
static_assert(twos_width(4) == 4);

static_assert(ones_width(-4) == 4);
static_assert(ones_width(-3) == 3);
static_assert(ones_width(-2) == 3);
static_assert(ones_width(-1) == 2);
static_assert(ones_width(0) == 1);
static_assert(ones_width(1) == 2);
static_assert(ones_width(2) == 3);
static_assert(ones_width(3) == 3);
static_assert(ones_width(4) == 4);

TEST(Big_Int, zero)
{
    Big_Int x;
    EXPECT_EQ(x, 0);
    EXPECT_TRUE(x.is_zero());

    x = Big_Int(0);
    EXPECT_EQ(x, 0);
    EXPECT_TRUE(x.is_zero());

    x = Big_Int(Int128 { 0 });
    EXPECT_EQ(x, Int128 { 0 });
    EXPECT_TRUE(x.is_zero());
}

TEST(Big_Int, from_int)
{
    EXPECT_EQ(Big_Int(123), 123);
    EXPECT_EQ(Big_Int(123ll), 123);
    EXPECT_EQ(Big_Int(123ll), 123);
    EXPECT_EQ(Big_Int(Int128 { 123 }), 123);
    EXPECT_EQ(Big_Int(Int128 { 123 }), Int128 { 123 });

    EXPECT_EQ(Big_Int(-123), -123);
    EXPECT_EQ(Big_Int(-123ll), -123);
    EXPECT_EQ(Big_Int(-123ll), -123);
    EXPECT_EQ(Big_Int(-Int128 { 123 }), -123);
    EXPECT_EQ(Big_Int(-Int128 { 123 }), -Int128 { 123 });

    constexpr auto pow_2_100 = Int128 { 1 } << 100;
    EXPECT_EQ(Big_Int(pow_2_100), pow_2_100);
    EXPECT_EQ(Big_Int(-pow_2_100), -pow_2_100);
}

TEST(Big_Int, to_int)
{
    EXPECT_EQ(Big_Int(123).as_i32(), Conversion_Result(123, false));
    EXPECT_EQ(Big_Int(123).as_i64(), Conversion_Result(123ll, false));
    EXPECT_EQ(Big_Int(123).as_i128(), Conversion_Result(Int128 { 123 }, false));

    const auto pow_2_100 = Big_Int::pow2(100);
    EXPECT_EQ(pow_2_100.as_i32(), Conversion_Result(0, true));
    EXPECT_EQ(pow_2_100.as_i64(), Conversion_Result(0ll, true));
    EXPECT_EQ(pow_2_100.as_i128(), Conversion_Result(Int128 { 1 } << 100, false));

    const auto pow_2_200 = Big_Int::pow2(200);
    EXPECT_EQ(pow_2_200.as_i32(), Conversion_Result(0, true));
    EXPECT_EQ(pow_2_200.as_i64(), Conversion_Result(0ll, true));
    EXPECT_EQ(pow_2_200.as_i128(), Conversion_Result(Int128 { 0 }, true));

    const auto minus_pow_2_200 = -Big_Int::pow2(200);
    EXPECT_EQ(minus_pow_2_200.as_i32(), Conversion_Result(0, true));
    EXPECT_EQ(minus_pow_2_200.as_i64(), Conversion_Result(0ll, true));
    EXPECT_EQ(minus_pow_2_200.as_i128(), Conversion_Result(Int128 { 0 }, true));

    const auto pow_2_200_minus_1 = Big_Int::pow2(200) - 1_n;
    EXPECT_EQ(pow_2_200_minus_1.as_i32(), Conversion_Result(-1, true));
    EXPECT_EQ(pow_2_200_minus_1.as_i64(), Conversion_Result(-1ll, true));
    EXPECT_EQ(pow_2_200_minus_1.as_i128(), Conversion_Result(Int128 { -1 }, true));
}

TEST(Big_Int, parse)
{
    EXPECT_EQ(Big_Int(u8"0"), 0);
    EXPECT_EQ(Big_Int(u8"-0"), 0);

    EXPECT_EQ(Big_Int(u8"1"), 1);
    EXPECT_EQ(Big_Int(u8"-1"), -1);

    EXPECT_EQ(Big_Int(u8"1", 16), 1);
    EXPECT_EQ(Big_Int(u8"-1", 16), -1);

    EXPECT_EQ(Big_Int(u8"11111111", 2), 255);
    EXPECT_EQ(Big_Int(u8"2010", 5), 255);
    EXPECT_EQ(Big_Int(u8"377", 8), 255);
    EXPECT_EQ(Big_Int(u8"255", 10), 255);
    EXPECT_EQ(Big_Int(u8"ff", 16), 255);
    EXPECT_EQ(Big_Int(u8"7v", 32), 255);

    EXPECT_EQ(Big_Int(u8"-11111111", 2), -255);
    EXPECT_EQ(Big_Int(u8"-2010", 5), -255);
    EXPECT_EQ(Big_Int(u8"-377", 8), -255);
    EXPECT_EQ(Big_Int(u8"-255", 10), -255);
    EXPECT_EQ(Big_Int(u8"-7v", 32), -255);

    EXPECT_EQ(
        Big_Int(u8"1606938044258990275541962092341162602522202993782792835301376", 10),
        Big_Int::pow2(200)
    );
    EXPECT_EQ(
        Big_Int(
            u8"1249ad2594c37ceb0b2784c4ce0bf38ace408e211a7caab24308a82e8f1"
            u8"0000000000000000000000000",
            16
        ),
        pow(Big_Int(10), 100)
    );
}

TEST(Big_Int, to_string)
{
    EXPECT_EQ(to_u8string(Big_Int(0)), u8"0");

    EXPECT_EQ(to_u8string(Big_Int(1)), u8"1");
    EXPECT_EQ(to_u8string(Big_Int(-1)), u8"-1");

    EXPECT_EQ(to_u8string(Big_Int(1), 16), u8"1");
    EXPECT_EQ(to_u8string(Big_Int(-1), 16), u8"-1");

    EXPECT_EQ(to_u8string(Big_Int(255), 2), u8"11111111");
    EXPECT_EQ(to_u8string(Big_Int(255), 5), u8"2010");
    EXPECT_EQ(to_u8string(Big_Int(255), 8), u8"377");
    EXPECT_EQ(to_u8string(Big_Int(255), 10), u8"255");
    EXPECT_EQ(to_u8string(Big_Int(255), 16), u8"ff");
    EXPECT_EQ(to_u8string(Big_Int(255), 32), u8"7v");

    EXPECT_EQ(to_u8string(Big_Int(-255), 2), u8"-11111111");
    EXPECT_EQ(to_u8string(Big_Int(-255), 5), u8"-2010");
    EXPECT_EQ(to_u8string(Big_Int(-255), 8), u8"-377");
    EXPECT_EQ(to_u8string(Big_Int(-255), 10), u8"-255");
    EXPECT_EQ(to_u8string(Big_Int(-255), 16), u8"-ff");
    EXPECT_EQ(to_u8string(Big_Int(-255), 32), u8"-7v");

    const auto pow_2_200 = Big_Int::pow2(200);
    const auto minus_pow_2_200 = -pow_2_200;
    const auto pow_10_100 = pow(Big_Int(10), 100);
    const auto minus_pow_10_100 = -pow_10_100;

    // clang-format off
    EXPECT_EQ(
        to_u8string(pow_2_200, 2),
        u8"100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    );
    EXPECT_EQ(
        to_u8string(pow_2_200, 5),
        u8"111020132102420112021343001342032333120043341314112104422342034202402044234211314121001"
    );
    EXPECT_EQ(
        to_u8string(pow_2_200, 8),
        u8"4000000000000000000000000000000000000000000000000000000000000000000"
    );
    EXPECT_EQ(
        to_u8string(pow_2_200, 10),
        u8"1606938044258990275541962092341162602522202993782792835301376"
    );
    EXPECT_EQ(
        to_u8string(pow_2_200, 16),
        u8"100000000000000000000000000000000000000000000000000"
    );
    EXPECT_EQ(
        to_u8string(pow_2_200, 32),
        u8"10000000000000000000000000000000000000000"
    );

    EXPECT_EQ(
        to_u8string(minus_pow_2_200, 2),
        u8"-100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    );
    EXPECT_EQ(
        to_u8string(minus_pow_2_200, 5),
        u8"-111020132102420112021343001342032333120043341314112104422342034202402044234211314121001"
    );
    EXPECT_EQ(
        to_u8string(minus_pow_2_200, 8),
        u8"-4000000000000000000000000000000000000000000000000000000000000000000"
    );
    EXPECT_EQ(
        to_u8string(minus_pow_2_200, 10),
        u8"-1606938044258990275541962092341162602522202993782792835301376"
    );
    EXPECT_EQ(
        to_u8string(minus_pow_2_200, 16),
        u8"-100000000000000000000000000000000000000000000000000"
    );
    EXPECT_EQ(
        to_u8string(minus_pow_2_200, 32),
        u8"-10000000000000000000000000000000000000000"
    );

    EXPECT_EQ(
        to_u8string(pow_10_100, 2),
        u8"100100100100110101101001001011001010011000011011111001110101100001011001001111000010011000100110011100000101111110011100010101100111001000000100011100010000100011010011111001010101010110010010000110000100010101000001011101000111100010000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    );
    EXPECT_EQ(
        to_u8string(pow_10_100, 5),
        u8"102414221203323202133113331031102220100330010000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    );
    EXPECT_EQ(
        to_u8string(pow_10_100, 8),
        u8"444465511312303371654131170230463405763425471004342043237125262206042501350742000000000000000000000000000000000"
    );
    EXPECT_EQ(
        to_u8string(pow_10_100, 10),
        u8"10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    );
    EXPECT_EQ(
        to_u8string(pow_10_100, 16),
        u8"1249ad2594c37ceb0b2784c4ce0bf38ace408e211a7caab24308a82e8f10000000000000000000000000"
    );
    EXPECT_EQ(
        to_u8string(pow_10_100, 32),
        u8"4i9lkip9grstc5if164po5v72me827226jslap462585q7h00000000000000000000"
    );

    EXPECT_EQ(
        to_u8string(minus_pow_10_100, 2),
        u8"-100100100100110101101001001011001010011000011011111001110101100001011001001111000010011000100110011100000101111110011100010101100111001000000100011100010000100011010011111001010101010110010010000110000100010101000001011101000111100010000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    );
    EXPECT_EQ(
        to_u8string(minus_pow_10_100, 5),
        u8"-102414221203323202133113331031102220100330010000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    );
    EXPECT_EQ(
        to_u8string(minus_pow_10_100, 8),
        u8"-444465511312303371654131170230463405763425471004342043237125262206042501350742000000000000000000000000000000000"
    );
    EXPECT_EQ(
        to_u8string(minus_pow_10_100, 10),
        u8"-10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    );
    EXPECT_EQ(
        to_u8string(minus_pow_10_100, 16),
        u8"-1249ad2594c37ceb0b2784c4ce0bf38ace408e211a7caab24308a82e8f10000000000000000000000000"
    );
    EXPECT_EQ(
        to_u8string(minus_pow_10_100, 32),
        u8"-4i9lkip9grstc5if164po5v72me827226jslap462585q7h00000000000000000000"
    );
    // clang-format on
}

} // namespace
} // namespace cowel
