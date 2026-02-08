#include <gtest/gtest.h>

#include "cowel/regexp.hpp"

namespace cowel {
namespace {

TEST(Reg_Exp, match)
{
    EXPECT_EQ(Reg_Exp::make(u8"awoo")->match(u8"awoo"), Reg_Exp_Status::matched);
    EXPECT_EQ(Reg_Exp::make(u8".*")->match(u8"awoo"), Reg_Exp_Status::matched);

    EXPECT_EQ(
        Reg_Exp::make(u8"\\p{Ll}+", Reg_Exp_Flags::unicode)->match(u8"abc"), Reg_Exp_Status::matched
    );
    EXPECT_EQ(
        Reg_Exp::make(u8"\\p{Ll}+", Reg_Exp_Flags::unicode)->match(u8"αβγ"), Reg_Exp_Status::matched
    );

    EXPECT_EQ(Reg_Exp::make(u8"\\u")->match(u8"u"), Reg_Exp_Status::matched);
    EXPECT_EQ(Reg_Exp::make(u8"\\u003")->match(u8"u003"), Reg_Exp_Status::matched);
    EXPECT_EQ(Reg_Exp::make(u8"\\u0030")->match(u8"0"), Reg_Exp_Status::matched);
    EXPECT_EQ(Reg_Exp::make(u8"\\u00303")->match(u8"03"), Reg_Exp_Status::matched);
    EXPECT_EQ(Reg_Exp::make(u8"\\\\u0030")->match(u8"\\u0030"), Reg_Exp_Status::matched);
}

TEST(Reg_Exp, search)
{
    EXPECT_EQ(Reg_Exp::make(u8"w")->search(u8"awoo").status, Reg_Exp_Status::matched);
    EXPECT_EQ(Reg_Exp::make(u8"z")->search(u8"awoo").status, Reg_Exp_Status::unmatched);

    const auto [w_status, w_match] = Reg_Exp::make(u8"w")->search(u8"ßw");
    EXPECT_EQ(w_status, Reg_Exp_Status::matched);
    EXPECT_EQ(w_match.index, 2);
    EXPECT_EQ(w_match.length, 1);

    const auto [sz_status, sz_match] = Reg_Exp::make(u8"ß")->search(u8"wß");
    EXPECT_EQ(sz_status, Reg_Exp_Status::matched);
    EXPECT_EQ(sz_match.index, 1);
    EXPECT_EQ(sz_match.length, 2);
}

} // namespace
} // namespace cowel
