#include <cmath>
#include <cstddef>
#include <cstring>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/html_writer.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/to_chars.hpp"

#include "cowel/policy/capture.hpp"
#include "cowel/policy/content_policy.hpp"
#include "cowel/policy/paragraph_split.hpp"
#include "cowel/policy/plaintext.hpp"

#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_behavior.hpp"
#include "cowel/directive_display.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"
#include "cowel/output_language.hpp"
#include "cowel/parameters.hpp"
#include "cowel/value.hpp"

#include "cowel/syntax/ast.hpp"

using namespace std::string_view_literals;

namespace cowel {

Processing_Status
Directive_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    const Result<Value, Processing_Status> result = evaluate(call, context);
    if (!result) {
        return try_generate_error(out, call, context, result.error());
    }
    return splice_value(out, *result, call.directive.get_source_span(), context);
}

Result<Value, Processing_Status>
Bool_Directive_Behavior::evaluate(const Invocation& call, Context& context) const
{
    const Result<bool, Processing_Status> result = do_evaluate(call, context);
    if (!result) {
        return result.error();
    }
    return Value::boolean(*result);
}

Processing_Status
Bool_Directive_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    const Result<bool, Processing_Status> result = do_evaluate(call, context);
    if (!result) {
        return try_generate_error(out, call, context, result.error());
    }
    splice_bool(out, *result);
    return Processing_Status::ok;
}

Result<Value, Processing_Status>
Int_Directive_Behavior::evaluate(const Invocation& call, Context& context) const
{
    Result<Big_Int, Processing_Status> result = do_evaluate(call, context);
    if (!result) {
        return result.error();
    }
    return Value::integer(std::move(*result));
}

Processing_Status
Int_Directive_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    const Result<Big_Int, Processing_Status> result = do_evaluate(call, context);
    if (!result) {
        return try_generate_error(out, call, context, result.error());
    }
    splice_int(out, *result);
    return Processing_Status::ok;
}

Result<Value, Processing_Status>
Float_Directive_Behavior::evaluate(const Invocation& call, Context& context) const
{
    const Result<Float, Processing_Status> result = do_evaluate(call, context);
    if (!result) {
        return result.error();
    }
    return Value::floating(*result);
}

Processing_Status Float_Directive_Behavior::splice(
    Content_Policy& out,
    const Invocation& call,
    Context& context
) const
{
    const Result<Float, Processing_Status> result = do_evaluate(call, context);
    if (!result) {
        return try_generate_error(out, call, context, result.error());
    }
    splice_float(out, *result);
    return Processing_Status::ok;
}

Result<Value, Processing_Status>
Short_String_Directive_Behavior::evaluate(const Invocation& call, Context& context) const
{
    const Result<Short_String_Value, Processing_Status> result = do_evaluate(call, context);
    if (!result) {
        return result.error();
    }
    return Value::short_string(*result, String_Kind::unknown);
}

Processing_Status Short_String_Directive_Behavior::splice(
    Content_Policy& out,
    const Invocation& call,
    Context& context
) const
{
    const Result<Short_String_Value, Processing_Status> result = do_evaluate(call, context);
    if (!result) {
        return try_generate_error(out, call, context, result.error());
    }
    out.write(result->as_string(), Output_Language::text);
    return Processing_Status::ok;
}

[[nodiscard]]
Result<Value, Processing_Status>
Block_Directive_Behavior::evaluate(const Invocation& call, Context&) const
{
    return Value::block(call.directive, call.content_frame);
}

[[nodiscard]]
const Type& get_static_type(const ast::Expression& v, Context& context)
{
    if (const auto* const d = v.try_as_directive()) {
        return get_static_type(*d, context);
    }
    if (const auto* const p = v.try_as_primary()) {
        return get_type(*p);
    }
    // TODO: infer static type for binary expressions
    return Type::any;
}

[[nodiscard]]
const Type& get_static_type(const ast::Directive& directive, Context& context)
{
    const Directive_Behavior* const behavior = context.find_directive(directive.get_name());
    return behavior ? behavior->get_static_type() : Type::any;
}

