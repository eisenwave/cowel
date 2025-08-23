#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cowel/parameters.hpp"
#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/to_chars.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/invocation.hpp"
#include "cowel/output_language.hpp"

namespace cowel {

namespace {

[[nodiscard]] [[maybe_unused]]
std::pmr::string vec_to_string(const std::pmr::vector<char>& v)
{
    return { v.data(), v.size(), v.get_allocator() };
}

long long operate(Expression_Type type, long long x, long long y)
{
    switch (type) {
    case Expression_Type::add: return x + y;
    case Expression_Type::subtract: return x - y;
    case Expression_Type::multiply: return x * y;
    case Expression_Type::divide: return x / y;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid expression type.");
}

Result<long long, Processing_Status> compute_expression(
    Expression_Type type,
    Content_Policy& out,
    const Invocation& call,
    Context& context
)
{
    Group_Pack_Integer_Matcher group_matcher { context.get_transient_memory() };
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return status_is_error(match_status) ? try_generate_error(out, call, context, match_status)
                                             : match_status;
    }

    Integer result = expression_type_neutral_element(type);
    bool first = true;
    for (const auto& [value, location] : group_matcher.get_values()) {
        if (type == Expression_Type::divide && value == 0) {
            context.try_error(
                diagnostic::arithmetic_div_by_zero, location, u8"Division by zero."sv
            );
            return try_generate_error(out, call, context);
        }

        if (first) {
            first = false;
            result = value;
        }
        else {
            result = operate(type, result, value);
        }
    }

    return result;
}

} // namespace

Processing_Status
Expression_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context) const
{
    const Result<long long, Processing_Status> result
        = compute_expression(m_type, out, call, context);
    if (!result) {
        return result.error();
    }

    try_enter_paragraph(out);

    const Characters8 result_chars = to_characters8(*result);
    out.write(result_chars.as_string(), Output_Language::text);
    return Processing_Status::ok;
}

Processing_Status
Variable_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context) const
{
    String_Matcher name_matcher { context.get_transient_memory() };
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
        = to_plaintext(body_string, call.get_content_span(), call.content_frame, context);
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
