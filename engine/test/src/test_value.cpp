
#include <iostream>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "cowel/util/strings.hpp"

#include "cowel/regexp.hpp"
#include "cowel/type.hpp"
#include "cowel/value.hpp"

using namespace std::string_view_literals;

namespace cowel {

// NOLINTNEXTLINE(misc-use-internal-linkage)
std::ostream& operator<<(std::ostream& out, const Type& type);

std::ostream& operator<<(std::ostream& out, const Type& type)
{
    return out << as_string_view(type.get_display_name());
}

namespace {

static_assert(Value::unit.get_type() == Type::unit);
static_assert(Value::null.get_type() == Type::null);
static_assert(Value::boolean(true).get_type() == Type::boolean);
static_assert(Value::boolean(false).get_type() == Type::boolean);
static_assert(Value::integer(0_n).get_type() == Type::integer);
static_assert(Value::zero_int.get_type() == Type::integer);
static_assert(Value::empty_string.get_type() == Type::str);

TEST(Value, void_)
{
    EXPECT_EQ(Value::unit, Value::unit);
    EXPECT_TRUE(Value::unit.is_unit());
}

TEST(Value, null)
{
    EXPECT_EQ(Value::null, Value::null);
    EXPECT_TRUE(Value::null.is_null());
}

TEST(Value, boolean)
{
    EXPECT_EQ(Value::boolean(true).as_boolean(), true);
    EXPECT_EQ(Value::boolean(false).as_boolean(), false);

    EXPECT_EQ(Value::boolean(true), Value::true_);
    EXPECT_EQ(Value::boolean(false), Value::false_);

    EXPECT_EQ(Value::true_, Value::true_);
    EXPECT_EQ(Value::false_, Value::false_);
    EXPECT_NE(Value::true_, Value::false_);
    EXPECT_NE(Value::false_, Value::true_);
}

TEST(Value, integer)
{
    EXPECT_EQ(Value::integer(123_n).as_integer(), 123_n);
    EXPECT_EQ(Value::integer(0_n), Value::integer(0_n));
    EXPECT_NE(Value::integer(0_n), Value::integer(1_n));
}

TEST(Value, string)
{
    constexpr auto static_string = Value::static_string(u8"awoo"sv, String_Kind::ascii);
    EXPECT_EQ(static_string.as_string(), u8"awoo"sv);
    EXPECT_EQ(static_string.get_type(), Type::str);
    EXPECT_TRUE(static_string.is_static_string());

    constexpr auto short_string = Value::short_string(u8"awoo"sv, String_Kind::ascii);
    EXPECT_EQ(short_string.as_string(), u8"awoo"sv);
    EXPECT_EQ(short_string.get_type(), Type::str);
    EXPECT_FALSE(short_string.is_static_string());

    const auto dynamic_string = Value::dynamic_string_forced(u8"awoo"sv, String_Kind::ascii);
    EXPECT_EQ(dynamic_string.as_string(), u8"awoo"sv);
    EXPECT_EQ(dynamic_string.get_type(), Type::str);
    EXPECT_FALSE(dynamic_string.is_static_string());

    EXPECT_EQ(static_string, dynamic_string);
}

// =============================================================================
// Value::regex — construction and accessors
// =============================================================================

TEST(Value, regex_construction)
{
    const auto re = Reg_Exp::make(u8"abc"sv);
    ASSERT_TRUE(re.has_value());
    const Value v = Value::regex(*re);
    EXPECT_EQ(v.get_type(), Type::regex);
    EXPECT_TRUE(v.is_regex());
    EXPECT_EQ(v.as_regex().match(u8"abc"), Reg_Exp_Status::matched);
}

TEST(Value, regex_move_construction)
{
    auto re = Reg_Exp::make(u8"xyz"sv);
    ASSERT_TRUE(re.has_value());
    const Value v = Value::regex(std::move(*re));
    EXPECT_EQ(v.get_type(), Type::regex);
    EXPECT_EQ(v.as_regex().match(u8"xyz"), Reg_Exp_Status::matched);
}

TEST(Value, regex_const_ref_access)
{
    const auto re = Reg_Exp::make(u8"hello"sv);
    ASSERT_TRUE(re.has_value());
    const Value v = Value::regex(*re);
    const Value& cv = v;
    EXPECT_EQ(cv.as_regex().match(u8"hello"), Reg_Exp_Status::matched);
}

TEST(Value, regex_rvalue_access)
{
    auto re = Reg_Exp::make(u8"hi"sv);
    ASSERT_TRUE(re.has_value());
    Value v = Value::regex(std::move(*re));
    const Reg_Exp moved_re = std::move(v).as_regex();
    EXPECT_EQ(moved_re.match(u8"hi"), Reg_Exp_Status::matched);
}

TEST(Value, regex_copy)
{
    const auto re = Reg_Exp::make(u8"abc"sv);
    ASSERT_TRUE(re.has_value());
    const Value v1 = Value::regex(*re);
    const Value v2 = v1; // // NOLINT
    EXPECT_EQ(v2.as_regex().match(u8"abc"), Reg_Exp_Status::matched);
}

TEST(Value, regex_move_value)
{
    auto re = Reg_Exp::make(u8"abc"sv);
    ASSERT_TRUE(re.has_value());
    Value v1 = Value::regex(std::move(*re));
    const Value v2 = std::move(v1); // move Value
    EXPECT_EQ(v2.as_regex().match(u8"abc"), Reg_Exp_Status::matched);
}

// =============================================================================
// Value — floating point
// =============================================================================

TEST(Value, floating)
{
    const Value v = Value::floating(3.14);
    EXPECT_EQ(v.get_type(), Type::floating);
    EXPECT_DOUBLE_EQ(v.as_float(), 3.14);
}

TEST(Value, floating_equality)
{
    const Value a = Value::floating(1.0);
    const Value b = Value::floating(1.0);
    const Value c = Value::floating(2.0);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

// =============================================================================
// Value — dynamic string
// =============================================================================

TEST(Value, dynamic_string_construction)
{
    const Value v = Value::dynamic_string_forced(u8"hello world"sv, String_Kind::ascii);
    EXPECT_EQ(v.get_type(), Type::str);
    EXPECT_EQ(v.as_string(), u8"hello world"sv);
    EXPECT_FALSE(v.is_static_string());
}

TEST(Value, as_string_with_meta)
{
    const Value v = Value::static_string(u8"test"sv, String_Kind::ascii);
    const auto [sv, kind] = v.as_string_with_meta();
    EXPECT_EQ(sv, u8"test"sv);
    EXPECT_EQ(kind, String_Kind::ascii);
}

// =============================================================================
// Value — as_integer rvalue overloads
// =============================================================================

TEST(Value, as_integer_rvalue)
{
    Value v = Value::integer(42_n);
    const Big_Int moved_int = std::move(v).as_integer();
    EXPECT_EQ(moved_int, 42_n);
}

// =============================================================================
// Value — copy and move assignment across different types
// =============================================================================

TEST(Value, copy_assign_same_type_integer)
{
    Value a = Value::integer(10_n);
    const Value b = Value::integer(20_n);
    a = b;
    EXPECT_EQ(a.as_integer(), 20_n);
}

TEST(Value, copy_assign_different_types)
{
    Value a = Value::integer(10_n);
    const Value b = Value::boolean(true);
    a = b;
    EXPECT_EQ(a.get_type(), Type::boolean);
    EXPECT_EQ(a.as_boolean(), true);
}

TEST(Value, copy_assign_to_dynamic_string)
{
    Value a = Value::integer(5_n);
    const Value b = Value::dynamic_string_forced(u8"hi"sv, String_Kind::ascii);
    a = b;
    EXPECT_EQ(a.get_type(), Type::str);
    EXPECT_EQ(a.as_string(), u8"hi"sv);
}

TEST(Value, copy_assign_from_dynamic_string_to_dynamic_string)
{
    Value a = Value::dynamic_string_forced(u8"aaa"sv, String_Kind::ascii);
    const Value b = Value::dynamic_string_forced(u8"bbb"sv, String_Kind::ascii);
    a = b;
    EXPECT_EQ(a.as_string(), u8"bbb"sv);
}

TEST(Value, move_assign_integer)
{
    Value a = Value::integer(1_n);
    Value b = Value::integer(99_n);
    a = std::move(b);
    EXPECT_EQ(a.as_integer(), 99_n);
}

TEST(Value, move_assign_regex)
{
    const auto re = Reg_Exp::make(u8"abc"sv);
    ASSERT_TRUE(re.has_value());
    Value a = Value::integer(0_n);
    Value b = Value::regex(*re);
    a = std::move(b);
    EXPECT_EQ(a.get_type(), Type::regex);
    EXPECT_EQ(a.as_regex().match(u8"abc"), Reg_Exp_Status::matched);
}

TEST(Value, copy_assign_regex_to_regex)
{
    const auto re = Reg_Exp::make(u8"abc"sv);
    ASSERT_TRUE(re.has_value());
    Value a = Value::regex(*re);
    const auto re2 = Reg_Exp::make(u8"xyz"sv);
    ASSERT_TRUE(re2.has_value());
    const Value b = Value::regex(*re2);
    a = b;
    EXPECT_EQ(a.as_regex().match(u8"xyz"), Reg_Exp_Status::matched);
}

TEST(Value, self_copy_assign)
{
    Value v = Value::integer(42_n);
    // NOLINTNEXTLINE(clang-diagnostic-self-assign-overloaded)
    v = v;
    EXPECT_EQ(v.as_integer(), 42_n);
}

// =============================================================================
// Value — equality across mismatched types
// =============================================================================

TEST(Value, cross_type_inequality)
{
    EXPECT_NE(Value::integer(0_n), Value::boolean(false));
    EXPECT_NE(Value::null, Value::unit);
    EXPECT_NE(Value::null, Value::boolean(false));
}

// =============================================================================
// Type::canonical_union_of
// =============================================================================

TEST(Type, canonical_union_of)
{
    std::vector<Type> types = { Type::nothing };
    EXPECT_EQ(Type::canonical_union_of(types), Type::nothing);

    types = { Type::nothing, Type::nothing };
    EXPECT_EQ(Type::canonical_union_of(types), Type::nothing);

    types = { Type::any, Type::nothing };
    EXPECT_EQ(Type::canonical_union_of(types), Type::any);

    types = { Type::integer, Type::nothing };
    EXPECT_EQ(Type::canonical_union_of(types), Type::integer);

    types = { Type::integer, Type::integer };
    EXPECT_EQ(Type::canonical_union_of(types), Type::integer);

    types = { Type::integer };
    EXPECT_EQ(Type::canonical_union_of(types), Type::integer);

    types = { Type::integer, Type::any };
    EXPECT_EQ(Type::canonical_union_of(types), Type::any);

    types = { Type::integer, Type::unit };
    static constexpr Type unit_and_integer_types[] { Type::unit, Type::integer };
    EXPECT_EQ(Type::canonical_union_of(types), Type::union_of(unit_and_integer_types));
}

TEST(Type, analytically_convertible_to)
{
    static constexpr Type int_float_types[] { Type::integer, Type::floating };
    static constexpr auto int_or_float = Type::union_of(int_float_types);
    static_assert(int_or_float.is_canonical());
    static constexpr auto int_and_float = Type::group_of(int_float_types);
    static_assert(int_and_float.is_canonical());
    static constexpr auto lazy_int = Type::lazy(&Type::integer);

    EXPECT_TRUE(Type::any.analytically_convertible_to(Type::any));
    EXPECT_TRUE(Type::nothing.analytically_convertible_to(Type::nothing));
    EXPECT_TRUE(Type::unit.analytically_convertible_to(Type::unit));
    EXPECT_TRUE(Type::null.analytically_convertible_to(Type::null));
    EXPECT_TRUE(Type::integer.analytically_convertible_to(Type::integer));
    EXPECT_TRUE(Type::floating.analytically_convertible_to(Type::floating));

    EXPECT_TRUE(int_or_float.analytically_convertible_to(Type::any));
    EXPECT_FALSE(Type::any.analytically_convertible_to(int_or_float));
    EXPECT_TRUE(Type::nothing.analytically_convertible_to(int_or_float));
    EXPECT_TRUE(Type::integer.analytically_convertible_to(int_or_float));
    EXPECT_TRUE(Type::floating.analytically_convertible_to(int_or_float));
    EXPECT_FALSE(int_or_float.analytically_convertible_to(Type::integer));
    EXPECT_FALSE(int_or_float.analytically_convertible_to(Type::floating));

    EXPECT_TRUE(Type::integer.analytically_convertible_to(lazy_int));
    EXPECT_FALSE(lazy_int.analytically_convertible_to(Type::integer));

    EXPECT_TRUE(Type::group.analytically_convertible_to(Type::group));

    EXPECT_TRUE(Type::empty_group.analytically_convertible_to(Type::group));
    EXPECT_TRUE(Type::empty_group.analytically_convertible_to(Type::empty_group));
    EXPECT_FALSE(Type::empty_group.analytically_convertible_to(int_and_float));

    EXPECT_TRUE(int_and_float.analytically_convertible_to(int_and_float));
    EXPECT_TRUE(int_and_float.analytically_convertible_to(Type::group));
    EXPECT_FALSE(int_and_float.analytically_convertible_to(Type::empty_group));

    static constexpr Type some_union_types[] { Type::integer, int_and_float };
    static constexpr auto some_union = Type::union_of(some_union_types);
    static_assert(some_union.is_canonical());
    EXPECT_TRUE(int_and_float.analytically_convertible_to(some_union));

    EXPECT_FALSE(int_and_float.analytically_convertible_to(int_or_float));
    EXPECT_FALSE(Type::integer.analytically_convertible_to(int_and_float));
    EXPECT_FALSE(Type::floating.analytically_convertible_to(int_and_float));
}

} // namespace
} // namespace cowel