[[nodiscard]]
const Type& get_type(const ast::Primary& primary)
{
    // FIXME: This isn't actually the correct type.
    static constexpr auto pack_any = Type::pack_of(&Type::any);
    static const Type group_anything = Type::group_of({ &pack_any, 1 });

    switch (primary.get_kind()) {
    case ast::Primary_Kind::unit_literal: return Type::unit;
    case ast::Primary_Kind::null_literal: return Type::null;
    case ast::Primary_Kind::bool_literal: return Type::boolean;
    case ast::Primary_Kind::int_literal: return Type::integer;
    case ast::Primary_Kind::decimal_float_literal:
    case ast::Primary_Kind::infinity: return Type::floating;
    case ast::Primary_Kind::unquoted_member_name:
    case ast::Primary_Kind::quoted_string: return Type::str;
    case ast::Primary_Kind::id_expression: return Type::any;
    case ast::Primary_Kind::block: return Type::block;
    case ast::Primary_Kind::group: return group_anything;

    case ast::Primary_Kind::text:
    case ast::Primary_Kind::comment:
    case ast::Primary_Kind::escape:
        COWEL_ASSERT_UNREACHABLE(u8"Expected a value, not a markup element.");
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid primary kind.");
}

const Directive_Behavior* Context::find_directive(std::u8string_view name)
{
    if (const Directive_Behavior* const alias = find_alias(name)) {
        return alias;
    }
    if (const Directive_Behavior* const macro = find_macro(name)) {
        return macro;
    }
    return m_builtin_name_resolver(name);
}

bool Context::emplace_macro(
    std::pmr::u8string&& name,
    const std::span<const ast::Markup_Element> definition
)
{
    // TODO: once available, upgrade this to std::from_range construction
    std::pmr::vector<ast::Markup_Element> body {
        definition.begin(),
        definition.end(),
        m_macros.get_allocator(),
    };
    const auto [_, success] = m_macros.try_emplace(std::move(name), std::move(body));
    return success;
}

std::span<const ast::Markup_Element>
trim_blank_text_left(std::span<const ast::Markup_Element> content)
{
    while (!content.empty()) {
        if (const auto* const text = content.front().try_as_primary()) {
            if (text->get_kind() == ast::Primary_Kind::text && is_ascii_blank(text->get_source())) {
                content = content.subspan(1);
                continue;
            }
        }
        break;
    }
    return content;
}

std::span<const ast::Markup_Element>
trim_blank_text_right(std::span<const ast::Markup_Element> content)
{
    while (!content.empty()) {
        if (const auto* const text = content.back().try_as_primary()) {
            if (text->get_kind() == ast::Primary_Kind::text && is_ascii_blank(text->get_source())) {
                content = content.subspan(0, content.size() - 1);
                continue;
            }
        }
        break;
    }
    return content;
}

std::span<const ast::Markup_Element> trim_blank_text(std::span<const ast::Markup_Element> content)
{
    return trim_blank_text_right(trim_blank_text_left(content));
}

namespace {

void try_lookup_error(const ast::Directive& directive, Context& context)
{
    if (!context.emits(Severity::error)) {
        return;
    }

    const std::u8string_view message[] {
        u8"No directive with the name \"",
        directive.get_name(),
        u8"\" exists.",
    };
    context.try_error(
        diagnostic::directive_lookup_unresolved, directive.get_name_span(),
        joined_char_sequence(message)
    );
}

} // namespace

Processing_Status splice_to_plaintext(
    std::pmr::vector<char8_t>& out,
    std::span<const ast::Markup_Element> content,
    Frame_Index frame,
    Context& context
)
{
    const auto diagnostic_frame = context.push_diagnostic_frame(frame);
    Capturing_Ref_Text_Sink sink { out, Output_Language::text };
    Text_Only_Policy policy { sink };
    return splice_all(policy, content, frame, context);
}

[[nodiscard]]
Processing_Status splice_expression(
    Content_Policy& out,
    const ast::Expression& value,
    Frame_Index frame,
    Context& context
)
{
    const auto diagnostic_frame = context.push_diagnostic_frame(frame);
    // Fast path for directives.
    // Can bypass forming a `Value` when splicing directives that return blocks.
    if (const auto* const directive = value.try_as_directive()) {
        return splice_directive_invocation(out, *directive, frame, context);
    }
    // Fast path for primary-expressions.
    // Can bypass forming a Value when splicing string literals etc.
    if (const auto* const primary = value.try_as_primary()) {
        return splice_primary(out, *primary, frame, context);
    }

    // General path for anything else: evaluate and splice the value.
    const Result<Value, Processing_Status> evaluated = evaluate_expression(value, frame, context);
    if (!evaluated) {
        return evaluated.error();
    }
    return splice_value(out, *evaluated, value.get_source_span(), context);
}

[[nodiscard]]
Processing_Status splice_primary(
    Content_Policy& out,
    const ast::Primary& primary,
    Frame_Index frame,
    Context& context
)
{
    COWEL_ASSERT(primary.is_value());

    switch (primary.get_kind()) {
    case ast::Primary_Kind::unit_literal:
    case ast::Primary_Kind::null_literal:
    case ast::Primary_Kind::bool_literal:
    case ast::Primary_Kind::unquoted_member_name: {
        out.write(primary.get_source(), Output_Language::text);
        return Processing_Status::ok;
    }
    case ast::Primary_Kind::id_expression: {
        const Value* const var = context.get_variable(primary.get_source());
        if (!var) {
            context.try_error(
                diagnostic::id_lookup, primary.get_source_span(),
                joined_char_sequence(
                    {
                        u8"No variable with the name \""sv,
                        primary.get_source(),
                        u8"\" was found."sv,
                    }
                )
            );
            return Processing_Status::error;
        }
        return splice_value(out, *var, primary.get_source_span(), context);
    }
    case ast::Primary_Kind::int_literal:
    case ast::Primary_Kind::decimal_float_literal: {
        const Result<Value, Processing_Status> value = evaluate(primary, frame, context);
        COWEL_ASSERT(value);
        return splice_value(out, *value, primary.get_source_span(), context);
    }
    case ast::Primary_Kind::quoted_string: {
        return splice_quoted_string(out, primary, frame, context);
    }
    case ast::Primary_Kind::block: {
        return splice_block(out, primary, frame, context);
    }
    default: break;
    }
    COWEL_ASSERT_UNREACHABLE(u8"All spliceable kinds should have been handled above.");
}

Processing_Status splice_value(
    Content_Policy& out,
    const Value& value,
    const File_Source_Span& error_location,
    Context& context
)
{
    switch (value.get_type_kind()) {
    case Type_Kind::any:
    case Type_Kind::nothing:
    case Type_Kind::union_:
    case Type_Kind::pack:
    case Type_Kind::named:
    case Type_Kind::lazy: {
        COWEL_ASSERT_UNREACHABLE(u8"Values of this type should not exist.");
    }
    case Type_Kind::regex:
    case Type_Kind::group: {
        context.try_error(
            diagnostic::splice, error_location,
            joined_char_sequence(
                {
                    u8"Unable to splice value of type "sv,
                    value.get_type().get_display_name(),
                    u8"."sv,
                }
            )
        );
        return Processing_Status::error;
    }

    case Type_Kind::null: {
        out.write(u8"null"sv, Output_Language::text);
        return Processing_Status::ok;
    }

    case Type_Kind::unit: {
        return Processing_Status::ok;
    }
    case Type_Kind::boolean: {
        splice_bool(out, value.as_boolean());
        return Processing_Status::ok;
    }
    case Type_Kind::integer: {
        splice_int(out, value.as_integer());
        return Processing_Status::ok;
    }
    case Type_Kind::floating: {
        splice_float(out, value.as_float());
        return Processing_Status::ok;
    }
    case Type_Kind::str: {
        const std::u8string_view string = value.as_string();
        if (!string.empty()) {
            out.write(value.as_string(), Output_Language::text);
        }
        return Processing_Status::ok;
    }
    case Type_Kind::block: {
        return value.splice_block(out, context);
    }
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid kind of value.");
}

void splice_bool(Content_Policy& out, const bool value)
{
    const auto str = value ? u8"true"sv : u8"false"sv;
    out.write(str, Output_Language::text);
}

void splice_int(Content_Policy& out, const Big_Int& value)
{
    value.print_to([&](const std::u8string_view string) {
        out.write(string, Output_Language::text);
    });
}

void splice_float(Content_Policy& out, Float value, Float_Format format)
{
    if (std::isnan(value)) {
        out.write(u8"NaN"sv, Output_Language::text);
        return;
    }
    if (std::isinf(value)) {
        out.write(value < 0 ? u8"-infinity"sv : u8"infinity"sv, Output_Language::text);
        return;
    }
    switch (format) {
    case Float_Format::fixed: {
        const auto chars = to_characters8(value, std::chars_format::fixed);
        out.write(chars.as_string(), Output_Language::text);
        return;
    }
    case Float_Format::scientific: {
        const auto chars = to_characters8(value, std::chars_format::scientific);
        out.write(chars.as_string(), Output_Language::text);
        return;
    }
    case Float_Format::splice: break;
    }
    const auto fixed = to_characters8(value, std::chars_format::fixed);
    auto scientific = to_characters8(value, std::chars_format::scientific);
    // The problem we face is that to_chars outputs an exponent with a leading zero,
    // meaning that 10000 is printed as 1e+05,
    // making it a worse candidate for the shortest representation.
    struct Handicap {
        bool has_handicap;
        std::size_t handicap_index;
    };
    const auto [has_handicap, zero_index] = [&] -> Handicap {
        const std::u8string_view sci_string = scientific.as_string();
        const std::size_t e_index = sci_string.find(u8'e');
        COWEL_ASSERT(e_index != std::u8string_view::npos);
        COWEL_DEBUG_ASSERT(sci_string[e_index + 1] == u8'+' || sci_string[e_index + 1] == u8'-');
        if (sci_string[e_index + 2] == u8'0') {
            return { true, e_index + 2 };
        }
        return { false, 0 };
    }();
    if (fixed.size() + std::size_t(has_handicap) <= scientific.size()) {
        out.write(fixed.as_string(), Output_Language::text);
    }
    else {
        if (has_handicap) {
            scientific.erase(zero_index);
        }
        out.write(scientific.as_string(), Output_Language::text);
    }
}

[[nodiscard]]
Processing_Status Value::splice_block(Content_Policy& out, Context& context) const
{
    COWEL_DEBUG_ASSERT(get_type_kind() == Type_Kind::block);
    switch (m_index) {
    case block_index: {
        return splice_all(out, m_value.block.block->get_elements(), m_value.block.frame, context);
    }
    case directive_index: {
        return splice_directive_invocation(
            out, *m_value.directive.directive, m_value.directive.frame, context
        );
    }
    default: break;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Expected block.");
}

Processing_Status splice_value_to_plaintext(
    std::pmr::vector<char8_t>& out,
    const Value& value,
    const File_Source_Span& error_location,
    Context& context
)
{
    if (value.is_str()) {
        append(out, value.as_string());
        return Processing_Status::ok;
    }
    Capturing_Ref_Text_Sink sink { out, Output_Language::text };
    Text_Only_Policy policy { sink };
    return splice_value(policy, value, error_location, context);
}

Result<Value, Processing_Status> splice_value_to_string(
    const Value& value, //
    const File_Source_Span& error_location,
    Context& context
)
{
    if (value.is_str()) {
        return value;
    }
    Vector_Text_Sink text { Output_Language::text, context.get_transient_memory() };
    Text_Only_Policy policy { text };
    const Processing_Status status = splice_value(policy, value, error_location, context);
    if (status != Processing_Status::ok) {
        return status;
    }
    return Value::string(as_u8string_view(*text), String_Kind::unknown);
}

Processing_Status splice_expression_to_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Expression& value,
    Frame_Index frame,
    Context& context
)
{
    Capturing_Ref_Text_Sink sink { out, Output_Language::text };
    Text_Only_Policy policy { sink };
    return splice_expression(policy, value, frame, context);
}

[[nodiscard]]
Result<Value, Processing_Status>
evaluate_expression(const ast::Expression& value, Frame_Index frame, Context& context)
{
    const auto diagnostic_frame = context.push_diagnostic_frame(frame);
    const auto visitor = [&](const auto& v) { return evaluate(v, frame, context); };
    return std::visit(visitor, value);
}

[[nodiscard]]
Result<Value, Processing_Status>
evaluate(const ast::Directive& directive, Frame_Index frame, Context& context)
{
    Invocation call {
        .name = directive.get_name(),
        .directive = directive,
        .arguments = directive.get_arguments(),
        .content = directive.get_content(),
        .content_frame = frame,
        .call_frame = {},
    };
    const Directive_Behavior* const behavior = context.find_directive(call.name);
    if (!behavior) {
        try_lookup_error(directive, context);
        call.call_frame = context.get_call_stack().get_top_index();
        return Processing_Status::error;
    }

    const Scoped_Frame scope = context.get_call_stack().push_scoped({ *behavior, call });
    call.call_frame = scope.get_index();
    return behavior->evaluate(call, context);
}

[[nodiscard]]
Result<Value, Processing_Status> evaluate(
    const ast::Unary_Expression& expression, //
    const Frame_Index frame,
    Context& context
)
{
    Result<Value, Processing_Status> result
        = evaluate_expression(expression.get_operand(), frame, context);
    if (!result) {
        return result;
    }
    return evaluate_unary(expression.get_kind(), *result, expression.get_source_span(), context);
}

[[nodiscard]]
Result<Value, Processing_Status>
evaluate(const ast::Binary_Expression& expression, const Frame_Index frame, Context& context)
{
    const Binary_Expression_Kind kind = expression.get_kind();
    const File_Source_Span& span = expression.get_source_span();

    // Short-circuit logical operators: evaluate LHS first;
    // if the result determines the outcome, skip RHS evaluation.
    if (kind == Binary_Expression_Kind::logical_or || kind == Binary_Expression_Kind::logical_and) {
        const bool terminator = kind == Binary_Expression_Kind::logical_or;

        auto lhs_result = evaluate_expression(expression.get_lhs(), frame, context);
        if (!lhs_result) {
            return lhs_result;
        }
        if (!lhs_result->is_bool()) {
            context.try_error(
                diagnostic::type_mismatch, span,
                joined_char_sequence(
                    {
                        u8"Expected a value of type "sv,
                        Type::boolean.get_display_name(),
                        u8", but got "sv,
                        lhs_result->get_type().get_display_name(),
                        u8"."sv,
                    }
                )
            );
            return Processing_Status::error;
        }
        if (lhs_result->as_boolean() == terminator) {
            return Value::boolean(terminator);
        }

        auto rhs_result = evaluate_expression(expression.get_rhs(), frame, context);
        if (!rhs_result) {
            return rhs_result;
        }
        if (!rhs_result->is_bool()) {
            context.try_error(
                diagnostic::type_mismatch, span,
                joined_char_sequence(
                    {
                        u8"Expected a value of type "sv,
                        Type::boolean.get_display_name(),
                        u8", but got "sv,
                        rhs_result->get_type().get_display_name(),
                        u8"."sv,
                    }
                )
            );
            return Processing_Status::error;
        }
        return Value::boolean(rhs_result->as_boolean());
    }

    // For all other operators, evaluate both operands unconditionally.
    auto lhs_result = evaluate_expression(expression.get_lhs(), frame, context);
    if (!lhs_result) {
        return lhs_result;
    }
    auto rhs_result = evaluate_expression(expression.get_rhs(), frame, context);
    if (!rhs_result) {
        return rhs_result;
    }

    const Value& lhs = *lhs_result;
    const Value& rhs = *rhs_result;

    const Result<Builtin_Operation_Kind, Processing_Status> operation = check_operation(
        kind, lhs.get_type(), rhs.get_type(), expression.get_lhs().get_source_span(),
        expression.get_rhs().get_source_span(), context
    );
    if (!operation) {
        return operation.error();
    }
    return evaluate_builtin(
        *operation, lhs, rhs, expression.get_lhs().get_source_span(),
        expression.get_rhs().get_source_span(), context
    );
}

Result<Value, Processing_Status> evaluate_unary(
    Unary_Expression_Kind kind, //
    const Value& value,
    const File_Source_Span& error_location,
    Context& context
)
{
    static constexpr Type numeric_types[] { Type::integer, Type::floating };
    static constexpr auto numeric_type = Type::union_of(numeric_types);
    static_assert(numeric_type.is_canonical());

    switch (kind) {
    case Unary_Expression_Kind::bitwise_not: {
        if (!value.is_int()) {
            context.try_error(
                diagnostic::type_mismatch, error_location,
                joined_char_sequence(
                    {
                        u8"Bitwise negation requires an operand of type "sv,
                        Type::integer.get_display_name(),
                        u8", but got "sv,
                        value.get_type().get_display_name(),
                        u8"."sv,
                    }
                )
            );
            return Processing_Status::error;
        }
        return Value::integer(~value.as_integer());
    }

    case Unary_Expression_Kind::logical_not: {
        if (!value.is_bool()) {
            context.try_error(
                diagnostic::type_mismatch, error_location,
                joined_char_sequence(
                    {
                        u8"Logical negation requires an operand of type "sv,
                        Type::boolean.get_display_name(),
                        u8", but got "sv,
                        value.get_type().get_display_name(),
                        u8"."sv,
                    }
                )
            );
            return Processing_Status::error;
        }
        return Value::boolean(!value.as_boolean());
    }

    case Unary_Expression_Kind::plus: {
        if (value.is_int() || value.is_float()) {
            return value;
        }
        context.try_error(
            diagnostic::type_mismatch, error_location,
            joined_char_sequence(
                {
                    u8"Unary plus requires an operand of type "sv,
                    numeric_type.get_display_name(),
                    u8", but got "sv,
                    value.get_type().get_display_name(),
                    u8"."sv,
                }
            )
        );
        return Processing_Status::error;
    }

    case Unary_Expression_Kind::minus: {
        if (value.is_int()) {
            return Value::integer(-value.as_integer());
        }
        if (value.is_float()) {
            return Value::floating(-value.as_float());
        }
        context.try_error(
            diagnostic::type_mismatch, error_location,
            joined_char_sequence(
                {
                    u8"Unary minus requires an operand of type "sv,
                    numeric_type.get_display_name(),
                    u8", but got "sv,
                    value.get_type().get_display_name(),
                    u8"."sv,
                }
            )
        );
        return Processing_Status::error;
    }
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid expression kind.");
}

namespace {

struct Evaluate_Member_Values {
    std::pmr::vector<Group_Member_Value>& out;
    Frame_Index frame;
    Context& context;

    [[nodiscard]]
    Processing_Status operator()(std::span<const ast::Group_Member> members)
    {
        for (const auto& m : members) {
            switch (const auto kind = m.get_kind()) {
            case ast::Member_Kind::ellipsis: {
                const Stack_Frame& ellipsis_frame = context.get_call_stack()[frame];
                const auto status = Evaluate_Member_Values {
                    out, ellipsis_frame.invocation.content_frame, context
                }(ellipsis_frame.invocation.get_arguments_span());
                if (status != Processing_Status::ok) {
                    return status;
                }
                break;
            }

            case ast::Member_Kind::positional:
            case ast::Member_Kind::named: {
                auto name = [&] -> Result<Value, Processing_Status> {
                    if (kind == ast::Member_Kind::positional) {
                        return Value::null;
                    }
                    return evaluate(m.get_name(), frame, context);
                }();
                if (!name) {
                    return name.error();
                }
                Result<Value, Processing_Status> value
                    = evaluate_expression(m.get_value(), frame, context);
                if (!value) {
                    COWEL_DEBUG_ASSERT(value.error() != Processing_Status::ok);
                    return value.error();
                }
                out.push_back({ .name = std::move(*name), .value = std::move(*value) });
                break;
            }
            }
        }
        return Processing_Status::ok;
    }
};

} // namespace

[[nodiscard]]
Result<Value, Processing_Status>
evaluate(const ast::Primary& value, Frame_Index frame, Context& context)
{
    const auto diagnostic_frame = context.push_diagnostic_frame(frame);
    COWEL_ASSERT(value.is_value());

    switch (value.get_kind()) {
    case ast::Primary_Kind::unit_literal: {
        return Value::unit;
    }
    case ast::Primary_Kind::null_literal: {
        return Value::null;
    }
    case ast::Primary_Kind::bool_literal: {
        return Value::boolean(value.get_bool_value());
    }
    case ast::Primary_Kind::int_literal: {
        return Value::integer(value.get_int_value());
    }
    case ast::Primary_Kind::decimal_float_literal: {
        const ast::Parsed_Float parsed = value.get_float_value();
        switch (parsed.status) {
        case ast::Float_Literal_Status::float_overflow: {
            context.try_warning(
                diagnostic::literal_out_of_range, value.get_source_span(),
                joined_char_sequence(
                    {
                        u8"The parsed value is too large to be represented as "sv,
                        Type::floating.get_display_name(),
                        u8" and is rounded to "sv,
                        (parsed.value < 0 ? u8"negative"sv : u8"positive"sv),
                        u8" infinity instead."sv,
                    }
                )
            );
            break;
        }
        case ast::Float_Literal_Status::float_underflow: {
            context.try_warning(
                diagnostic::literal_out_of_range, value.get_source_span(),
                joined_char_sequence(
                    {
                        u8"The parsed value is too small to be represented as "sv,
                        Type::floating.get_display_name(),
                        u8" and is rounded to "sv,
                        (parsed.value < 0 ? u8"negative"sv : u8"positive"sv),
                        u8" zero instead."sv,
                    }
                )
            );
            break;
        }
        case ast::Float_Literal_Status::ok: break;
        }
        return Value::floating(parsed.value);
    }
    case ast::Primary_Kind::infinity: {
        const ast::Parsed_Float parsed = value.get_float_value();
        COWEL_ASSERT(parsed.status == ast::Float_Literal_Status::ok);
        return Value::floating(parsed.value);
    }
    case ast::Primary_Kind::unquoted_member_name: {
        return Value::static_string(value.get_source(), value.get_string_kind());
    }
    case ast::Primary_Kind::id_expression: {
        const Value* const var = context.get_variable(value.get_source());
        if (!var) {
            context.try_error(
                diagnostic::id_lookup, value.get_source_span(),
                joined_char_sequence(
                    {
                        u8"No variable with the name \""sv,
                        value.get_source(),
                        u8"\" was found."sv,
                    }
                )
            );
            return Processing_Status::error;
        }
        return *var;
    }
    case ast::Primary_Kind::quoted_string: {
        Vector_Text_Sink text { Output_Language::text, context.get_transient_memory() };
        Text_Only_Policy policy { text };
        const Processing_Status result = splice_primary(policy, value, frame, context);
        if (result != Processing_Status::ok) {
            return result;
        }
        return Value::string(as_u8string_view(*text), value.get_string_kind());
    }
    case ast::Primary_Kind::block: {
        return Value::block(value, frame);
    }
    case ast::Primary_Kind::group: {
        std::pmr::vector<Group_Member_Value> members;
        members.reserve(value.get_members_size());
        const auto status = Evaluate_Member_Values { members, frame, context }(value.get_members());
        if (status != Processing_Status::ok) {
            return status;
        }
        return Value::group_move(members);
    }
    case ast::Primary_Kind::text:
    case ast::Primary_Kind::comment:
    case ast::Primary_Kind::escape: break;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Unexpected kind of primary.");
}

Processing_Status splice_invocation( //
    Content_Policy& out,
    const ast::Directive& directive,
    std::u8string_view name,
    const ast::Primary* args,
    const ast::Primary* content,
    Frame_Index content_frame,
    Context& context
)
{
    const auto diagnostic_frame = context.push_diagnostic_frame(content_frame);
    Invocation call {
        .name = name,
        .directive = directive,
        .arguments = args,
        .content = content,
        .content_frame = content_frame,
        .call_frame = {},
    };
    const Directive_Behavior* const behavior = context.find_directive(name);
    if (!behavior) {
        try_lookup_error(directive, context);
        call.call_frame = context.get_call_stack().get_top_index();
        return try_generate_error(out, call, context);
    }

    const Scoped_Frame scope = context.get_call_stack().push_scoped({ *behavior, call });
    call.call_frame = scope.get_index();
    return behavior->splice(out, call, context);
}

Processing_Status splice_directive_invocation( //
    Content_Policy& out,
    const ast::Directive& d,
    Frame_Index content_frame,
    Context& context
)
{
    return splice_invocation(
        out, d, d.get_name(), d.get_arguments(), d.get_content(), content_frame, context
    );
}

void diagnose(
    Syntax_Highlight_Error error,
    std::u8string_view lang,
    const Invocation& call,
    Context& context
)
{
    if (!context.emits(Severity::warning)) {
        return;
    }
    switch (error) {
    case Syntax_Highlight_Error::unsupported_language: {
        if (lang.empty()) {
            context.try_warning(
                diagnostic::highlight_language, call.directive.get_source_span(),
                u8"Syntax highlighting was not possible because no language was given, "
                u8"and automatic language detection was not possible. "
                u8"Please use \\tt{...} or \\pre{...} if you want a code (block) "
                u8"without any syntax highlighting."sv
            );
            break;
        }
        const std::u8string_view message[] {
            u8"Unable to apply syntax highlighting because the specified language \"",
            lang,
            u8"\" is not supported.",
        };
        context.emit_warning(
            diagnostic::highlight_language, call.directive.get_source_span(),
            joined_char_sequence(message)
        );
        break;
    }
    case Syntax_Highlight_Error::bad_code: {
        const std::u8string_view message[] {
            u8"Unable to apply syntax highlighting because the code is not valid "
            u8"for the specified language \"",
            lang,
            u8"\".",
        };
        context.emit_warning(
            diagnostic::highlight_malformed, call.directive.get_source_span(),
            joined_char_sequence(message)
        );
        break;
    }
    case Syntax_Highlight_Error::other: {
        const std::u8string_view message[] {
            u8"Unable to apply syntax highlighting because of an internal error.",
            lang,
            u8"\".",
        };
        context.emit_warning(
            diagnostic::highlight_error, call.directive.get_source_span(),
            joined_char_sequence(message)
        );
        break;
    }
    }
}

Result<void, std::size_t> named_str_arguments_to_attributes(
    Text_Buffer_Attribute_Writer& out,
    const std::span<const Group_Member_Value> arguments,
    const Attribute_Style style
)
{
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const auto& [key, value] = arguments[i];
        COWEL_ASSERT(key.is_str());
        COWEL_ASSERT(value.is_str());
        const std::u8string_view key_string = key.as_string();
        if (!is_html_attribute_name(key_string)) {
            return i;
        }
        out.write_attribute(HTML_Attribute_Name(key_string), value.as_string(), style);
    }
    return {};
}

Processing_Status named_arguments_to_attributes_or_error(
    Text_Buffer_Attribute_Writer& out,
    const Group_Pack_Named_Str_Matcher& matcher,
    Context& context,
    Attribute_Style style
)
{
    if (!matcher.was_matched()) {
        return Processing_Status::ok;
    }
    const Result<void, std::size_t> result
        = named_str_arguments_to_attributes(out, matcher.get().get_group_members(), style);
    if (result) {
        return Processing_Status::ok;
    }
    context.try_error(
        diagnostic::html_attribute, matcher.get_location(),
        joined_char_sequence(
            {
                u8"The key \""sv,
                matcher.get().get_group_members()[result.error()].name.as_string(),
                u8"\" is not a valid HTML attribute name."sv,
            }
        )
    );
    return Processing_Status::error;
}

Processing_Status named_arguments_to_attributes_or_error(
    Text_Buffer_Attribute_Writer& out,
    const Pack_Named_Of_Type_Matcher& matcher,
    Context& context,
    Attribute_Style style
)
{
    if (!matcher.was_matched()) {
        return Processing_Status::ok;
    }

    const auto do_named_arguments_to_attributes_or_error
        = [&](std::span<const Group_Member_Value> arguments) -> Processing_Status {
        const Result<void, std::size_t> result
            = named_str_arguments_to_attributes(out, arguments, style);
        if (result) {
            return Processing_Status::ok;
        }
        context.try_error(
            diagnostic::html_attribute, matcher.get_locations()[result.error()],
            joined_char_sequence(
                {
                    u8"The key \""sv,
                    matcher.get()[result.error()].name.as_string(),
                    u8"\" is not a valid HTML attribute name."sv,
                }
            )
        );
        return Processing_Status::error;
    };

    const bool are_arguments_strings = matcher.get_element_type() == Type::str
        || std::ranges::all_of(matcher.get(),
                               [](const Group_Member_Value& val) { return val.value.is_str(); });
    if (are_arguments_strings) {
        return do_named_arguments_to_attributes_or_error(matcher.get());
    }

    Small_Vector<Group_Member_Value, 8> string_values;
    for (std::size_t i = 0; i < matcher.get().size(); ++i) {
        const auto& [name, value] = matcher.get()[i];
        Result<Value, Processing_Status> spliced
            = splice_value_to_string(value, matcher.get_locations()[i], context);
        if (!spliced) {
            return spliced.error();
        }
        string_values.push_back({ .name = name, .value = std::move(*spliced) });
    }
    return do_named_arguments_to_attributes_or_error(string_values);
}

Processing_Status try_generate_error(
    Content_Policy& out,
    const Invocation& call,
    Context& context,
    Processing_Status on_success
)
{
    if (const Directive_Behavior* const behavior = context.get_error_behavior()) {
        const Processing_Status result = behavior->splice(out, call, context);
        if (result != Processing_Status::ok) {
            context.try_error(
                diagnostic::error_error, call.directive.get_source_span(),
                u8"A fatal error was raised because producing a non-fatal error failed."sv
            );
            return Processing_Status::fatal;
        }
        return on_success;
    }
    return on_success;
}

void try_inherit_paragraph(Content_Policy& out)
{
    if (auto* const derived = dynamic_cast<Paragraph_Split_Policy*>(&out)) {
        derived->inherit_paragraph();
    }
}

void try_enter_paragraph(Content_Policy& out)
{
    if (auto* const derived = dynamic_cast<Paragraph_Split_Policy*>(&out)) {
        derived->enter_paragraph();
    }
}

void try_leave_paragraph(Content_Policy& out)
{
    if (auto* const derived = dynamic_cast<Paragraph_Split_Policy*>(&out)) {
        derived->leave_paragraph();
    }
}

void ensure_paragraph_matches_display(Content_Policy& out, Directive_Display display)
{
    switch (display) {
    case Directive_Display::in_line: {
        try_enter_paragraph(out);
        break;
    }
    case Directive_Display::block: {
        try_leave_paragraph(out);
        break;
    }
    case Directive_Display::none: break;
    }
}

} // namespace cowel
