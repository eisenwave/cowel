#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence.hpp"
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

// FIXME: the whole neutral element picking is technically dead code right now.
//        Specifically, this happens because we always require nonempty packs,
//        which is necessary because it would be unclear which type of zero cowel_add()
//        should return otherwise.
//        However, it is plausible that something like cowel_add_f32()
//        may clarify that in the future.

[[nodiscard]]
int expression_type_neutral_element(Numeric_Expression_Kind e)
{
    switch (e) {
    case Numeric_Expression_Kind::neg:
    case Numeric_Expression_Kind::add:
    case Numeric_Expression_Kind::sub: return 0;
    case Numeric_Expression_Kind::mul:
    case Numeric_Expression_Kind::div: return 1;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid expression type.");
}

[[maybe_unused]] [[nodiscard]]
Value expression_type_neutral_element(Numeric_Expression_Kind e, const Type& type)
{
    const int result = expression_type_neutral_element(e);
    switch (type.get_kind()) {
    case Type_Kind::integer: return Value::integer(result);
    case Type_Kind::f32: return Value::f32(Float32(result));
    case Type_Kind::f64: return Value::f64(Float64(result));
    default: break;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Unexpected type.");
}

[[nodiscard]] [[maybe_unused]]
std::pmr::string vec_to_string(const std::pmr::vector<char>& v)
{
    return { v.data(), v.size(), v.get_allocator() };
}

template <typename T>
[[nodiscard]]
T operate_binary(Numeric_Expression_Kind type, T x, T y)
{
    switch (type) {
    case Numeric_Expression_Kind::neg:
        COWEL_ASSERT_UNREACHABLE(u8"Negation is not a binary operation.");
    case Numeric_Expression_Kind::add: return x + y;
    case Numeric_Expression_Kind::sub: return x - y;
    case Numeric_Expression_Kind::mul: return x * y;
    case Numeric_Expression_Kind::div: return x / y;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid expression type.");
}

} // namespace

Result<Value, Processing_Status>
Expression_Behavior::evaluate(const Invocation& call, Context& context) const
{
    static const Type numeric_type
        = Type::canonical_union_of({ Type::integer, Type::f32, Type::f64 });

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

    if (m_expression_kind == Numeric_Expression_Kind::neg) {
        if (group_matcher.get_values().size() != 1) {
            context.try_error(
                diagnostic::type_mismatch, call.get_arguments_source_span(),
                u8"Negation is unary and requires exactly one argument"sv
            );
            return Processing_Status::error;
        }
        switch (first_type.get_kind()) {
        case Type_Kind::integer: {
            return Value::integer(-first_value.as_integer());
        }
        case Type_Kind::f32: {
            return Value::f32(-first_value.as_f32());
        }
        case Type_Kind::f64: {
            return Value::f64(-first_value.as_f64());
        }
        default: break;
        }
        COWEL_ASSERT_UNREACHABLE(u8"Type of value should have already been checked.");
    }

    // Type checks and check for integer division by zero.
    for (const auto& [value, location] : group_matcher.get_values()) {
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
        }
    }

    bool first = true;
    switch (first_type.get_kind()) {
    case Type_Kind::integer: {
        auto result = Integer(expression_type_neutral_element(m_expression_kind));
        for (const auto& [value, location] : group_matcher.get_values()) {
            if (first) {
                first = false;
                result = value.as_integer();
            }
            else if (m_expression_kind == Numeric_Expression_Kind::div && value.as_integer() == 0) {
                context.try_error(
                    diagnostic::arithmetic_div_by_zero, location, u8"Division by zero."sv
                );
                return Processing_Status::error;
            }
            else {

                result = operate_binary(m_expression_kind, result, value.as_integer());
            }
        }
        return Value::integer(result);
    }

    case Type_Kind::f32: {
        auto result = Float32(expression_type_neutral_element(m_expression_kind));
        for (const auto& [value, location] : group_matcher.get_values()) {
            if (first) {
                first = false;
                result = value.as_f32();
            }
            else {
                result = operate_binary(m_expression_kind, result, value.as_f32());
            }
        }
        return Value::f32(result);
    }

    case Type_Kind::f64: {
        auto result = Float64(expression_type_neutral_element(m_expression_kind));
        for (const auto& [value, location] : group_matcher.get_values()) {
            if (first) {
                first = false;
                result = value.as_f64();
            }
            else {
                result = operate_binary(m_expression_kind, result, value.as_f64());
            }
        }
        return Value::f64(result);
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
