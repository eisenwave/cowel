#include <limits>
#include <ranges>
#include <span>
#include <string>
#include <vector>

#include "cowel/expression_kind.hpp"
#include "cowel/util/assert.hpp"

#include "cowel/ast.hpp"
#include "cowel/fwd.hpp"
#include "cowel/type.hpp"
#include "cowel/value.hpp"

namespace cowel {

static_assert(std::numeric_limits<Float>::is_iec559);
static_assert(sizeof(Float) == 8);

[[nodiscard]]
std::u8string Type::get_display_name() const
{
    std::u8string result;

    if (m_kind == Type_Kind::union_) {
        result += u8'(';
        bool first = true;
        for (const Type& m : m_members) {
            if (!first) {
                result += u8" | ";
            }
            first = false;
            result += m.get_display_name();
        }
        return result += u8')';
    }

    result += type_kind_display_name(get_kind());

    switch (m_members.size()) {
    case 0: break;
    case 1: {
        result += u8" ";
        result += m_members.front().get_display_name();
        break;
    }
    default: {
        result += u8'(';
        bool first = true;
        for (const Type& m : get_members()) {
            if (!first) {
                result += u8", ";
            }
            first = false;
            result += m.get_display_name();
        }
        result += u8')';
    }
    }

    return result;
}

Value Value::block(const ast::Primary& block, Frame_Index frame)
{
    COWEL_ASSERT(block.get_kind() == ast::Primary_Kind::block);
    return Value { Union { .block = Block_And_Frame { &block, frame } }, block_index };
}

Value Value::block(const ast::Directive& block, Frame_Index frame)
{
    // TODO: assertions
    return Value { Union { .directive = Directive_And_Frame { &block, frame } }, directive_index };
}

Value Value::dynamic_string_forced(const std::u8string_view value, const String_Kind kind)
{
    GC_Ref<char8_t> gc = gc_ref_from_range<char8_t>(value);
    return Value { Union { .dynamic_string = std::move(gc) }, dynamic_string_index, kind };
}

Value Value::string(const std::u8string_view value, const String_Kind kind)
{
    if (value.size() <= Short_String_Value::max_size_v) {
        return short_string({ value.data(), value.size() }, kind);
    }
    return dynamic_string_forced(value, kind);
}

Value Value::regex(const Reg_Exp& value)
{
    return Value { Union { .regex = value }, regex_index };
}

Value Value::regex(Reg_Exp&& value)
{
    return Value { Union { .regex = std::move(value) }, regex_index };
}

Value Value::group(std::span<const Group_Member_Value> values)
{
    Group_Value gc = gc_ref_from_range<Group_Member_Value>(values);
    return Value { Union { .group = std::move(gc) }, group_index };
}

Value Value::group_move(std::span<Group_Member_Value> values)
{
    constexpr auto move_value
        = [](Group_Member_Value& v) -> Group_Member_Value&& { return std::move(v); };
    Group_Value gc
        = gc_ref_from_range<Group_Member_Value>(values | std::views::transform(move_value));
    return Value { Union { .group = std::move(gc) }, group_index };
}

Value Value::group_pack(std::span<const Value> values)
{
    constexpr auto copy_value = [](const Value& v) -> Group_Member_Value {
        return {
            .name = Value::null,
            .value = v,
        };
    };
    Group_Value gc
        = gc_ref_from_range<Group_Member_Value>(values | std::views::transform(copy_value));
    return Value { Union { .group = std::move(gc) }, group_index };
}

Value Value::group_pack_move(std::span<Value> values)
{
    constexpr auto move_value = [](Value& v) -> Group_Member_Value {
        return {
            .name = Value::null,
            .value = std::move(v),
        };
    };
    Group_Value gc
        = gc_ref_from_range<Group_Member_Value>(values | std::views::transform(move_value));
    return Value { Union { .group = std::move(gc) }, group_index };
}

namespace {

template <Comparison_Expression_Kind kind, typename T, typename U>
bool do_compare(const T& x, const U& y)
{
    if constexpr (kind == Comparison_Expression_Kind::eq) {
        return x == y;
    }
    else if constexpr (kind == Comparison_Expression_Kind::ne) {
        return x != y;
    }
    else if constexpr (kind == Comparison_Expression_Kind::lt) {
        return x < y;
    }
    else if constexpr (kind == Comparison_Expression_Kind::gt) {
        return x > y;
    }
    else if constexpr (kind == Comparison_Expression_Kind::le) {
        return x <= y;
    }
    else if constexpr (kind == Comparison_Expression_Kind::ge) {
        return x >= y;
    }
    else {
        static_assert(false);
    }
}

[[/* false positive */ maybe_unused]] bool
members_equal(std::span<const Group_Member_Value> xs, std::span<const Group_Member_Value> ys)
{
    if (xs.size() != ys.size()) {
        return false;
    }
    for (std::size_t i = 0; i < xs.size(); ++i) {
        if (!compare<Comparison_Expression_Kind::eq>(xs[i].name, ys[i].name)) {
            return false;
        }
        if (!compare<Comparison_Expression_Kind::eq>(xs[i].value, ys[i].value)) {
            return false;
        }
    }
    return true;
}

} // namespace

template <Comparison_Expression_Kind kind>
bool compare(const Value& x, const Value& y)
{
    switch (x.get_type_kind()) {
    case Type_Kind::unit:
    case Type_Kind::null: {
        if constexpr (kind == Comparison_Expression_Kind::eq) {
            return true;
        }
        else if constexpr (kind == Comparison_Expression_Kind::ne) {
            return false;
        }
        else {
            COWEL_ASSERT_UNREACHABLE(u8"Relational comparison of unit types?!");
        }
    }
    case Type_Kind::boolean: {
        return do_compare<kind>(x.as_boolean(), y.as_boolean());
    }
    case Type_Kind::integer: {
        return do_compare<kind>(x.as_integer(), y.as_integer());
    }
    case Type_Kind::floating: {
        return do_compare<kind>(x.as_float(), y.as_float());
    }
    case Type_Kind::str: {
        return do_compare<kind>(x.as_string(), y.as_string());
    }
    case Type_Kind::group: {
        if constexpr (kind == Comparison_Expression_Kind::eq) {
            return members_equal(x.get_group_members(), y.get_group_members());
        }
        else if constexpr (kind == Comparison_Expression_Kind::ne) {
            return !members_equal(x.get_group_members(), y.get_group_members());
        }
        else {
            COWEL_ASSERT_UNREACHABLE(u8"Relational comparison of unit types?!");
        }
    }
    default: break;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid type in comparison.");
}

template bool compare<Comparison_Expression_Kind::eq>(const Value&, const Value&);
template bool compare<Comparison_Expression_Kind::ne>(const Value&, const Value&);
template bool compare<Comparison_Expression_Kind::lt>(const Value&, const Value&);
template bool compare<Comparison_Expression_Kind::gt>(const Value&, const Value&);
template bool compare<Comparison_Expression_Kind::le>(const Value&, const Value&);
template bool compare<Comparison_Expression_Kind::ge>(const Value&, const Value&);

static_assert(sizeof(Int32) == 4);
static_assert(sizeof(Int64) == 8);
static_assert(sizeof(Int128) == 16);

static_assert(alignof(Value) <= 16, "Value should not be excessively aligned.");
static_assert(sizeof(Value) <= 64, "Value should not be too large to be passed by value.");

consteval bool test_sanitized()
{
    {
        auto v = Value::boolean(true);
        v = Value::integer(123_n);
        v = Value::integer(456_n);
    }
    {
        auto v = Value::boolean(true);
        v = Value::integer(123_n);
        v = Value::integer(456_n);
        v = Value::unit;
    }
    const Big_Int x = 123_n;
    {
        auto v = Value::integer(x);
    }
    {
        auto v = Value::boolean(true);
        v = Value::integer(x);
    }
    {
        auto v = Value::boolean(true);
        v = Value::integer(x);
        v = Value::unit;
    }
    return true;
}

static_assert(test_sanitized());

} // namespace cowel
