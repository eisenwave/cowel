#include <ostream>
#include <string_view>

#include <gtest/gtest.h>

#include "cowel/util/math.hpp"

#include "cowel/big_int.hpp"
#include "cowel/big_int_ops.hpp"

namespace cowel {

using namespace std::string_view_literals;

// NOLINTNEXTLINE(misc-use-internal-linkage)
std::ostream& operator<<(std::ostream&, const Big_Int&);

std::ostream& operator<<(std::ostream& out, const Big_Int& x)
{
    x.print_to([&](const std::string_view string) { out << string; });
    return out;
}

namespace {

TEST(Int128, division_sanity)
{
    for (Int128 dividend = -10; dividend < 10; ++dividend) {
        for (Int128 divisor = -10; divisor < 10; ++divisor) {
            if (divisor == 0) {
                continue;
            }
            const auto [q_to_zero, r_to_zero] = div_rem_to_zero(dividend, divisor);
            EXPECT_EQ(q_to_zero, dividend / divisor);
            EXPECT_EQ(r_to_zero, dividend % divisor);

            const auto [q_to_pos_inf, r_to_pos_inf] = div_rem_to_pos_inf(dividend, divisor);
            EXPECT_EQ(q_to_pos_inf, div_to_pos_inf(dividend, divisor));
            EXPECT_EQ(r_to_pos_inf, rem_to_pos_inf(dividend, divisor));

            const auto [q_to_neg_inf, r_to_neg_inf] = div_rem_to_neg_inf(dividend, divisor);
            EXPECT_EQ(q_to_neg_inf, div_to_neg_inf(dividend, divisor));
            EXPECT_EQ(r_to_neg_inf, rem_to_neg_inf(dividend, divisor));
        }
    }
}

TEST(Int128, div_to_pos_inf_small)
{
    EXPECT_EQ(div_to_pos_inf(Int128(-2), Int128(-2)), 1);
    EXPECT_EQ(div_to_pos_inf(Int128(-2), Int128(-1)), 2);
    EXPECT_EQ(div_to_pos_inf(Int128(-2), Int128(1)), -2);
    EXPECT_EQ(div_to_pos_inf(Int128(-2), Int128(2)), -1);

    EXPECT_EQ(div_to_pos_inf(Int128(-1), Int128(-2)), 1);
    EXPECT_EQ(div_to_pos_inf(Int128(-1), Int128(-1)), 1);
    EXPECT_EQ(div_to_pos_inf(Int128(-1), Int128(1)), -1);
    EXPECT_EQ(div_to_pos_inf(Int128(-1), Int128(2)), 0);

    EXPECT_EQ(div_to_pos_inf(Int128(0), Int128(-2)), 0);
    EXPECT_EQ(div_to_pos_inf(Int128(0), Int128(-1)), 0);
    EXPECT_EQ(div_to_pos_inf(Int128(0), Int128(1)), 0);
    EXPECT_EQ(div_to_pos_inf(Int128(0), Int128(2)), 0);

    EXPECT_EQ(div_to_pos_inf(Int128(1), Int128(-2)), 0);
    EXPECT_EQ(div_to_pos_inf(Int128(1), Int128(-1)), -1);
    EXPECT_EQ(div_to_pos_inf(Int128(1), Int128(1)), 1);
    EXPECT_EQ(div_to_pos_inf(Int128(1), Int128(2)), 1);

    EXPECT_EQ(div_to_pos_inf(Int128(2), Int128(-2)), -1);
    EXPECT_EQ(div_to_pos_inf(Int128(2), Int128(-1)), -2);
    EXPECT_EQ(div_to_pos_inf(Int128(2), Int128(1)), 2);
    EXPECT_EQ(div_to_pos_inf(Int128(2), Int128(2)), 1);
}

TEST(Int128, div_to_neg_inf_small)
{
    EXPECT_EQ(div_to_neg_inf(Int128(-2), Int128(-2)), 1);
    EXPECT_EQ(div_to_neg_inf(Int128(-2), Int128(-1)), 2);
    EXPECT_EQ(div_to_neg_inf(Int128(-2), Int128(1)), -2);
    EXPECT_EQ(div_to_neg_inf(Int128(-2), Int128(2)), -1);

    EXPECT_EQ(div_to_neg_inf(Int128(-1), Int128(-2)), 0);
    EXPECT_EQ(div_to_neg_inf(Int128(-1), Int128(-1)), 1);
    EXPECT_EQ(div_to_neg_inf(Int128(-1), Int128(1)), -1);
    EXPECT_EQ(div_to_neg_inf(Int128(-1), Int128(2)), -1);

    EXPECT_EQ(div_to_neg_inf(Int128(0), Int128(-2)), 0);
    EXPECT_EQ(div_to_neg_inf(Int128(0), Int128(-1)), 0);
    EXPECT_EQ(div_to_neg_inf(Int128(0), Int128(1)), 0);
    EXPECT_EQ(div_to_neg_inf(Int128(0), Int128(2)), 0);

    EXPECT_EQ(div_to_neg_inf(Int128(1), Int128(-2)), -1);
    EXPECT_EQ(div_to_neg_inf(Int128(1), Int128(-1)), -1);
    EXPECT_EQ(div_to_neg_inf(Int128(1), Int128(1)), 1);
    EXPECT_EQ(div_to_neg_inf(Int128(1), Int128(2)), 0);

    EXPECT_EQ(div_to_neg_inf(Int128(2), Int128(-2)), -1);
    EXPECT_EQ(div_to_neg_inf(Int128(2), Int128(-1)), -2);
    EXPECT_EQ(div_to_neg_inf(Int128(2), Int128(1)), 2);
    EXPECT_EQ(div_to_neg_inf(Int128(2), Int128(2)), 1);
}

TEST(Int128, rem_to_pos_inf_small)
{
    EXPECT_EQ(rem_to_pos_inf(Int128(-2), Int128(-2)), 0);
    EXPECT_EQ(rem_to_pos_inf(Int128(-2), Int128(-1)), 0);
    EXPECT_EQ(rem_to_pos_inf(Int128(-2), Int128(1)), 0);
    EXPECT_EQ(rem_to_pos_inf(Int128(-2), Int128(2)), 0);

    EXPECT_EQ(rem_to_pos_inf(Int128(-1), Int128(-2)), 1);
    EXPECT_EQ(rem_to_pos_inf(Int128(-1), Int128(-1)), 0);
    EXPECT_EQ(rem_to_pos_inf(Int128(-1), Int128(1)), 0);
    EXPECT_EQ(rem_to_pos_inf(Int128(-1), Int128(2)), -1);

    EXPECT_EQ(rem_to_pos_inf(Int128(0), Int128(-2)), 0);
    EXPECT_EQ(rem_to_pos_inf(Int128(0), Int128(-1)), 0);
    EXPECT_EQ(rem_to_pos_inf(Int128(0), Int128(1)), 0);
    EXPECT_EQ(rem_to_pos_inf(Int128(0), Int128(2)), 0);

    EXPECT_EQ(rem_to_pos_inf(Int128(1), Int128(-2)), 1);
    EXPECT_EQ(rem_to_pos_inf(Int128(1), Int128(-1)), 0);
    EXPECT_EQ(rem_to_pos_inf(Int128(1), Int128(1)), 0);
    EXPECT_EQ(rem_to_pos_inf(Int128(1), Int128(2)), -1);

    EXPECT_EQ(rem_to_pos_inf(Int128(2), Int128(-2)), 0);
    EXPECT_EQ(rem_to_pos_inf(Int128(2), Int128(-1)), 0);
    EXPECT_EQ(rem_to_pos_inf(Int128(2), Int128(1)), 0);
    EXPECT_EQ(rem_to_pos_inf(Int128(2), Int128(2)), 0);
}

TEST(Int128, rem_to_neg_inf_small)
{
    EXPECT_EQ(rem_to_neg_inf(Int128(-2), Int128(-2)), 0);
    EXPECT_EQ(rem_to_neg_inf(Int128(-2), Int128(-1)), 0);
    EXPECT_EQ(rem_to_neg_inf(Int128(-2), Int128(1)), 0);
    EXPECT_EQ(rem_to_neg_inf(Int128(-2), Int128(2)), 0);

    EXPECT_EQ(rem_to_neg_inf(Int128(-1), Int128(-2)), -1);
    EXPECT_EQ(rem_to_neg_inf(Int128(-1), Int128(-1)), 0);
    EXPECT_EQ(rem_to_neg_inf(Int128(-1), Int128(1)), 0);
    EXPECT_EQ(rem_to_neg_inf(Int128(-1), Int128(2)), 1);

    EXPECT_EQ(rem_to_neg_inf(Int128(0), Int128(-2)), 0);
    EXPECT_EQ(rem_to_neg_inf(Int128(0), Int128(-1)), 0);
    EXPECT_EQ(rem_to_neg_inf(Int128(0), Int128(1)), 0);
    EXPECT_EQ(rem_to_neg_inf(Int128(0), Int128(2)), 0);

    EXPECT_EQ(rem_to_neg_inf(Int128(1), Int128(-2)), -1);
    EXPECT_EQ(rem_to_neg_inf(Int128(1), Int128(-1)), 0);
    EXPECT_EQ(rem_to_neg_inf(Int128(1), Int128(1)), 0);
    EXPECT_EQ(rem_to_neg_inf(Int128(1), Int128(2)), 1);

    EXPECT_EQ(rem_to_neg_inf(Int128(2), Int128(-2)), 0);
    EXPECT_EQ(rem_to_neg_inf(Int128(2), Int128(-1)), 0);
    EXPECT_EQ(rem_to_neg_inf(Int128(2), Int128(1)), 0);
    EXPECT_EQ(rem_to_neg_inf(Int128(2), Int128(2)), 0);
}

TEST(Int128, countl_zero)
{
    EXPECT_EQ(countl_zero(Uint128(0)), 128);
    EXPECT_EQ(countl_zero(Uint128(1)), 127);
    EXPECT_EQ(countl_zero(Uint128(2)), 126);
    EXPECT_EQ(countl_zero(Uint128(3)), 126);
    EXPECT_EQ(countl_zero(Uint128(4)), 125);
    EXPECT_EQ(countl_zero(Uint128(7)), 125);
    EXPECT_EQ(countl_zero(Uint128(8)), 124);
    EXPECT_EQ(countl_zero(Uint128(15)), 124);
    EXPECT_EQ(countl_zero(Uint128(16)), 123);
    EXPECT_EQ(countl_zero(Uint128(255)), 120);
    EXPECT_EQ(countl_zero(Uint128(256)), 119);

    EXPECT_EQ(countl_zero(Uint128(Uint64(-1))), 64);
    EXPECT_EQ(countl_zero(Uint128(1) << 65), 62);
    EXPECT_EQ(countl_zero((Uint128(1) << 100)), 27);
    EXPECT_EQ(countl_zero((Uint128(1) << 64)), 63);

    EXPECT_EQ(countl_zero((Uint128(1) << 127)), 0);
    EXPECT_EQ(countl_zero((Uint128(Uint64(-1)) << 64)), 0);
}

TEST(Int128, countl_one)
{
    EXPECT_EQ(countl_one(Uint128(0)), 0);
    EXPECT_EQ(countl_one(Uint128(1)), 0);
    EXPECT_EQ(countl_one(Uint128(0xffffffffffffffff)), 0);
    EXPECT_EQ(countl_one((Uint128(0xffffffffffffffff) << 64) | Uint128(0)), 64);
    EXPECT_EQ(countl_one((Uint128(0xffffffffffffffff) << 64) | Uint128(0xffffffffffffffff)), 128);
    EXPECT_EQ(countl_one((Uint128(0xfffffffffffffffe) << 64) | Uint128(0xffffffffffffffff)), 63);
    EXPECT_EQ(countl_one(~Uint128(0)), 128);
    EXPECT_EQ(countl_one(~(Uint128(1))), 127);
    EXPECT_EQ(countl_one(~(Uint128(0xff))), 120);
    EXPECT_EQ(countl_one((Uint128(3) << 126)), 2);
    EXPECT_EQ(countl_one((Uint128(1) << 127)), 1);
    EXPECT_EQ(countl_one((Uint128(0xf000000000000000) << 64)), 4);
}

TEST(Int128, twos_width)
{
    EXPECT_EQ(twos_width(-4), 3);
    EXPECT_EQ(twos_width(-3), 3);
    EXPECT_EQ(twos_width(-2), 2);
    EXPECT_EQ(twos_width(-1), 1);
    EXPECT_EQ(twos_width(0), 1);
    EXPECT_EQ(twos_width(1), 2);
    EXPECT_EQ(twos_width(2), 3);
    EXPECT_EQ(twos_width(3), 3);
    EXPECT_EQ(twos_width(4), 4);

    EXPECT_EQ(twos_width(Int128(1) << 126), 128);
    EXPECT_EQ(twos_width(Int128(1) << 127), 128);
}

TEST(Int128, ones_width)
{
    EXPECT_EQ(ones_width(-4), 4);
    EXPECT_EQ(ones_width(-3), 3);
    EXPECT_EQ(ones_width(-2), 3);
    EXPECT_EQ(ones_width(-1), 2);
    EXPECT_EQ(ones_width(0), 1);
    EXPECT_EQ(ones_width(1), 2);
    EXPECT_EQ(ones_width(2), 3);
    EXPECT_EQ(ones_width(3), 3);
    EXPECT_EQ(ones_width(4), 4);

    EXPECT_EQ(ones_width(Int128(1) << 126), 128);
    EXPECT_EQ(ones_width(Int128(1) << 127), 129);
}

TEST(Int128, add_overflow_uint128)
{
    Uint128 result;
    EXPECT_FALSE(add_overflow(result, Uint128(0), Uint128(0)));
    EXPECT_EQ(result, 0);

    EXPECT_FALSE(add_overflow(result, Uint128(1), Uint128(1)));
    EXPECT_EQ(result, 2);

    EXPECT_FALSE(add_overflow(result, Uint128(100), Uint128(200)));
    EXPECT_EQ(result, 300);

    EXPECT_FALSE(add_overflow(result, Uint128(0x7fffffffffffffff), Uint128(0x7fffffffffffffff)));
    EXPECT_EQ(result, Uint128(0xfffffffffffffffe));

    EXPECT_FALSE(add_overflow(result, Uint128(1) << 64, Uint128(1)));
    EXPECT_EQ(result, (Uint128(1) << 64) + 1);

    EXPECT_FALSE(add_overflow(result, ~Uint128(0), Uint128(0)));
    EXPECT_EQ(result, ~Uint128(0));

    EXPECT_FALSE(add_overflow(result, Uint128(0), Uint128(0x7fffffffffffffff)));
    EXPECT_EQ(result, 0x7fffffffffffffff);

    EXPECT_TRUE(add_overflow(result, ~Uint128(0), Uint128(1)));
}

TEST(Int128, add_overflow_int128)
{
    Int128 result;
    EXPECT_FALSE(add_overflow(result, Int128(0), Int128(0)));
    EXPECT_EQ(result, 0);

    EXPECT_FALSE(add_overflow(result, Int128(1), Int128(1)));
    EXPECT_EQ(result, 2);

    EXPECT_FALSE(add_overflow(result, Int128(100), Int128(200)));
    EXPECT_EQ(result, 300);

    EXPECT_FALSE(add_overflow(result, Int128(-100), Int128(-200)));
    EXPECT_EQ(result, -300);

    EXPECT_FALSE(add_overflow(result, Int128(100), Int128(-50)));
    EXPECT_EQ(result, 50);

    EXPECT_FALSE(add_overflow(result, Int128(50), Int128(-100)));
    EXPECT_EQ(result, -50);

    EXPECT_FALSE(add_overflow(result, -(Int128(1) << 126), -(Int128(1) << 126)));
    EXPECT_EQ(result, Int128(Uint128(1) << 127));

    EXPECT_FALSE(add_overflow(result, Int128(1) << 100, Int128(1) << 100));
    EXPECT_EQ(result, Int128(1) << 101);

    EXPECT_TRUE(add_overflow(result, Int128(1) << 126, Int128(1) << 126));
}

TEST(Int128, sub_overflow_uint128)
{
    Uint128 result;
    EXPECT_FALSE(sub_overflow(result, Uint128(5), Uint128(3)));
    EXPECT_EQ(result, 2);

    EXPECT_FALSE(sub_overflow(result, Uint128(100), Uint128(50)));
    EXPECT_EQ(result, 50);

    EXPECT_FALSE(sub_overflow(result, Uint128(0), Uint128(0)));
    EXPECT_EQ(result, 0);

    EXPECT_FALSE(sub_overflow(result, Uint128(1) << 64, Uint128(1)));
    EXPECT_EQ(result, (Uint128(1) << 64) - 1);

    EXPECT_FALSE(sub_overflow(result, ~Uint128(0), Uint128(1)));
    EXPECT_EQ(result, ~Uint128(1));

    EXPECT_TRUE(sub_overflow(result, Uint128(1), Uint128(2)));
}

TEST(Int128, sub_overflow_int128)
{
    Int128 result;
    EXPECT_FALSE(sub_overflow(result, Int128(5), Int128(3)));
    EXPECT_EQ(result, 2);

    EXPECT_FALSE(sub_overflow(result, Int128(100), Int128(50)));
    EXPECT_EQ(result, 50);

    EXPECT_FALSE(sub_overflow(result, Int128(0), Int128(0)));
    EXPECT_EQ(result, 0);

    EXPECT_FALSE(sub_overflow(result, Int128(50), Int128(-50)));
    EXPECT_EQ(result, 100);

    EXPECT_FALSE(sub_overflow(result, Int128(-50), Int128(50)));
    EXPECT_EQ(result, -100);

    EXPECT_FALSE(sub_overflow(result, Int128(0), Int128(100)));
    EXPECT_EQ(result, -100);

    EXPECT_FALSE(sub_overflow(result, -(Int128(1) << 126), -(Int128(1) << 126)));
    EXPECT_EQ(result, 0);

    EXPECT_FALSE(sub_overflow(result, Int128(1) << 100, Int128(1) << 99));
    EXPECT_EQ(result, Int128(1) << 99);

    EXPECT_TRUE(sub_overflow(result, Int128(Uint128(1) << 127), 1));
}

TEST(Int128, mul_overflow_int128)
{
    Int128 result;
    EXPECT_FALSE(mul_overflow(result, Int128(0), Int128(100)));
    EXPECT_EQ(result, 0);

    EXPECT_FALSE(mul_overflow(result, Int128(5), Int128(3)));
    EXPECT_EQ(result, 15);

    EXPECT_FALSE(mul_overflow(result, Int128(100), Int128(200)));
    EXPECT_EQ(result, 20000);

    EXPECT_FALSE(mul_overflow(result, Int128(-5), Int128(3)));
    EXPECT_EQ(result, -15);

    EXPECT_FALSE(mul_overflow(result, Int128(-5), Int128(-3)));
    EXPECT_EQ(result, 15);

    EXPECT_FALSE(mul_overflow(result, Int128(1), Int128(1) << 100));
    EXPECT_EQ(result, Int128(1) << 100);

    EXPECT_FALSE(mul_overflow(result, Int128(2), Int128(1) << 100));
    EXPECT_EQ(result, Int128(1) << 101);

    EXPECT_TRUE(mul_overflow(result, Int128(1) << 64, Int128(1) << 64));
    EXPECT_TRUE(mul_overflow(result, Int128(1) << 100, Int128(1) << 100));
}

TEST(Int128, mul_overflow_uint128)
{
    Uint128 result;
    EXPECT_FALSE(mul_overflow(result, Uint128(0), Uint128(100)));
    EXPECT_EQ(result, 0);

    EXPECT_FALSE(mul_overflow(result, Uint128(5), Uint128(3)));
    EXPECT_EQ(result, 15);

    EXPECT_FALSE(mul_overflow(result, Uint128(100), Uint128(200)));
    EXPECT_EQ(result, 20000);

    EXPECT_FALSE(mul_overflow(result, Uint128(1) << 64, Uint128(1)));
    EXPECT_EQ(result, Uint128(1) << 64);

    EXPECT_FALSE(mul_overflow(result, Uint128(1000000), Uint128(1000000)));
    EXPECT_EQ(result, 1000000000000ULL);

    EXPECT_TRUE(mul_overflow(result, Uint128(1) << 127, Uint128(2)));
    EXPECT_TRUE(mul_overflow(result, ~Uint128(0), Uint128(2)));
    EXPECT_TRUE(mul_overflow(result, Uint128(1) << 65, Uint128(1) << 65));
}

TEST(Big_Int, construct_from_int)
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

TEST(Big_Int, construct_from_string)
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

TEST(Big_Int, is_zero)
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

