#include <bitset>
#include <limits>
#include <ranges>
#include <span>
#include <string>
#include <string_view>

#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence.hpp"
#include "cowel/util/result.hpp"

#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/fwd.hpp"
#include "cowel/type.hpp"
#include "cowel/value.hpp"

#include "cowel/syntax/ast.hpp"
#include "cowel/syntax/expression_kind.hpp"

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

#define COWEL_BUILTIN_OPERATION_ONE(id) +1 // NOLINT

constexpr std::size_t cowel_builtin_operation_count
    = COWEL_BUILTIN_OPERATION_ENUM_DATA(COWEL_BUILTIN_OPERATION_ONE);

constexpr bool builtin_operation_kind_is_dynamically_typed_impl(const Builtin_Operation_Kind kind)
{
    const auto index = std::size_t(kind);
    COWEL_ASSERT(index <= std::size_t(cowel_builtin_operation_count));
    static constexpr auto lookup = []() {
        std::bitset<cowel_builtin_operation_count> result;
#define COWEL_BUILTIN_OPERATION_IS_DYNAMIC(id)                                                     \
    result[std::size_t(Builtin_Operation_Kind::id)] = (u8## #id##sv).ends_with(u8"_dynamic"sv);
        COWEL_BUILTIN_OPERATION_ENUM_DATA(COWEL_BUILTIN_OPERATION_IS_DYNAMIC)
        return result;
    }();
    return lookup[index];
}

static_assert(builtin_operation_kind_is_dynamically_typed_impl(Builtin_Operation_Kind::plus_dynamic)
);
static_assert(
    !builtin_operation_kind_is_dynamically_typed_impl(Builtin_Operation_Kind::plus_int_int)
);
static_assert(
    !builtin_operation_kind_is_dynamically_typed_impl(Builtin_Operation_Kind::eq_dynamic_groups)
);

} // namespace

bool builtin_operation_kind_is_dynamically_typed(const Builtin_Operation_Kind kind)
{
    return builtin_operation_kind_is_dynamically_typed_impl(kind);
}

namespace {

[[nodiscard]]
Result<bool, Processing_Status> evaluate_internal_equality(
    const Value& lhs,
    const Value& rhs,
    const File_Source_Span& lhs_location,
    const File_Source_Span& rhs_location,
    Context& context
)
{
    using enum Builtin_Operation_Kind;

    if (lhs.get_type_kind() != rhs.get_type_kind()) {
        return false;
    }

    switch (lhs.get_type_kind()) {
    case Type_Kind::unit:
    case Type_Kind::null:
    case Type_Kind::boolean:
    case Type_Kind::integer:
    case Type_Kind::floating:
    case Type_Kind::str: {
        const auto result
            = evaluate_builtin(eq_dynamic, lhs, rhs, lhs_location, rhs_location, context);
        if (!result) {
            return result.error();
        }
        return result->as_boolean();
    }

    case Type_Kind::group: {
        const auto result
            = evaluate_builtin(eq_dynamic_groups, lhs, rhs, lhs_location, rhs_location, context);
        if (!result) {
            return result.error();
        }
        return result->as_boolean();
    }

    default: break;
    }

    COWEL_ASSERT_UNREACHABLE(u8"Invalid type in internal equality.");
}

bool members_equal(
    std::span<const Group_Member_Value> xs,
    std::span<const Group_Member_Value> ys,
    const File_Source_Span& lhs_location,
    const File_Source_Span& rhs_location,
    Context& context
)
{
    if (xs.size() != ys.size()) {
        return false;
    }
    for (std::size_t i = 0; i < xs.size(); ++i) {
        const auto name_equal = evaluate_internal_equality(
            xs[i].name, ys[i].name, lhs_location, rhs_location, context
        );
        if (!name_equal || !*name_equal) {
            return false;
        }

        const auto value_equal = evaluate_internal_equality(
            xs[i].value, ys[i].value, lhs_location, rhs_location, context
        );
        if (!value_equal || !*value_equal) {
            return false;
        }
    }
    return true;
}

} // namespace

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

