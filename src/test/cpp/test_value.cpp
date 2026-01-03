
#include <iostream>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "cowel/util/strings.hpp"

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
static_assert(Value::integer(0).get_type() == Type::integer);
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
    EXPECT_EQ(Value::integer(123).as_integer(), Integer { 123 });
    EXPECT_EQ(Value::integer(0), Value::integer(0));
    EXPECT_NE(Value::integer(0), Value::integer(1));
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

TEST(Type, canonical_union_of)
{
    EXPECT_EQ(Type::canonical_union_of({ Type::nothing }), Type::nothing);
    EXPECT_EQ(Type::canonical_union_of({ Type::nothing, Type::nothing }), Type::nothing);

    EXPECT_EQ(Type::canonical_union_of({ Type::any, Type::nothing }), Type::any);

    EXPECT_EQ(Type::canonical_union_of({ Type::integer, Type::nothing }), Type::integer);
    EXPECT_EQ(Type::canonical_union_of({ Type::integer, Type::integer }), Type::integer);
    EXPECT_EQ(Type::canonical_union_of({ Type::integer }), Type::integer);

    const auto v = Type::canonical_union_of({ Type::integer, Type::any });
    EXPECT_EQ(Type::canonical_union_of({ Type::integer, Type::any }), Type::any);
    EXPECT_EQ(
        Type::canonical_union_of({ Type::integer, Type::unit }),
        Type::union_of({ Type::unit, Type::integer })
    );
}

TEST(Type, analytically_convertible_to)
{
    const auto int_or_float = Type::canonical_union_of({ Type::integer, Type::floating });
    const auto int_and_float = Type::group_of({ Type::integer, Type::floating });
    const auto lazy_int = Type::lazy(Type::integer);

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

    EXPECT_TRUE(int_and_float.analytically_convertible_to(
        Type::canonical_union_of({ Type::integer, int_and_float })
    ));
    EXPECT_FALSE(int_and_float.analytically_convertible_to(int_or_float));
    EXPECT_FALSE(Type::integer.analytically_convertible_to(int_and_float));
    EXPECT_FALSE(Type::floating.analytically_convertible_to(int_and_float));
}

} // namespace
} // namespace cowel