    EXPECT_FALSE(Big_Int(1).is_zero());
    EXPECT_FALSE(Big_Int::pow2(100).is_zero());
    EXPECT_FALSE(Big_Int::pow2(200).is_zero());
}

TEST(Big_Int, get_twos_width)
{
    EXPECT_EQ(Big_Int(-1).get_twos_width(), 1);
    EXPECT_EQ(Big_Int(0).get_twos_width(), 1);

    EXPECT_EQ(Big_Int(-2).get_twos_width(), 2);
    EXPECT_EQ(Big_Int(1).get_twos_width(), 2);

    EXPECT_EQ(Big_Int(-4).get_twos_width(), 3);
    EXPECT_EQ(Big_Int(-3).get_twos_width(), 3);
    EXPECT_EQ(Big_Int(2).get_twos_width(), 3);
    EXPECT_EQ(Big_Int(3).get_twos_width(), 3);

    EXPECT_EQ(Big_Int::pow2(100).get_twos_width(), 102);
    EXPECT_EQ((-Big_Int::pow2(100)).get_twos_width(), 101);

    EXPECT_EQ(Big_Int::pow2(200).get_twos_width(), 202);
    EXPECT_EQ((-Big_Int::pow2(200)).get_twos_width(), 201);

    EXPECT_EQ(Big_Int::pow2(255).get_twos_width(), 257);
    EXPECT_EQ((-Big_Int::pow2(255)).get_twos_width(), 256);

    EXPECT_EQ(Big_Int::pow2(256).get_twos_width(), 258);
    EXPECT_EQ((-Big_Int::pow2(256)).get_twos_width(), 257);
}

TEST(Big_Int, get_ones_width)
{
    EXPECT_EQ(Big_Int(0).get_ones_width(), 1);

    EXPECT_EQ(Big_Int(-1).get_ones_width(), 2);
    EXPECT_EQ(Big_Int(1).get_ones_width(), 2);

    EXPECT_EQ(Big_Int(-3).get_ones_width(), 3);
    EXPECT_EQ(Big_Int(-2).get_ones_width(), 3);
    EXPECT_EQ(Big_Int(2).get_ones_width(), 3);
    EXPECT_EQ(Big_Int(3).get_ones_width(), 3);

    EXPECT_EQ(Big_Int(-4).get_ones_width(), 4);
    EXPECT_EQ(Big_Int(4).get_ones_width(), 4);

    EXPECT_EQ(Big_Int::pow2(100).get_ones_width(), 102);
    EXPECT_EQ((-Big_Int::pow2(100)).get_ones_width(), 102);

    EXPECT_EQ(Big_Int::pow2(200).get_ones_width(), 202);
    EXPECT_EQ((-Big_Int::pow2(200)).get_ones_width(), 202);

    EXPECT_EQ(Big_Int::pow2(255).get_ones_width(), 257);
    EXPECT_EQ((-Big_Int::pow2(255)).get_ones_width(), 257);

    EXPECT_EQ(Big_Int::pow2(256).get_ones_width(), 258);
    EXPECT_EQ((-Big_Int::pow2(256)).get_ones_width(), 258);
}

TEST(Big_Int, compare_zero)
{
    EXPECT_EQ((-Big_Int::pow2(200)).compare_zero(), std::strong_ordering::less);
    EXPECT_EQ((-Big_Int::pow2(100)).compare_zero(), std::strong_ordering::less);
    EXPECT_EQ(Big_Int(-2).compare_zero(), std::strong_ordering::less);
    EXPECT_EQ(Big_Int(-1).compare_zero(), std::strong_ordering::less);
    EXPECT_EQ(Big_Int(0).compare_zero(), std::strong_ordering::equal);
    EXPECT_EQ(Big_Int(1).compare_zero(), std::strong_ordering::greater);
    EXPECT_EQ(Big_Int(2).compare_zero(), std::strong_ordering::greater);
    EXPECT_EQ(Big_Int::pow2(100).compare_zero(), std::strong_ordering::greater);
    EXPECT_EQ(Big_Int::pow2(200).compare_zero(), std::strong_ordering::greater);
}

TEST(Big_Int, get_signum)
{
    EXPECT_EQ((-Big_Int::pow2(200)).get_signum(), -1);
    EXPECT_EQ((-Big_Int::pow2(100)).get_signum(), -1);
    EXPECT_EQ(Big_Int(-2).get_signum(), -1);
    EXPECT_EQ(Big_Int(-1).get_signum(), -1);
    EXPECT_EQ(Big_Int(0).get_signum(), 0);
    EXPECT_EQ(Big_Int(1).get_signum(), 1);
    EXPECT_EQ(Big_Int(2).get_signum(), 1);
    EXPECT_EQ(Big_Int::pow2(100).get_signum(), 1);
    EXPECT_EQ(Big_Int::pow2(200).get_signum(), 1);
}

TEST(Big_Int, unary_plus)
{
    EXPECT_EQ(+Big_Int(0), 0_n);
    EXPECT_EQ(+Big_Int::pow2(200), Big_Int::pow2(200));
}

TEST(Big_Int, unary_minus)
{
    EXPECT_EQ(-Big_Int(-2), 2);
    EXPECT_EQ(-Big_Int(-1), 1);
    EXPECT_EQ(Big_Int(0), 0);
    EXPECT_EQ(-Big_Int(1), -1);
    EXPECT_EQ(-Big_Int(2), -2);
    EXPECT_EQ(-Big_Int::pow2(100), (Big_Int::pow2(100) * -1_n));
    EXPECT_EQ(-Big_Int::pow2(200), (Big_Int::pow2(200) * -1_n));
}

TEST(Big_Int, bit_not)
{
    EXPECT_EQ(~(-Big_Int::pow2(200)), (Big_Int::pow2(200) - 1_n));
    EXPECT_EQ(~(-Big_Int::pow2(100)), (Big_Int::pow2(100) - 1_n));
    EXPECT_EQ(~Big_Int(-2), 1);
    EXPECT_EQ(~Big_Int(-1), 0);
    EXPECT_EQ(~Big_Int(0), -1);
    EXPECT_EQ(~Big_Int(1), -2);
    EXPECT_EQ(~Big_Int(2), -3);
    EXPECT_EQ(~Big_Int::pow2(100), (-Big_Int::pow2(100) - 1_n));
    EXPECT_EQ(~Big_Int::pow2(200), (-Big_Int::pow2(200) - 1_n));
}

TEST(Big_Int, compare_eq)
{
    EXPECT_EQ(Big_Int(0), 0);
    EXPECT_EQ(Big_Int(0), Int128 { 0 });
    EXPECT_EQ(Big_Int(0), 0_n);

    EXPECT_NE(Big_Int(1), 0);
    EXPECT_NE(Big_Int(1), Int128 { 0 });
    EXPECT_NE(Big_Int(1), 0_n);
}

TEST(Big_Int, compare_three_way)
{
    EXPECT_EQ(Big_Int(-1) <=> 0, std::strong_ordering::less);
    EXPECT_EQ(Big_Int(-1) <=> Int128 { 0 }, std::strong_ordering::less);
    EXPECT_EQ(Big_Int(-1) <=> 0_n, std::strong_ordering::less);
    EXPECT_EQ(-Big_Int::pow2(200) <=> 0_n, std::strong_ordering::less);

    EXPECT_EQ(Big_Int(0) <=> 0, std::strong_ordering::equal);
    EXPECT_EQ(Big_Int(0) <=> Int128 { 0 }, std::strong_ordering::equal);
    EXPECT_EQ(Big_Int(0) <=> 0_n, std::strong_ordering::equal);

    EXPECT_EQ(Big_Int(1) <=> 0, std::strong_ordering::greater);
    EXPECT_EQ(Big_Int(1) <=> Int128 { 0 }, std::strong_ordering::greater);
    EXPECT_EQ(Big_Int(1) <=> 0_n, std::strong_ordering::greater);
    EXPECT_EQ(Big_Int::pow2(200) <=> 0_n, std::strong_ordering::greater);
}

TEST(Big_Int, plus)
{
    EXPECT_EQ(0_n + 0_n, 0);
    EXPECT_EQ(1_n + 1_n, 2);
    EXPECT_EQ(1_n + -1_n, 0);

    EXPECT_EQ(-Big_Int::pow2(126) + -Big_Int::pow2(126), -Big_Int::pow2(127));
    EXPECT_EQ(+Big_Int::pow2(126) + -Big_Int::pow2(126), 0);
    EXPECT_EQ(-Big_Int::pow2(126) + +Big_Int::pow2(126), 0);
    EXPECT_EQ(+Big_Int::pow2(126) + +Big_Int::pow2(126), +Big_Int::pow2(127));

    EXPECT_EQ(+Big_Int::pow2(200) + +Big_Int::pow2(200), Big_Int::pow2(201));
    EXPECT_EQ(+Big_Int::pow2(200) + -Big_Int::pow2(200), 0);
    EXPECT_EQ(-Big_Int::pow2(200) + +Big_Int::pow2(200), 0);
    EXPECT_EQ(-Big_Int::pow2(200) + -Big_Int::pow2(200), -Big_Int::pow2(201));
}

TEST(Big_Int, minus)
{
    EXPECT_EQ(0_n - 0_n, 0);
    EXPECT_EQ(1_n - 1_n, 0);
    EXPECT_EQ(1_n - -1_n, 2);

    EXPECT_EQ(-Big_Int::pow2(126) - -Big_Int::pow2(126), 0);
    EXPECT_EQ(+Big_Int::pow2(126) - -Big_Int::pow2(126), +Big_Int::pow2(127));
    EXPECT_EQ(-Big_Int::pow2(126) - +Big_Int::pow2(126), -Big_Int::pow2(127));
    EXPECT_EQ(+Big_Int::pow2(126) - +Big_Int::pow2(126), 0);

    EXPECT_EQ(+Big_Int::pow2(200) - +Big_Int::pow2(200), 0);
    EXPECT_EQ(+Big_Int::pow2(200) - -Big_Int::pow2(200), +Big_Int::pow2(201));
    EXPECT_EQ(-Big_Int::pow2(200) - +Big_Int::pow2(200), -Big_Int::pow2(201));
    EXPECT_EQ(-Big_Int::pow2(200) - -Big_Int::pow2(200), 0);
}

TEST(Big_Int, multiplication)
{
    EXPECT_EQ(0_n * 0_n, 0);
    EXPECT_EQ(1_n * 1_n, 1);
    EXPECT_EQ(1_n * -1_n, -1);

    EXPECT_EQ(+Big_Int::pow2(126) * 2_n, +Big_Int::pow2(127));
    EXPECT_EQ(-Big_Int::pow2(126) * 2_n, -Big_Int::pow2(127));

    EXPECT_EQ(+Big_Int::pow2(100) * +Big_Int::pow2(100), +Big_Int::pow2(200));
    EXPECT_EQ(+Big_Int::pow2(100) * -Big_Int::pow2(100), -Big_Int::pow2(200));
    EXPECT_EQ(-Big_Int::pow2(100) * +Big_Int::pow2(100), -Big_Int::pow2(200));
    EXPECT_EQ(-Big_Int::pow2(100) * -Big_Int::pow2(100), +Big_Int::pow2(200));
}

void test_perfect_big_int_divisions(const Div_Rounding rounding)
{
    EXPECT_EQ(div(0_n, 1_n, rounding), 0);
    EXPECT_EQ(div(0_n, -1_n, rounding), 0);
    EXPECT_EQ(div(1_n, 1_n, rounding), 1);
    EXPECT_EQ(div(1_n, -1_n, rounding), -1);

    EXPECT_EQ(rem(0_n, 1_n, rounding), 0);
    EXPECT_EQ(rem(0_n, -1_n, rounding), 0);
    EXPECT_EQ(rem(1_n, 1_n, rounding), 0);
    EXPECT_EQ(rem(1_n, -1_n, rounding), 0);

    EXPECT_EQ(div(-Big_Int::pow2(200), 2_n, rounding), -Big_Int::pow2(199));
    EXPECT_EQ(div(-Big_Int::pow2(100), 2_n, rounding), -Big_Int::pow2(99));
    EXPECT_EQ(div(+Big_Int::pow2(100), 2_n, rounding), +Big_Int::pow2(99));
    EXPECT_EQ(div(+Big_Int::pow2(200), 2_n, rounding), +Big_Int::pow2(199));

    EXPECT_EQ(rem(-Big_Int::pow2(200), 2_n, rounding), 0);
    EXPECT_EQ(rem(-Big_Int::pow2(100), 2_n, rounding), 0);
    EXPECT_EQ(rem(+Big_Int::pow2(100), 2_n, rounding), 0);
    EXPECT_EQ(rem(+Big_Int::pow2(200), 2_n, rounding), 0);

    using Res = Div_Result<Big_Int>;
    EXPECT_EQ(div_rem(Big_Int::pow2(100), Big_Int::pow2(100), rounding), Res(1_n, 0_n));
    EXPECT_EQ(
        div_rem(Big_Int::pow2(200), Big_Int::pow2(100), rounding), Res(Big_Int::pow2(100), 0_n)
    );
    EXPECT_EQ(
        div_rem(Big_Int::pow2(400), Big_Int::pow2(200), rounding), Res(Big_Int::pow2(200), 0_n)
    );
    EXPECT_EQ(div_rem(Big_Int::pow2(400), Big_Int::pow2(400), rounding), Res(1_n, 0_n));

    constexpr auto i128_min = Big_Int(std::numeric_limits<Int128>::min());
    EXPECT_EQ(div(i128_min, -1_n, rounding), Big_Int::pow2(127));
    EXPECT_EQ(rem(i128_min, -1_n, rounding), 0);

    EXPECT_EQ(div(i128_min, -2_n, rounding), Big_Int::pow2(126));
    EXPECT_EQ(rem(i128_min, -2_n, rounding), 0);
}

void test_small_big_int_divisions(const Div_Rounding rounding)
{
    for (int dividend = -10; dividend < 10; ++dividend) {
        for (int divisor = -10; divisor < 10; ++divisor) {
            if (divisor == 0) {
                continue;
            }
            const auto [q_big, r_big] = div_rem(Big_Int(dividend), Big_Int(divisor), rounding);
            const auto [q, r] = div_rem(Int128(dividend), Int128(divisor), rounding);

            EXPECT_EQ(q_big, q);
            EXPECT_EQ(r_big, r);
        }
    }
}

TEST(Big_Int, div_to_zero_perfect)
{
    test_perfect_big_int_divisions(Div_Rounding::to_zero);
}

TEST(Big_Int, div_to_zero_small)
{
    test_small_big_int_divisions(Div_Rounding::to_zero);
}

TEST(Big_Int, div_to_pos_inf_perfect)
{
    test_perfect_big_int_divisions(Div_Rounding::to_pos_inf);
}

TEST(Big_Int, div_to_pos_inf_small)
{
    test_small_big_int_divisions(Div_Rounding::to_pos_inf);
}

TEST(Big_Int, div_to_neg_inf_perfect)
{
    test_perfect_big_int_divisions(Div_Rounding::to_neg_inf);
}

TEST(Big_Int, div_to_neg_inf_small)
{
    test_small_big_int_divisions(Div_Rounding::to_neg_inf);
}

TEST(Big_Int, shl)
{
    EXPECT_EQ(0_n << 0, 0);
    EXPECT_EQ(0_n << 100, 0);
    EXPECT_EQ(1_n << 100, Big_Int::pow2(100));
    EXPECT_EQ(1_n << 200, Big_Int::pow2(200));
    EXPECT_EQ(1_n << -1000, 0);
    EXPECT_EQ(-1_n << -1000, -1);

    EXPECT_EQ(+Big_Int::pow2(100) << +100, +Big_Int::pow2(200));
    EXPECT_EQ(+Big_Int::pow2(100) << -100, +1);
    EXPECT_EQ(-Big_Int::pow2(100) << +100, -Big_Int::pow2(200));
    EXPECT_EQ(-Big_Int::pow2(100) << -100, -1);

    EXPECT_EQ(+Big_Int::pow2(200) << +100, +Big_Int::pow2(300));
    EXPECT_EQ(+Big_Int::pow2(200) << -100, +Big_Int::pow2(100));
    EXPECT_EQ(-Big_Int::pow2(200) << +100, -Big_Int::pow2(300));
    EXPECT_EQ(-Big_Int::pow2(200) << -100, -Big_Int::pow2(100));
}

TEST(Big_Int, shr)
{
    EXPECT_EQ(0_n >> 0, 0);
    EXPECT_EQ(0_n >> 1000, 0);
    EXPECT_EQ(1_n >> -1000, Big_Int::pow2(1000));
    EXPECT_EQ(-1_n >> -1000, -Big_Int::pow2(1000));

    EXPECT_EQ(+Big_Int::pow2(100) >> +100, +1);
    EXPECT_EQ(+Big_Int::pow2(100) >> -100, +Big_Int::pow2(200));
    EXPECT_EQ(-Big_Int::pow2(100) >> +100, -1);
    EXPECT_EQ(-Big_Int::pow2(100) >> -100, -Big_Int::pow2(200));

    EXPECT_EQ(+Big_Int::pow2(200) >> +100, +Big_Int::pow2(100));
    EXPECT_EQ(+Big_Int::pow2(200) >> -100, +Big_Int::pow2(300));
    EXPECT_EQ(-Big_Int::pow2(200) >> +100, -Big_Int::pow2(100));
    EXPECT_EQ(-Big_Int::pow2(200) >> -100, -Big_Int::pow2(300));

    EXPECT_EQ(-Big_Int::pow2(200) >> 1000, -1);
    EXPECT_EQ(-Big_Int::pow2(100) >> 1000, -1);
    EXPECT_EQ(+Big_Int::pow2(100) >> 1000, 0);
    EXPECT_EQ(+Big_Int::pow2(200) >> 1000, 0);
}

TEST(Big_Int, pow)
{
    EXPECT_EQ(pow(2_n, 0), 1);
    EXPECT_EQ(pow(2_n, 100), Big_Int::pow2(100));
    EXPECT_EQ(pow(2_n, 200), Big_Int::pow2(200));
    EXPECT_EQ(pow(2_n, -1000), 0);
    EXPECT_EQ(pow(-2_n, -1000), 0);

    EXPECT_EQ(pow(Big_Int::pow2(200), 2), Big_Int::pow2(400));
    EXPECT_EQ(pow(-Big_Int::pow2(200), 2), Big_Int::pow2(400));

    EXPECT_EQ(pow(+Big_Int::pow2(200), -1), 0);
    EXPECT_EQ(pow(-Big_Int::pow2(200), -1), 0);
}

// -16 in two's complement is 0b1111...10000
// This makes it possible to run the bitwise operation tests below,
// with the same relevant bits, but involving negative numbers.
constexpr auto minus_16 = ~0b1111_n;
static_assert(minus_16 == -16);

TEST(Big_Int, bit_and)
{
    constexpr auto x = 0b0011_n;
    constexpr auto y = 0b0101_n;
    constexpr auto r = 0b0001_n;

    EXPECT_EQ(x & y, r);
    EXPECT_EQ((x << 200) & (y << 200), (r << 200));

    EXPECT_EQ((x | minus_16) & (y | minus_16), (r | minus_16));
    EXPECT_EQ(((x | minus_16) << 200) & ((y | minus_16) << 200), ((r | minus_16) << 200));
}

TEST(Big_Int, bit_or)
{
    constexpr auto x = 0b0011_n;
    constexpr auto y = 0b0101_n;
    constexpr auto r = 0b0111_n;

    EXPECT_EQ(x | y, r);
    EXPECT_EQ((x << 200) | (y << 200), (r << 200));

    EXPECT_EQ((x | minus_16) | (y | minus_16), (r | minus_16));
    EXPECT_EQ(((x | minus_16) << 200) | ((y | minus_16) << 200), ((r | minus_16) << 200));
}

TEST(Big_Int, bit_xor)
{
    constexpr auto x = 0b0011_n;
    constexpr auto y = 0b0101_n;
    constexpr auto r = 0b0110_n;

    EXPECT_EQ(x ^ y, r);
    EXPECT_EQ((x << 200) ^ (y << 200), (r << 200));

    EXPECT_EQ((x | minus_16) ^ (y | minus_16), r);
    EXPECT_EQ(((x | minus_16) << 200) ^ ((y | minus_16) << 200), (r << 200));
}

TEST(Big_Int, as_int)
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

TEST(Big_Int, from_characters)
{
    struct Test_Case {
        std::u8string_view string;
        int base;
    };
    // clang-format off
    static constexpr Test_Case test_cases[] {
        { u8"0"sv, 10 },
        { u8"1"sv, 10 },
        { u8"-1"sv, 10 },
        { u8"1"sv, 16 },
        { u8"-1"sv, 16 },
        { u8"11111111"sv, 2 },
        { u8"2010"sv, 5 },
        { u8"377"sv, 8 },
        { u8"255"sv, 10 },
        { u8"ff"sv, 16 },
        { u8"7v"sv, 32 },
        { u8"-11111111"sv, 2 },
        { u8"-2010"sv, 5 },
        { u8"-377"sv, 8 },
        { u8"-255"sv, 10 },
        { u8"-ff"sv, 16 },
        { u8"-7v"sv, 32 },
        { u8"100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"sv, 2 },
        { u8"111020132102420112021343001342032333120043341314112104422342034202402044234211314121001"sv, 5 },
        { u8"4000000000000000000000000000000000000000000000000000000000000000000"sv, 8 },
        { u8"1606938044258990275541962092341162602522202993782792835301376"sv, 10 },
        { u8"100000000000000000000000000000000000000000000000000"sv, 16 },
        { u8"10000000000000000000000000000000000000000"sv, 32 },
        { u8"-100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"sv, 2 },
        { u8"-111020132102420112021343001342032333120043341314112104422342034202402044234211314121001"sv, 5 },
        { u8"-4000000000000000000000000000000000000000000000000000000000000000000"sv, 8 },
        { u8"-1606938044258990275541962092341162602522202993782792835301376"sv, 10 },
        { u8"-100000000000000000000000000000000000000000000000000"sv, 16 },
        { u8"-10000000000000000000000000000000000000000"sv, 32 },
        { u8"100100100100110101101001001011001010011000011011111001110101100001011001001111000010011000100110011100000101111110011100010101100111001000000100011100010000100011010011111001010101010110010010000110000100010101000001011101000111100010000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"sv, 2 },
        { u8"102414221203323202133113331031102220100330010000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"sv, 5 },
        { u8"444465511312303371654131170230463405763425471004342043237125262206042501350742000000000000000000000000000000000"sv, 8 },
        { u8"10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"sv, 10 },
        { u8"1249ad2594c37ceb0b2784c4ce0bf38ace408e211a7caab24308a82e8f10000000000000000000000000"sv, 16 },
        { u8"4i9lkip9grstc5if164po5v72me827226jslap462585q7h00000000000000000000"sv, 32 },
        { u8"-100100100100110101101001001011001010011000011011111001110101100001011001001111000010011000100110011100000101111110011100010101100111001000000100011100010000100011010011111001010101010110010010000110000100010101000001011101000111100010000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"sv, 2 },
        { u8"-102414221203323202133113331031102220100330010000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"sv, 5 },
        { u8"-444465511312303371654131170230463405763425471004342043237125262206042501350742000000000000000000000000000000000"sv, 8 },
        { u8"-10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"sv, 10 },
        { u8"-1249ad2594c37ceb0b2784c4ce0bf38ace408e211a7caab24308a82e8f10000000000000000000000000"sv, 16 },
        { u8"-4i9lkip9grstc5if164po5v72me827226jslap462585q7h00000000000000000000"sv, 32 },
    };
    // clang-format on
    for (const auto& c : test_cases) {
        Big_Int result;
        const auto [p, ec] = from_characters(c.string, result, c.base);
        const auto parsed_length = std::size_t(p - reinterpret_cast<const char*>(c.string.data()));
        if (ec != std::errc {} || parsed_length != c.string.size()) {
            FAIL();
        }
        EXPECT_EQ(to_u8string(result, c.base), c.string);
    }
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