namespace {

[[nodiscard]]
bool expect_type(
    const Type& actual,
    const Type& expected,
    const File_Source_Span& error_location,
    Context& context
)
{
    if (actual.instance_of(expected)) {
        return true;
    }
    context.try_error(
        diagnostic::type_mismatch, error_location,
        joined_char_sequence(
            {
                u8"Expected a value of type "sv,
                expected.get_display_name(),
                u8", but got "sv,
                actual.get_display_name(),
                u8".",
            }
        )
    );
    return false;
}

[[nodiscard]]
bool expect_types_homogeneous(
    const Type& lhs,
    const Type& rhs,
    const Type& type,
    const File_Source_Span& lhs_location,
    const File_Source_Span& rhs_location,
    Context& context
)
{
    if (!expect_type(lhs, type, lhs_location, context)
        || !expect_type(rhs, type, rhs_location, context)) {
        return false;
    }
    if (lhs != rhs) {
        context.try_error(
            diagnostic::type_mismatch, rhs_location,
            joined_char_sequence(
                {
                    u8"Both operands must be of the same type, but left-hand side is "sv,
                    lhs.get_display_name(),
                    u8" and right-hand side is "sv,
                    rhs.get_display_name(),
                    u8".",
                }
            )
        );
        return false;
    }
    return true;
}

} // namespace

Result<Builtin_Operation_Kind, Processing_Status> check_dynamically_typed_operation(
    const Builtin_Operation_Kind kind,
    const Type& lhs,
    const Type& rhs,
    const File_Source_Span& lhs_location,
    const File_Source_Span& rhs_location,
    Context& context
)
{
    // Can't do proper type checking on analytical types like unions, any, etc.
    COWEL_ASSERT(type_kind_is_value_holdable(lhs.get_kind()));
    COWEL_ASSERT(type_kind_is_value_holdable(rhs.get_kind()));

    using enum Builtin_Operation_Kind;

    static constexpr Type equality_comparable_types[] {
        Type::unit, Type::null, Type::boolean, Type::integer, Type::floating, Type::str,
    };
    static constexpr auto equality_comparable = Type::union_of(equality_comparable_types);
    static_assert(equality_comparable.is_canonical());

    static constexpr Type relation_comparable_types[] { Type::integer, Type::floating, Type::str };
    static constexpr auto relation_comparable = Type::union_of(relation_comparable_types);
    static_assert(relation_comparable.is_canonical());

    static constexpr Type numeric_types[] { Type::integer, Type::floating };
    static constexpr auto numeric_type = Type::union_of(numeric_types);
    static_assert(numeric_type.is_canonical());

    switch (kind) {
    case logical_or_dynamic: {
        if (!expect_types_homogeneous(
                lhs, rhs, Type::boolean, lhs_location, rhs_location, context
            )) {
            return Processing_Status::error;
        }
        return logical_or_bool_bool;
    }
    case logical_and_dynamic: {
        if (!expect_types_homogeneous(
                lhs, rhs, Type::boolean, lhs_location, rhs_location, context
            )) {
            return Processing_Status::error;
        }
        return logical_and_bool_bool;
    }
    case eq_dynamic: {
        if (!expect_types_homogeneous(
                lhs, rhs, equality_comparable, lhs_location, rhs_location, context
            )) {
            return Processing_Status::error;
        }
        switch (lhs.get_kind()) {
        case Type_Kind::unit:
        case Type_Kind::null: return tautology;
        case Type_Kind::boolean: return eq_bool_bool;
        case Type_Kind::integer: return eq_int_int;
        case Type_Kind::floating: return eq_float_float;
        case Type_Kind::str: return eq_str_str;
        default: break;
        }
        COWEL_ASSERT_UNREACHABLE(u8"Validated type not in equality_comparable?!");
    }
    case ne_dynamic: {
        if (!expect_types_homogeneous(
                lhs, rhs, equality_comparable, lhs_location, rhs_location, context
            )) {
            return Processing_Status::error;
        }
        switch (lhs.get_kind()) {
        case Type_Kind::unit:
        case Type_Kind::null: return contradiction;
        case Type_Kind::boolean: return ne_bool_bool;
        case Type_Kind::integer: return ne_int_int;
        case Type_Kind::floating: return ne_float_float;
        case Type_Kind::str: return ne_str_str;
        default: break;
        }
        COWEL_ASSERT_UNREACHABLE(u8"Validated type not in equality_comparable?!");
    }
    case lt_dynamic:
    case gt_dynamic:
    case le_dynamic:
    case ge_dynamic: {
        if (!expect_types_homogeneous(
                lhs, rhs, relation_comparable, lhs_location, rhs_location, context
            )) {
            return Processing_Status::error;
        }
        switch (lhs.get_kind()) {
        case Type_Kind::integer:
            switch (kind) {
            case lt_dynamic: return lt_int_int;
            case gt_dynamic: return gt_int_int;
            case le_dynamic: return le_int_int;
            case ge_dynamic: return ge_int_int;
            default: break;
            }
            break;
        case Type_Kind::floating:
            switch (kind) {
            case lt_dynamic: return lt_float_float;
            case gt_dynamic: return gt_float_float;
            case le_dynamic: return le_float_float;
            case ge_dynamic: return ge_float_float;
            default: break;
            }
            break;
        case Type_Kind::str:
            switch (kind) {
            case lt_dynamic: return lt_str_str;
            case gt_dynamic: return gt_str_str;
            case le_dynamic: return le_str_str;
            case ge_dynamic: return ge_str_str;
            default: break;
            }
            break;
        default: break;
        }
        COWEL_ASSERT_UNREACHABLE(u8"Validated type not in relation_comparable?!");
    }

    case plus_dynamic: {
        if (!expect_types_homogeneous(
                lhs, rhs, numeric_type, lhs_location, rhs_location, context
            )) {
            return Processing_Status::error;
        }
        switch (lhs.get_kind()) {
        case Type_Kind::integer: return plus_int_int;
        case Type_Kind::floating: return plus_float_float;
        default: break;
        }
        COWEL_ASSERT_UNREACHABLE(u8"Validated type not in numeric_type?!");
    }
    case minus_dynamic: {
        if (!expect_types_homogeneous(
                lhs, rhs, numeric_type, lhs_location, rhs_location, context
            )) {
            return Processing_Status::error;
        }
        switch (lhs.get_kind()) {
        case Type_Kind::integer: return minus_int_int;
        case Type_Kind::floating: return minus_float_float;
        default: break;
        }
        COWEL_ASSERT_UNREACHABLE(u8"Validated type not in numeric_type?!");
    }
    case multiply_dynamic: {
        if (!expect_types_homogeneous(
                lhs, rhs, numeric_type, lhs_location, rhs_location, context
            )) {
            return Processing_Status::error;
        }
        switch (lhs.get_kind()) {
        case Type_Kind::integer: return multiply_int_int;
        case Type_Kind::floating: return multiply_float_float;
        default: break;
        }
        COWEL_ASSERT_UNREACHABLE(u8"Validated type not in numeric_type?!");
    }
    case div_dynamic: {
        if (!expect_types_homogeneous(
                lhs, rhs, Type::floating, lhs_location, rhs_location, context
            )) {
            return Processing_Status::error;
        }
        return div_float_float;
    }
    case rem_to_zero_dynamic: {
        if (!expect_types_homogeneous(
                lhs, rhs, Type::integer, lhs_location, rhs_location, context
            )) {
            return Processing_Status::error;
        }
        return rem_to_zero_int_int;
    }
    case tautology:
    case contradiction:
    case logical_or_bool_bool:
    case logical_and_bool_bool:
    case eq_bool_bool:
    case eq_int_int:
    case eq_float_float:
    case eq_str_str:
    case eq_dynamic_groups:
    case ne_bool_bool:
    case ne_int_int:
    case ne_float_float:
    case ne_str_str:
    case lt_int_int:
    case lt_float_float:
    case lt_str_str:
    case gt_int_int:
    case gt_float_float:
    case gt_str_str:
    case le_int_int:
    case le_float_float:
    case le_str_str:
    case ge_int_int:
    case ge_float_float:
    case ge_str_str:
    case plus_int_int:
    case plus_float_float:
    case minus_int_int:
    case minus_float_float:
    case multiply_int_int:
    case multiply_float_float:
    case div_float_float:
    case rem_to_zero_int_int: {
        if constexpr (is_debug_build) {
            COWEL_ASSERT_UNREACHABLE(u8"Should not check statically typed operations.");
        }
        return kind;
    }
    }

    COWEL_ASSERT_UNREACHABLE(u8"All switch cases should have returned.");
}

