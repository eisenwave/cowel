#include <cmath>
#include <limits>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence.hpp"
#include "cowel/util/math.hpp"
#include "cowel/util/result.hpp"

#include "cowel/policy/plaintext.hpp"

#include "cowel/big_int_ops.hpp"
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

template <typename T>
[[nodiscard]]
T operate_unary(Unary_Numeric_Expression_Kind type, const T& x)
{
    if constexpr (std::is_same_v<T, Big_Int>) {
        switch (type) {
        case Unary_Numeric_Expression_Kind::pos: return +x;
        case Unary_Numeric_Expression_Kind::neg: return -x;
        case Unary_Numeric_Expression_Kind::abs: return abs(x);
        default: COWEL_ASSERT_UNREACHABLE(u8"Invalid unary operation for integers.");
        }
    }
    else {
        static_assert(std::is_floating_point_v<T>);
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
T operate_binary(N_Ary_Numeric_Expression_Kind type, const T& x, const T& y)
{
    switch (type) {
    case N_Ary_Numeric_Expression_Kind::add: return x + y;
    case N_Ary_Numeric_Expression_Kind::sub: return x - y;
    case N_Ary_Numeric_Expression_Kind::mul: return x * y;
    case N_Ary_Numeric_Expression_Kind::div: {
        COWEL_DEBUG_ASSERT(std::is_floating_point_v<T>);
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
Big_Int operate_binary(
    const Integer_Division_Kind type, //
    const Big_Int& x,
    const Big_Int& y
)
{
    COWEL_DEBUG_ASSERT(y != 0);
    switch (type) {
    case Integer_Division_Kind::div_to_zero: return x / y;
    case Integer_Division_Kind::rem_to_zero: return x % y;
    case Integer_Division_Kind::div_to_pos_inf: return div(x, y, Div_Rounding::to_pos_inf);
    case Integer_Division_Kind::rem_to_pos_inf: return rem(x, y, Div_Rounding::to_pos_inf);
    case Integer_Division_Kind::div_to_neg_inf: return div(x, y, Div_Rounding::to_neg_inf);
    case Integer_Division_Kind::rem_to_neg_inf: return rem(x, y, Div_Rounding::to_neg_inf);
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
            joined_char_sequence(
                {
                    u8"Expected a value of type "sv,
                    Type::boolean.get_display_name(),
                    u8", but got "sv,
                    argument.value.get_type().get_display_name(),
                    u8".",
                }
            )
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
            joined_char_sequence(
                {
                    u8"Expected a value of type "sv,
                    Type::boolean.get_display_name(),
                    u8", but got "sv,
                    type.get_display_name(),
                    u8".",
                }
            )
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
    static const auto equality_comparable = Type::canonical_union_of(
        { Type::unit, Type::null, Type::boolean, Type::integer, Type::floating, Type::str }
    );
    static const auto relation_comparable
        = Type::canonical_union_of({ Type::integer, Type::floating, Type::str });
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
    const Value& y = y_value.get();

    if (x.get_type() != y.get_type()) {
        context.try_error(
            diagnostic::type_mismatch, y_value.get_location(),
            joined_char_sequence(
                {
                    u8"Cannot compare values of different type; that is, cannot compare "sv,
                    y.get_type().get_display_name(),
                    u8" with left-hand-side type "sv,
                    x.get_type().get_display_name(),
                    u8".",
                }
            )
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

Result<bool, Processing_Status>
Internal_Eq_Behavior::do_evaluate(const Invocation& call, Context& context) const
{
    static const auto equality_comparable = Type::canonical_union_of(
        {
            Type::unit,
            Type::null,
            Type::boolean,
            Type::integer,
            Type::floating,
            Type::str,
            Type::group,
        }
    );

    Value_Of_Type_Matcher x_value { &equality_comparable };
    Group_Member_Matcher x_member { u8"x"sv, Optionality::mandatory, x_value };
    Value_Of_Type_Matcher y_value { &equality_comparable };
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
    const Value& y = y_value.get();

    if (x.get_type_kind() != y.get_type_kind()
        || (!x.get_type().is_dynamic() && !y.get_type().is_dynamic() && x.get_type() != y.get_type()
        )) {
        context.try_error(
            diagnostic::type_mismatch, y_value.get_location(),
            joined_char_sequence(
                {
                    u8"Cannot compare values of different type; that is, cannot compare "sv,
                    y.get_type().get_display_name(),
                    u8" with left-hand-side type "sv,
                    x.get_type().get_display_name(),
                    u8".",
                }
            )
        );
        return Processing_Status::error;
    }

    return compare<Comparison_Expression_Kind::eq>(x, y);
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
            joined_char_sequence(
                {
                    u8"Expected a value of type "sv,
                    numeric_type.get_display_name(),
                    u8", but got "sv,
                    first_type.get_display_name(),
                    u8".",
                }
            )
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

Result<Big_Int, Processing_Status>
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
                joined_char_sequence(
                    {
                        u8"Expected a value of type "sv,
                        Type::integer.get_display_name(),
                        u8", but got "sv,
                        value.get_type().get_display_name(),
                        u8".",
                    }
                )
            );
            type_check_ok = false;
        }
    }
    if (!type_check_ok) {
        return Processing_Status::error;
    }

    const auto& x_value = group_matcher.get_values()[0].value;
    const auto& [y_value, y_location] = group_matcher.get_values()[1];

    if (y_value.as_integer().is_zero()) {
        context.try_error(diagnostic::arithmetic_div_by_zero, y_location, u8"Division by zero."sv);
        return Processing_Status::error;
    }
    Big_Int result = operate_binary(m_expression_kind, x_value.as_integer(), y_value.as_integer());
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
                    joined_char_sequence(
                        {
                            u8"Expected a value of type "sv,
                            Type::floating.get_display_name(),
                            u8", but got "sv,
                            value.get_type().get_display_name(),
                            u8".",
                        }
                    )
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
                    joined_char_sequence(
                        {
                            u8"Expected a value of type "sv,
                            numeric_type.get_display_name(),
                            u8", but got "sv,
                            value.get_type().get_display_name(),
                            u8".",
                        }
                    )
                );
                type_check_ok = false;
            }
        }
        if (value.get_type() != first_type) {
            context.try_error(
                diagnostic::type_mismatch, location,
                joined_char_sequence(
                    {
                        u8"All arguments have to be of the same type, i.e. "sv,
                        first_type.get_display_name(),
                        u8".",
                    }
                )
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
        Big_Int result = first_value.as_integer();
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

Result<Value, Processing_Status>
To_Str_Behavior::evaluate(const Invocation& call, Context& context) const
{
    static const auto to_str_type = Type::canonical_union_of(
        { Type::unit, Type::boolean, Type::integer, Type::floating, Type::str, Type::block }
    );
    static constexpr Float_Format float_formats[] {
        Float_Format::fixed,
        Float_Format::scientific,
        Float_Format::splice,
    };
    static constexpr std::u8string_view format_options[] {
        u8"fixed"sv,
        u8"scientific"sv,
        u8"splice"sv,
    };

    Value_Of_Type_Matcher x_matcher { &to_str_type };
    Group_Member_Matcher x_member { u8"x"sv, Optionality::mandatory, x_matcher };
    Integer_Matcher base_matcher;
    Group_Member_Matcher base_member { u8"base"sv, Optionality::optional, base_matcher };
    Integer_Matcher zpad_matcher;
    Group_Member_Matcher zpad_member { u8"zpad"sv, Optionality::optional, zpad_matcher };
    Sorted_Options_Matcher format_matcher { format_options };
    Group_Member_Matcher format_member { u8"format"sv, Optionality::optional, format_matcher };
    Group_Member_Matcher* const parameters[] {
        &x_member,
        &base_member,
        &zpad_member,
        &format_member,
    };
    Pack_Usual_Matcher args_matcher { parameters };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    static constexpr auto base_error = u8"A base can only be provided for arguments of type int."sv;
    static constexpr auto zpad_error
        = u8"A zpad (zero-padding) can only be provided for arguments of type int."sv;
    static constexpr auto format_error
        = u8"A format can only be provided for arguments of type float."sv;

    const auto check_no_extra_parameters = [&] -> Processing_Status {
        if (base_matcher.was_matched()) {
            context.try_error(diagnostic::type_mismatch, base_matcher.get_location(), base_error);
            return Processing_Status::error;
        }
        if (zpad_matcher.was_matched()) {
            context.try_error(diagnostic::type_mismatch, zpad_matcher.get_location(), zpad_error);
            return Processing_Status::error;
        }
        if (format_matcher.was_matched()) {
            context.try_error(
                diagnostic::type_mismatch, format_matcher.get_location(), format_error
            );
            return Processing_Status::error;
        }
        return Processing_Status::ok;
    };

    Value& x_value = x_matcher.get();
    switch (x_value.get_type_kind()) {
    case Type_Kind::unit: {
        if (const auto s = check_no_extra_parameters(); s != Processing_Status::ok) {
            return s;
        }
        return Value::unit_string;
    }

    case Type_Kind::boolean: {
        if (const auto s = check_no_extra_parameters(); s != Processing_Status::ok) {
            return s;
        }
        return x_value.as_boolean() ? Value::true_string : Value::false_string;
    }

    case Type_Kind::integer: {
        if (format_matcher.was_matched()) {
            context.try_error(
                diagnostic::type_mismatch, format_matcher.get_location(), format_error
            );
            return Processing_Status::error;
        }
        const Big_Int base = base_matcher.get_or_default(10_n);
        if (base < 2 || base > 36) {
            context.try_error(
                diagnostic::to_str_base, base_matcher.get_location(),
                joined_char_sequence(
                    {
                        u8"The given base "sv,
                        to_u8string(base),
                        u8" is outside the valid range [2,36]."sv,
                    }
                )
            );
            return Processing_Status::error;
        }
        const Big_Int zpad = zpad_matcher.get_or_default(0_n);
        if (zpad < 0) {
            context.try_error(
                diagnostic::to_str_zpad, zpad_matcher.get_location(),
                joined_char_sequence(
                    {
                        u8"The given zpad "sv,
                        to_u8string(zpad),
                        u8" must not be negative."sv,
                    }
                )
            );
            return Processing_Status::error;
        }
        if (zpad > 1'000'000_n) {
            context.try_error(
                diagnostic::to_str_zpad, zpad_matcher.get_location(),
                joined_char_sequence(
                    {
                        u8"The given zpad "sv,
                        to_u8string(zpad),
                        u8" exceeds implementation limits."sv,
                    }
                )
            );
            return Processing_Status::error;
        }
        const auto base_int = int(base);
        const auto zpad_int = int(zpad);
        const Big_Int& x_int = x_value.as_integer();
        const auto chars = to_u8string(x_int, base_int);
        const auto sign_length = std::size_t(x_int < 0);
        const auto significant_digits = chars.length() - sign_length;
        if (std::size_t(zpad_int) <= significant_digits) {
            return Value::string(chars, String_Kind::ascii);
        }
        const auto zeros_to_prepend
            = std::max(std::size_t(zpad_int), significant_digits) - significant_digits;
        std::pmr::vector<char8_t> result { context.get_transient_memory() };
        result.reserve(sign_length + zeros_to_prepend + significant_digits);
        if (x_int < 0) {
            result.push_back(u8'-');
        }
        for (std::size_t i = 0; i < zeros_to_prepend; ++i) {
            result.push_back(u8'0');
        }
        result.insert(result.end(), chars.begin() + std::ptrdiff_t(sign_length), chars.end());
        return Value::string(as_u8string_view(result), String_Kind::ascii);
    }

    case Type_Kind::floating: {
        if (base_matcher.was_matched()) {
            context.try_error(diagnostic::type_mismatch, base_matcher.get_location(), base_error);
            return Processing_Status::error;
        }
        if (zpad_matcher.was_matched()) {
            context.try_error(diagnostic::type_mismatch, zpad_matcher.get_location(), zpad_error);
            return Processing_Status::error;
        }
        const auto f = x_matcher.get().as_float();
        const auto format = format_matcher.was_matched() ? float_formats[format_matcher.get()]
                                                         : Float_Format::splice;

        // TODO: This could be simplified if we simply had a function that converts Float
        // to a spliced Static_String.
        std::pmr::vector<char8_t> text { context.get_transient_memory() };
        Capturing_Ref_Text_Sink sink { text, Output_Language::text };
        Text_Only_Policy policy { sink };
        splice_float(policy, f, format);
        return Value::string(as_u8string_view(text), String_Kind::ascii);
    }

    case Type_Kind::str: {
        if (const auto s = check_no_extra_parameters(); s != Processing_Status::ok) {
            return s;
        }
        return std::move(x_value);
    }

    case Type_Kind::block: {
        if (const auto s = check_no_extra_parameters(); s != Processing_Status::ok) {
            return s;
        }
        std::pmr::vector<char8_t> text { context.get_transient_memory() };
        const Processing_Status splice_result
            = splice_value_to_plaintext(text, x_matcher.get(), context);
        if (splice_result != Processing_Status::ok) {
            return splice_result;
        }
        return Value::string(as_u8string_view(text), String_Kind::unknown);
    }
    default: break;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Type checking should have prevented this.");
}

Result<Float, Processing_Status>
Reinterpret_As_Float_Behavior::do_evaluate(const Invocation& call, Context& context) const
{
    Integer_Matcher x_matcher {};
    Group_Member_Matcher to_member { u8"x"sv, Optionality::mandatory, x_matcher };
    Group_Member_Matcher* const parameters[] { &to_member };
    Pack_Usual_Matcher args_matcher { parameters };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    const Big_Int& x_int = x_matcher.get();
    if (x_int < 0) {
        context.try_error(
            diagnostic::reinterpret_out_of_range, x_matcher.get_location(),
            joined_char_sequence(
                {
                    u8"Only positive values can be reinterpreted as "sv,
                    Type::floating.get_display_name(),
                    u8", but "sv,
                    to_u8string(x_int),
                    u8" was given."sv,
                }
            )
        );
        return Processing_Status::error;
    }
    constexpr auto max = Int128 { std::numeric_limits<Uint64>::max() };
    if (x_int > max) {
        context.try_error(
            diagnostic::reinterpret_out_of_range, x_matcher.get_location(),
            joined_char_sequence(
                {
                    u8"The given value "sv,
                    to_u8string(x_int),
                    u8" is too large to be reinterpreted as "sv,
                    Type::floating.get_display_name(),
                    u8". The maximum is ((1 << 64) - 1) = 0xffffffffffffffff."sv,
                }
            )
        );
        return Processing_Status::error;
    }

    const auto i128 = Int128(x_int);
    const auto u64 = Uint64(i128);
    COWEL_DEBUG_ASSERT(Int128 { u64 } == i128);
    return std::bit_cast<Float>(u64);
}

Result<Big_Int, Processing_Status>
Reinterpret_As_Int_Behavior::do_evaluate(const Invocation& call, Context& context) const
{
    Float_Matcher x_matcher {};
    Group_Member_Matcher x_member { u8"x"sv, Optionality::mandatory, x_matcher };
    Group_Member_Matcher* const parameters[] { &x_member };
    Pack_Usual_Matcher args_matcher { parameters };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    const Float x_float = x_matcher.get();
    const auto u64 = std::bit_cast<std::uint64_t>(x_float);
    return Big_Int { Int128 { u64 } };
}

namespace {

const Type& get_variable_type()
{
    // This is a function in order to avoid dynamic initialization.
    // Once Type stores a "small vector",
    // this can likely just be a constexpr variable.
    static const auto result = Type::canonical_union_of(
        {
            Type::unit,
            Type::null,
            Type::boolean,
            Type::integer,
            Type::floating,
            Type::str,
            Type::group,
        }
    );
    return result;
}

} // namespace

Processing_Status Var_Delete_Behavior::do_evaluate(const Invocation& call, Context& context) const
{
    String_Matcher name_matcher { context.get_transient_memory() };
    Group_Member_Matcher name_member { u8"name"sv, Optionality::mandatory, name_matcher };
    Group_Member_Matcher* const parameters[] { &name_member };
    Pack_Usual_Matcher args_matcher { parameters };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto status = call_matcher.match_call(call, context, make_fail_callback());
    if (status != Processing_Status::ok) {
        return status;
    }

    // This could be made more efficient using C++23 transparent erase,
    // once available.
    const auto it = context.get_variables().find(name_matcher.get());
    if (it == context.get_variables().end()) {
        context.try_error(
            diagnostic::var_delete, name_matcher.get_location(),
            joined_char_sequence(
                {
                    u8"Unable to get delete variable with the name \""sv,
                    name_matcher.get(),
                    u8"\"."sv,
                }
            )
        );
        return Processing_Status::error;
    }
    context.get_variables().erase(it);
    return Processing_Status::ok;
}

Result<bool, Processing_Status>
Var_Exists_Behavior::do_evaluate(const Invocation& call, Context& context) const
{
    String_Matcher name_matcher { context.get_transient_memory() };
    Group_Member_Matcher name_member { u8"name"sv, Optionality::mandatory, name_matcher };
    Group_Member_Matcher* const parameters[] { &name_member };
    Pack_Usual_Matcher args_matcher { parameters };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto status = call_matcher.match_call(call, context, make_fail_callback());
    if (status != Processing_Status::ok) {
        return status;
    }

    return context.get_variables().contains(name_matcher.get());
}

Result<Value, Processing_Status>
Var_Get_Behavior::evaluate(const Invocation& call, Context& context) const
{
    String_Matcher name_matcher { context.get_transient_memory() };
    Group_Member_Matcher name_member { u8"name"sv, Optionality::mandatory, name_matcher };
    Group_Member_Matcher* const parameters[] { &name_member };
    Pack_Usual_Matcher args_matcher { parameters };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto status = call_matcher.match_call(call, context, make_fail_callback());
    if (status != Processing_Status::ok) {
        return status;
    }

    const auto it = context.get_variables().find(name_matcher.get());
    if (it == context.get_variables().end()) {
        context.try_error(
            diagnostic::var_get, name_matcher.get_location(),
            joined_char_sequence(
                {
                    u8"Unable to get variable with the name \""sv,
                    name_matcher.get(),
                    u8"\"."sv,
                }
            )
        );
        return Processing_Status::error;
    }
    return it->second;
}

Processing_Status Var_Let_Behavior::do_evaluate(const Invocation& call, Context& context) const
{
    String_Matcher name_matcher { context.get_transient_memory() };
    Group_Member_Matcher name_member { u8"name"sv, Optionality::mandatory, name_matcher };
    Value_Of_Type_Matcher value_matcher { &get_variable_type() };
    Group_Member_Matcher value_member { u8"value"sv, Optionality::optional, value_matcher };
    Group_Member_Matcher* const parameters[] { &name_member, &value_member };
    Pack_Usual_Matcher args_matcher { parameters };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto status = call_matcher.match_call(call, context, make_fail_callback());
    if (status != Processing_Status::ok) {
        return status;
    }

    std::pmr::u8string name { name_matcher.get(), context.get_transient_memory() };
    const auto it = context.get_variables().find(name);
    if (it != context.get_variables().end()) {
        context.try_error(
            diagnostic::var_let, name_matcher.get_location(),
            joined_char_sequence(
                {
                    u8"Unable to declare new variable with the name \""sv,
                    name_matcher.get(),
                    u8"\"."sv,
                }
            )
        );
        return Processing_Status::error;
    }
    const auto [_, success] = context.get_variables().emplace(
        std::move(name),
        value_matcher.was_matched() ? std::move(value_matcher.get()) : auto(Value::null)
    );
    COWEL_DEBUG_ASSERT(success);
    return Processing_Status::ok;
}

Processing_Status Var_Set_Behavior::do_evaluate(const Invocation& call, Context& context) const
{
    String_Matcher name_matcher { context.get_transient_memory() };
    Group_Member_Matcher name_member { u8"name"sv, Optionality::mandatory, name_matcher };
    Value_Of_Type_Matcher value_matcher { &get_variable_type() };
    Group_Member_Matcher value_member { u8"value"sv, Optionality::mandatory, value_matcher };
    Group_Member_Matcher* const parameters[] { &name_member, &value_member };
    Pack_Usual_Matcher args_matcher { parameters };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto status = call_matcher.match_call(call, context, make_fail_callback());
    if (status != Processing_Status::ok) {
        return status;
    }

    const auto it = context.get_variables().find(name_matcher.get());
    if (it == context.get_variables().end()) {
        context.try_error(
            diagnostic::var_set, name_matcher.get_location(),
            joined_char_sequence(
                {
                    u8"Unable to set variable with the name \""sv,
                    name_matcher.get(),
                    u8"\"."sv,
                }
            )
        );
        return Processing_Status::error;
    }
    it->second = std::move(value_matcher.get());
    return Processing_Status::ok;
}

} // namespace cowel
