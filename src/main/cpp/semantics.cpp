#include <limits>
#include <ranges>
#include <span>
#include <string>
#include <string_view>

#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence.hpp"
#include "cowel/util/result.hpp"

#include "cowel/ast.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/expression_kind.hpp"
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

Result<bool, Processing_Status> evaluate_comparison(
    const Comparison_Expression_Kind kind,
    const Value& lhs,
    const Value& rhs,
    const File_Source_Span& lhs_location,
    const File_Source_Span& rhs_location,
    Context& context
)
{
    using namespace std::string_view_literals;

    static constexpr Type equality_comparable_types[] {
        Type::unit, Type::null, Type::boolean, Type::integer, Type::floating, Type::str,
    };
    static constexpr auto equality_comparable = Type::union_of(equality_comparable_types);
    static_assert(equality_comparable.is_canonical());

    static constexpr Type relation_comparable_types[] { Type::integer, Type::floating, Type::str };
    static constexpr auto relation_comparable = Type::union_of(relation_comparable_types);
    static_assert(relation_comparable.is_canonical());

    const auto& acceptable_type
        = kind <= Comparison_Expression_Kind::ne ? equality_comparable : relation_comparable;

    bool ok = true;
    if (!lhs.get_type().analytically_convertible_to(acceptable_type)) {
        context.try_error(
            diagnostic::type_mismatch, lhs_location,
            joined_char_sequence(
                {
                    u8"Expected a value of type "sv,
                    acceptable_type.get_display_name(),
                    u8", but got "sv,
                    lhs.get_type().get_display_name(),
                    u8".",
                }
            )
        );
        ok = false;
    }
    if (!rhs.get_type().analytically_convertible_to(acceptable_type)) {
        context.try_error(
            diagnostic::type_mismatch, rhs_location,
            joined_char_sequence(
                {
                    u8"Expected a value of type "sv,
                    acceptable_type.get_display_name(),
                    u8", but got "sv,
                    rhs.get_type().get_display_name(),
                    u8".",
                }
            )
        );
        ok = false;
    }
    if (!ok) {
        return Processing_Status::error;
    }
    if (lhs.get_type() != rhs.get_type()) {
        context.try_error(
            diagnostic::type_mismatch, rhs_location,
            joined_char_sequence(
                {
                    u8"Cannot compare values of different type; that is, cannot compare "sv,
                    rhs.get_type().get_display_name(),
                    u8" with left-hand-side type "sv,
                    lhs.get_type().get_display_name(),
                    u8".",
                }
            )
        );
        return Processing_Status::error;
    }

    switch (kind) {
    case Comparison_Expression_Kind::eq: return compare<Comparison_Expression_Kind::eq>(lhs, rhs);
    case Comparison_Expression_Kind::ne: return compare<Comparison_Expression_Kind::ne>(lhs, rhs);
    case Comparison_Expression_Kind::lt: return compare<Comparison_Expression_Kind::lt>(lhs, rhs);
    case Comparison_Expression_Kind::gt: return compare<Comparison_Expression_Kind::gt>(lhs, rhs);
    case Comparison_Expression_Kind::le: return compare<Comparison_Expression_Kind::le>(lhs, rhs);
    case Comparison_Expression_Kind::ge: return compare<Comparison_Expression_Kind::ge>(lhs, rhs);
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid comparison kind.");
}

Result<Value, Processing_Status> evaluate_binary_numeric(
    const Binary_Expression_Kind kind,
    const Value& lhs,
    const Value& rhs,
    const File_Source_Span& lhs_location,
    const File_Source_Span& rhs_location,
    Context& context
)
{
    using namespace std::string_view_literals;

    // Division: float operands only.
    if (kind == Binary_Expression_Kind::divide) {
        bool ok = true;
        if (!lhs.is_float()) {
            context.try_error(
                diagnostic::type_mismatch, lhs_location,
                joined_char_sequence(
                    {
                        u8"Expected a value of type "sv,
                        Type::floating.get_display_name(),
                        u8", but got "sv,
                        lhs.get_type().get_display_name(),
                        u8".",
                    }
                )
            );
            ok = false;
        }
        if (!rhs.is_float()) {
            context.try_error(
                diagnostic::type_mismatch, rhs_location,
                joined_char_sequence(
                    {
                        u8"Expected a value of type "sv,
                        Type::floating.get_display_name(),
                        u8", but got "sv,
                        rhs.get_type().get_display_name(),
                        u8".",
                    }
                )
            );
            ok = false;
        }
        if (!ok) {
            return Processing_Status::error;
        }
        return Value::floating(lhs.as_float() / rhs.as_float());
    }

    // Remainder: integer operands only.
    if (kind == Binary_Expression_Kind::remainder) {
        bool ok = true;
        if (!lhs.is_int()) {
            context.try_error(
                diagnostic::type_mismatch, lhs_location,
                joined_char_sequence(
                    {
                        u8"Expected a value of type "sv,
                        Type::integer.get_display_name(),
                        u8", but got "sv,
                        lhs.get_type().get_display_name(),
                        u8".",
                    }
                )
            );
            ok = false;
        }
        if (!rhs.is_int()) {
            context.try_error(
                diagnostic::type_mismatch, rhs_location,
                joined_char_sequence(
                    {
                        u8"Expected a value of type "sv,
                        Type::integer.get_display_name(),
                        u8", but got "sv,
                        rhs.get_type().get_display_name(),
                        u8".",
                    }
                )
            );
            ok = false;
        }
        if (!ok) {
            return Processing_Status::error;
        }
        if (rhs.as_integer().is_zero()) {
            context.try_error(
                diagnostic::arithmetic_div_by_zero, rhs_location, u8"Division by zero."sv
            );
            return Processing_Status::error;
        }
        return Value::integer(lhs.as_integer() % rhs.as_integer());
    }

    // Addition, subtraction, multiplication: integer or float, same type.
    static constexpr Type numeric_types[] { Type::integer, Type::floating };
    static constexpr auto numeric_type = Type::union_of(numeric_types);
    static_assert(numeric_type.is_canonical());

    bool ok = true;
    if (!lhs.get_type().analytically_convertible_to(numeric_type)) {
        context.try_error(
            diagnostic::type_mismatch, lhs_location,
            joined_char_sequence(
                {
                    u8"Expected a value of type "sv,
                    numeric_type.get_display_name(),
                    u8", but got "sv,
                    lhs.get_type().get_display_name(),
                    u8".",
                }
            )
        );
        ok = false;
    }
    if (!rhs.get_type().analytically_convertible_to(numeric_type)) {
        context.try_error(
            diagnostic::type_mismatch, rhs_location,
            joined_char_sequence(
                {
                    u8"Expected a value of type "sv,
                    numeric_type.get_display_name(),
                    u8", but got "sv,
                    rhs.get_type().get_display_name(),
                    u8".",
                }
            )
        );
        ok = false;
    }
    if (!ok) {
        return Processing_Status::error;
    }
    if (lhs.get_type() != rhs.get_type()) {
        context.try_error(
            diagnostic::type_mismatch, rhs_location,
            joined_char_sequence(
                {
                    u8"Both operands must be of the same type, but left-hand side is "sv,
                    lhs.get_type().get_display_name(),
                    u8" and right-hand side is "sv,
                    rhs.get_type().get_display_name(),
                    u8".",
                }
            )
        );
        return Processing_Status::error;
    }

    switch (kind) {
    case Binary_Expression_Kind::plus: {
        if (lhs.is_int()) {
            return Value::integer(lhs.as_integer() + rhs.as_integer());
        }
        return Value::floating(lhs.as_float() + rhs.as_float());
    }
    case Binary_Expression_Kind::minus: {
        if (lhs.is_int()) {
            return Value::integer(lhs.as_integer() - rhs.as_integer());
        }
        return Value::floating(lhs.as_float() - rhs.as_float());
    }
    case Binary_Expression_Kind::multiply: {
        if (lhs.is_int()) {
            return Value::integer(lhs.as_integer() * rhs.as_integer());
        }
        return Value::floating(lhs.as_float() * rhs.as_float());
    }
    default: break;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Unexpected binary numeric kind.");
}

} // namespace cowel