Result<Value, Processing_Status> evaluate_builtin(
    Builtin_Operation_Kind kind,
    const Value& lhs,
    const Value& rhs,
    [[maybe_unused]] const File_Source_Span& lhs_location,
    const File_Source_Span& rhs_location,
    Context& context
)
{
    using enum Builtin_Operation_Kind;

    switch (kind) {
    case tautology: {
        return Value::true_;
    }
    case contradiction: {
        return Value::false_;
    }
    case logical_or_bool_bool: {
        return Value::boolean(lhs.as_boolean() || rhs.as_boolean());
    }
    case logical_and_bool_bool: {
        return Value::boolean(lhs.as_boolean() && rhs.as_boolean());
    }
    case eq_bool_bool: {
        return Value::boolean(lhs.as_boolean() == rhs.as_boolean());
    }
    case eq_int_int: {
        return Value::boolean(lhs.as_integer() == rhs.as_integer());
    }
    case eq_float_float: {
        return Value::boolean(lhs.as_float() == rhs.as_float());
    }
    case eq_dynamic_groups: {
        return Value::boolean(members_equal(
            lhs.get_group_members(), rhs.get_group_members(), lhs_location, rhs_location, context
        ));
    }
    case eq_str_str: {
        return Value::boolean(lhs.as_string() == rhs.as_string());
    }
    case ne_bool_bool: {
        return Value::boolean(lhs.as_boolean() != rhs.as_boolean());
    }
    case ne_int_int: {
        return Value::boolean(lhs.as_integer() != rhs.as_integer());
    }
    case ne_float_float: {
        return Value::boolean(lhs.as_float() != rhs.as_float());
    }
    case ne_str_str: {
        return Value::boolean(lhs.as_string() != rhs.as_string());
    }
    case lt_int_int: {
        return Value::boolean(lhs.as_integer() < rhs.as_integer());
    }
    case lt_float_float: {
        return Value::boolean(lhs.as_float() < rhs.as_float());
    }
    case lt_str_str: {
        return Value::boolean(lhs.as_string() < rhs.as_string());
    }
    case gt_int_int: {
        return Value::boolean(lhs.as_integer() > rhs.as_integer());
    }
    case gt_float_float: {
        return Value::boolean(lhs.as_float() > rhs.as_float());
    }
    case gt_str_str: {
        return Value::boolean(lhs.as_string() > rhs.as_string());
    }
    case le_int_int: {
        return Value::boolean(lhs.as_integer() <= rhs.as_integer());
    }
    case le_float_float: {
        return Value::boolean(lhs.as_float() <= rhs.as_float());
    }
    case le_str_str: {
        return Value::boolean(lhs.as_string() <= rhs.as_string());
    }
    case ge_int_int: {
        return Value::boolean(lhs.as_integer() >= rhs.as_integer());
    }
    case ge_float_float: {
        return Value::boolean(lhs.as_float() >= rhs.as_float());
    }
    case ge_str_str: {
        return Value::boolean(lhs.as_string() >= rhs.as_string());
    }
    case plus_int_int: {
        return Value::integer(lhs.as_integer() + rhs.as_integer());
    }
    case plus_float_float: {
        return Value::floating(lhs.as_float() + rhs.as_float());
    }
    case minus_int_int: {
        return Value::integer(lhs.as_integer() - rhs.as_integer());
    }
    case minus_float_float: {
        return Value::floating(lhs.as_float() - rhs.as_float());
    }
    case multiply_int_int: {
        return Value::integer(lhs.as_integer() * rhs.as_integer());
    }
    case multiply_float_float: {
        return Value::floating(lhs.as_float() * rhs.as_float());
    }
    case div_float_float: {
        return Value::floating(lhs.as_float() / rhs.as_float());
    }
    case rem_to_zero_int_int: {
        if (rhs.as_integer().is_zero()) {
            context.try_error(
                diagnostic::arithmetic_div_by_zero, rhs_location, u8"Division by zero."sv
            );
            return Processing_Status::error;
        }
        return Value::integer(lhs.as_integer() % rhs.as_integer());
    }
    case Builtin_Operation_Kind::logical_or_dynamic:
    case Builtin_Operation_Kind::logical_and_dynamic:
    case Builtin_Operation_Kind::eq_dynamic:
    case Builtin_Operation_Kind::ne_dynamic:
    case Builtin_Operation_Kind::lt_dynamic:
    case Builtin_Operation_Kind::gt_dynamic:
    case Builtin_Operation_Kind::le_dynamic:
    case Builtin_Operation_Kind::ge_dynamic:
    case Builtin_Operation_Kind::plus_dynamic:
    case Builtin_Operation_Kind::minus_dynamic:
    case Builtin_Operation_Kind::multiply_dynamic:
    case Builtin_Operation_Kind::div_dynamic:
    case Builtin_Operation_Kind::rem_to_zero_dynamic: {
        const Result<Builtin_Operation_Kind, Processing_Status> checked
            = check_dynamically_typed_operation(
                kind, lhs.get_type(), rhs.get_type(), lhs_location, rhs_location, context
            );
        if (!checked) {
            return checked.error();
        }
        // If we got a dynamic operation back out, we would recurse infinitely.
        COWEL_DEBUG_ASSERT(!builtin_operation_kind_is_dynamically_typed(*checked));
        return evaluate_builtin(*checked, lhs, rhs, lhs_location, rhs_location, context);
    }
    }
    COWEL_ASSERT_UNREACHABLE(u8"All switch cases should have returned.");
}

} // namespace cowel
