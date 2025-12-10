#include <cmath>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence.hpp"
#include "cowel/util/math.hpp"
#include "cowel/util/result.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/invocation.hpp"
#include "cowel/output_language.hpp"
#include "cowel/parameters.hpp"
#include "cowel/type.hpp"

namespace cowel {
namespace {

[[nodiscard]] [[maybe_unused]]
std::pmr::string vec_to_string(const std::pmr::vector<char>& v)
{
    return { v.data(), v.size(), v.get_allocator() };
}

template <Comparison_Expression_Kind kind, typename T>
bool do_compare(T x, T y)
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
    default: break;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid type in comparison.");
}

template <typename T>
[[nodiscard]]
T operate_unary(Unary_Numeric_Expression_Kind type, T x)
{
    if constexpr (std::is_integral_v<T>) {
        switch (type) {
        case Unary_Numeric_Expression_Kind::pos: return +x;
        case Unary_Numeric_Expression_Kind::neg: return -x;
        case Unary_Numeric_Expression_Kind::abs: return std::abs(x);
        default: COWEL_ASSERT_UNREACHABLE(u8"Invalid unary operation for integers.");
        }
    }
    else {
        switch (type) {
        case Unary_Numeric_Expression_Kind::pos: return +x;
        case Unary_Numeric_Expression_Kind::neg: return -x;
        case Unary_Numeric_Expression_Kind::abs: return std::abs(x);
        case Unary_Numeric_Expression_Kind::sqrt: return std::sqrt(x);
        case Unary_Numeric_Expression_Kind::trunc: return std::trunc(x);
        case Unary_Numeric_Expression_Kind::floor: return std::floor(x);
        case Unary_Numeric_Expression_Kind::ceil: return std::ceil(x);
        case Unary_Numeric_Expression_Kind::nearest: return cowel::roundeven(x);
        case Unary_Numeric_Expression_Kind::nearest_away_zero: return std::round(x);
        }
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid expression type.");
}

template <typename T>
[[nodiscard]]
T operate_binary(N_Ary_Numeric_Expression_Kind type, T x, T y)
{
    switch (type) {
    case N_Ary_Numeric_Expression_Kind::add: return x + y;
    case N_Ary_Numeric_Expression_Kind::sub: return x - y;
    case N_Ary_Numeric_Expression_Kind::mul: return x * y;
    case N_Ary_Numeric_Expression_Kind::div: {
        COWEL_DEBUG_ASSERT(y != 0);
        return x / y;
    }
    case N_Ary_Numeric_Expression_Kind::min: {
        if constexpr (std::is_floating_point_v<T>) {
            return cowel::fminimum(x, y);
        }
        else {
            return std::min(x, y);
        }
    }
    case N_Ary_Numeric_Expression_Kind::max: {
        if constexpr (std::is_floating_point_v<T>) {
            return cowel::fmaximum(x, y);
        }
        else {
            return std::max(x, y);
        }
    }
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid expression type.");
}

[[nodiscard]]
Integer operate_binary(Integer_Division_Kind type, Integer x, Integer y)
{
    COWEL_DEBUG_ASSERT(y != 0);
    switch (type) {
    case Integer_Division_Kind::div_to_zero: return x / y;
    case Integer_Division_Kind::rem_to_zero: return x % y;
    case Integer_Division_Kind::div_to_pos_inf: return div_to_pos_inf(x, y);
    case Integer_Division_Kind::rem_to_pos_inf: return rem_to_pos_inf(x, y);
    case Integer_Division_Kind::div_to_neg_inf: return div_to_neg_inf(x, y);
    case Integer_Division_Kind::rem_to_neg_inf: return rem_to_neg_inf(x, y);
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid expression type.");
}

} // namespace

Result<bool, Processing_Status>
Logical_Not_Behavior::do_evaluate(const Invocation& call, Context& context) const
{
    Group_Pack_Value_Matcher group_matcher { context.get_transient_memory() };
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    if (group_matcher.get_values().size() != 1) {
        context.try_error(
            diagnostic::type_mismatch, call.get_arguments_source_span(),
            u8"Logical NOT is unary and requires exactly one argument"sv
        );
        return Processing_Status::error;
    }

    const auto& argument = group_matcher.get_values().front();

    if (argument.value.get_type() != Type::boolean) {
        context.try_error(
            diagnostic::type_mismatch, argument.location,
            joined_char_sequence({
                u8"Expected a value of type "sv,
                Type::boolean.get_display_name(),
                u8", but got "sv,
                argument.value.get_type().get_display_name(),
                u8".",
            })
        );
    }

    return !argument.value.as_boolean();
}

namespace {

struct Logical_Expression_Evaluator {
    const Logical_Expression_Kind kind;
    Context& context;

    [[nodiscard]]
    Processing_Status type_error(const Type& type, const File_Source_Span& location) const
    {
        context.try_error(
            diagnostic::type_mismatch, location,
            joined_char_sequence({
                u8"Expected a value of type "sv,
                Type::boolean.get_display_name(),
                u8", but got "sv,
                type.get_display_name(),
                u8".",
            })
        );
        return Processing_Status::error;
    }

    [[nodiscard]]
    Result<bool, Processing_Status>
    operator()(std::span<const ast::Group_Member> members, Frame_Index frame) const
    {
        const bool neutral_element = kind == Logical_Expression_Kind::logical_and;
        const bool terminator = !neutral_element;

        for (const ast::Group_Member& member : members) {
            switch (member.get_kind()) {
            case ast::Member_Kind::named: {
                return Processing_Status::error;
            }
            case ast::Member_Kind::ellipsis: {
                const Stack_Frame& ellipsis_frame = context.get_call_stack()[frame];
                Result<bool, Processing_Status> maybe_result = (*this)(
                    ellipsis_frame.invocation.get_arguments_span(),
                    ellipsis_frame.invocation.content_frame
                );
                if (!maybe_result) {
                    return maybe_result;
                }
                if (*maybe_result == terminator) {
                    return *maybe_result;
                }
                continue;
            }
            case ast::Member_Kind::positional: {
                const ast::Member_Value& member_value = member.get_value();
                const std::optional<Type> static_type
                    = cowel::get_static_type(member_value, context);
                if (static_type && *static_type != Type::boolean) {
                    return type_error(*static_type, member_value.get_source_span());
                }
                const Result<Value, Processing_Status> member_result
                    = evaluate_member_value(member_value, frame, context);
                if (!member_result) {
                    return member_result.error();
                }
                if (!member_result->is_bool()) {
                    return type_error(member_result->get_type(), member_value.get_source_span());
                }
                if (member_result->as_boolean() == terminator) {
                    return terminator;
                }
                continue;
            }
            }
            COWEL_ASSERT_UNREACHABLE(u8"Invalid member kind.");
        }
        return neutral_element;
    }
};

} // namespace

Result<bool, Processing_Status>
Logical_Expression_Behavior::do_evaluate(const Invocation& call, Context& context) const
{
    Group_Pack_Lazy_Any_Matcher group_matcher;
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    const Logical_Expression_Evaluator evaluator { m_expression_kind, context };
    return evaluator(group_matcher.get().get_members(), call.content_frame);
}

Result<bool, Processing_Status>
Comparison_Expression_Behavior::do_evaluate(const Invocation& call, Context& context) const
{
    static const auto equality_comparable
        = Type::canonical_union_of({ Type::unit, Type::null, Type::integer, Type::floating });
    static const auto relation_comparable
        = Type::canonical_union_of({ Type::integer, Type::floating });
    const auto* parameter_type = m_expression_kind <= Comparison_Expression_Kind::ne
        ? &equality_comparable
        : &relation_comparable;

    Value_Of_Type_Matcher x_value { parameter_type };
    Group_Member_Matcher x_member { u8"x"sv, Optionality::mandatory, x_value };
    Value_Of_Type_Matcher y_value { parameter_type };
    Group_Member_Matcher y_member { u8"y"sv, Optionality::mandatory, y_value };
    Group_Member_Matcher* const matchers[] { &x_member, &y_member };
    Pack_Usual_Matcher args_matcher { matchers };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    const Value& x = x_value.get();
    const Value& y = x_value.get();

    if (x.get_type() != y.get_type()) {
        context.try_error(
            diagnostic::type_mismatch, y_value.get_location(),
            joined_char_sequence({
                u8"Cannot compare values of different type; that is, cannot compare "sv,
                y.get_type().get_display_name(),
                u8" with left-hand-side type "sv,
                x.get_type().get_display_name(),
                u8".",
            })
        );
        return Processing_Status::error;
    }

    switch (m_expression_kind) {
    case Comparison_Expression_Kind::eq: return compare<Comparison_Expression_Kind::eq>(x, y);
    case Comparison_Expression_Kind::ne: return compare<Comparison_Expression_Kind::ne>(x, y);
    case Comparison_Expression_Kind::lt: return compare<Comparison_Expression_Kind::lt>(x, y);
    case Comparison_Expression_Kind::gt: return compare<Comparison_Expression_Kind::gt>(x, y);
    case Comparison_Expression_Kind::le: return compare<Comparison_Expression_Kind::le>(x, y);
    case Comparison_Expression_Kind::ge: return compare<Comparison_Expression_Kind::ge>(x, y);
    }

    COWEL_ASSERT_UNREACHABLE(u8"Invalid expression kind.");
}

Result<Value, Processing_Status>
Unary_Numeric_Expression_Behavior::evaluate(const Invocation& call, Context& context) const
{
    static const Type numeric_type = Type::canonical_union_of({ Type::integer, Type::floating });

    Group_Pack_Value_Matcher group_matcher { context.get_transient_memory() };
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    if (group_matcher.get_values().size() != 1) {
        context.try_error(
            diagnostic::type_mismatch, call.get_arguments_source_span(),
            u8"Unary operation requires exactly one argument"sv
        );
        return Processing_Status::error;
    }

    const auto& first_value = group_matcher.get_values().front().value;
    const auto& first_type = first_value.get_type();

    if (!first_type.analytically_convertible_to(numeric_type)) {
        context.try_error(
            diagnostic::type_mismatch, group_matcher.get_values().front().location,
            joined_char_sequence({
                u8"Expected a value of type "sv,
                numeric_type.get_display_name(),
                u8", but got "sv,
                first_type.get_display_name(),
                u8".",
            })
        );
        return Processing_Status::error;
    }

    switch (first_type.get_kind()) {
    case Type_Kind::integer: {
        return Value::integer(operate_unary(m_expression_kind, first_value.as_integer()));
    }
    case Type_Kind::floating: {
        return Value::floating(operate_unary(m_expression_kind, first_value.as_float()));
    }
    default: break;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Type of value should have already been checked.");
}

Result<Integer, Processing_Status>
Integer_Division_Expression_Behavior::do_evaluate(const Invocation& call, Context& context) const
{
    Group_Pack_Value_Matcher group_matcher { context.get_transient_memory() };
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    bool type_check_ok = true;
    if (group_matcher.get_values().size() != 2) {
        context.try_error(
            diagnostic::type_mismatch, call.get_arguments_source_span(),
            u8"Binary operation requires two arguments."sv
        );
        type_check_ok = false;
    }

    for (const auto& [value, location] : group_matcher.get_values()) {
        if (value.get_type() != Type::integer) {
            context.try_error(
                diagnostic::type_mismatch, location,
                joined_char_sequence({
                    u8"Expected a value of type "sv,
                    Type::integer.get_display_name(),
                    u8", but got "sv,
                    value.get_type().get_display_name(),
                    u8".",
                })
            );
            type_check_ok = false;
        }
    }
    if (!type_check_ok) {
        return Processing_Status::error;
    }

    const auto& x_value = group_matcher.get_values()[0].value;
    const auto& [y_value, y_location] = group_matcher.get_values()[1];

    if (y_value.as_integer() == 0) {
        context.try_error(diagnostic::arithmetic_div_by_zero, y_location, u8"Division by zero."sv);
        return Processing_Status::error;
    }
    const Integer result
        = operate_binary(m_expression_kind, x_value.as_integer(), y_value.as_integer());
    return result;
}

Result<Value, Processing_Status>
N_Ary_Numeric_Expression_Behavior::evaluate(const Invocation& call, Context& context) const
{
    Group_Pack_Value_Matcher group_matcher { context.get_transient_memory() };
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    if (group_matcher.get_values().empty()) {
        context.try_error(
            diagnostic::type_mismatch, call.get_arguments_source_span(),
            u8"Cannot perform arithmetic with empty pack of arguments."sv
        );
        return Processing_Status::error;
    }

    const auto& first_value = group_matcher.get_values().front().value;
    const auto& first_type = first_value.get_type();

    bool type_check_ok = true;
    for (const auto& [value, location] : group_matcher.get_values()) {
        if (m_expression_kind == N_Ary_Numeric_Expression_Kind::div) {
            if (value.get_type() != Type::floating) {
                context.try_error(
                    diagnostic::type_mismatch, location,
                    joined_char_sequence({
                        u8"Expected a value of type "sv,
                        Type::floating.get_display_name(),
                        u8", but got "sv,
                        value.get_type().get_display_name(),
                        u8".",
                    })
                );
                type_check_ok = false;
            }
        }
        else {
            static const Type numeric_type
                = Type::canonical_union_of({ Type::integer, Type::floating });

            if (!value.get_type().analytically_convertible_to(numeric_type)) {
                context.try_error(
                    diagnostic::type_mismatch, location,
                    joined_char_sequence({
                        u8"Expected a value of type "sv,
                        numeric_type.get_display_name(),
                        u8", but got "sv,
                        value.get_type().get_display_name(),
                        u8".",
                    })
                );
                type_check_ok = false;
            }
        }
        if (value.get_type() != first_type) {
            context.try_error(
                diagnostic::type_mismatch, location,
                joined_char_sequence({
                    u8"All arguments have to be of the same type, i.e. "sv,
                    first_type.get_display_name(),
                    u8".",
                })
            );
            type_check_ok = false;
        }
    }
    if (!type_check_ok) {
        return Processing_Status::error;
    }

    COWEL_ASSERT(!group_matcher.get_values().empty());

    const auto values_without_first = group_matcher.get_values() | std::views::drop(1);

    switch (first_type.get_kind()) {
    case Type_Kind::integer: {
        Integer result = first_value.as_integer();
        for (const auto& [value, location] : values_without_first) {
            result = operate_binary(m_expression_kind, result, value.as_integer());
        }
        return Value::integer(result);
    }

    case Type_Kind::floating: {
        Float result = first_value.as_float();
        for (const auto& [value, location] : values_without_first) {
            result = operate_binary(m_expression_kind, result, value.as_float());
        }
        return Value::floating(result);
    }

    default: break;
    }

    COWEL_ASSERT_UNREACHABLE(u8"Unexpected type.");
}

Processing_Status
Variable_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    Spliceable_To_String_Matcher name_matcher { context.get_transient_memory() };
    Group_Member_Matcher to_member { u8"name"sv, Optionality::mandatory, name_matcher };
    Group_Member_Matcher* const parameters[] { &to_member };
    Pack_Usual_Matcher args_matcher { parameters };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return status_is_error(match_status) ? try_generate_error(out, call, context, match_status)
                                             : match_status;
    }

    return generate_var(out, call, name_matcher.get(), context);
}

Processing_Status Get_Variable_Behavior::generate_var(
    Content_Policy& out,
    const Invocation&,
    std::u8string_view var,
    Context& context
) const
{
    try_enter_paragraph(out);

    const auto it = context.get_variables().find(var);
    if (it != context.get_variables().end()) {
        const std::u8string_view value_string { it->second };
        if (!value_string.empty()) {
            out.write(value_string, Output_Language::text);
        }
    }
    return Processing_Status::ok;
}

Processing_Status set_variable_to_op_result(
    Variable_Operation op,
    const Invocation& call,
    std::u8string_view var,
    Context& context
)
{
    std::pmr::vector<char8_t> body_string { context.get_transient_memory() };
    const auto status
        = splice_to_plaintext(body_string, call.get_content_span(), call.content_frame, context);
    if (status != Processing_Status::ok) {
        return status;
    }

    const auto it = context.get_variables().find(var);
    if (op == Variable_Operation::set) {
        std::pmr::u8string value { body_string.data(), body_string.size(),
                                   context.get_persistent_memory() };
        if (it == context.get_variables().end()) {
            std::pmr::u8string key // NOLINT(misc-const-correctness)
                { var.data(), var.size(), context.get_persistent_memory() };
            context.get_variables().emplace(std::move(key), std::move(value));
        }
        else {
            it->second = std::move(value);
        }
    }
    return Processing_Status::ok;
}

} // namespace cowel
