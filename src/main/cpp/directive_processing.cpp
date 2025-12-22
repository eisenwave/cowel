#include <cmath>
#include <cstddef>
#include <cstring>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/chars.hpp"
#include "cowel/util/html_writer.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/to_chars.hpp"

#include "cowel/policy/capture.hpp"
#include "cowel/policy/content_policy.hpp"
#include "cowel/policy/paragraph_split.hpp"
#include "cowel/policy/plaintext.hpp"

#include "cowel/ast.hpp"
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

using namespace std::string_view_literals;

namespace cowel {

Processing_Status
Directive_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    const Result<Value, Processing_Status> result = evaluate(call, context);
    if (!result) {
        return try_generate_error(out, call, context, result.error());
    }
    return splice_value(out, *result, context);
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
    const Result<Integer, Processing_Status> result = do_evaluate(call, context);
    if (!result) {
        return result.error();
    }
    return Value::integer(*result);
}

Processing_Status
Int_Directive_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    const Result<Integer, Processing_Status> result = do_evaluate(call, context);
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
    return Value::short_string(*result);
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
const Type& get_static_type(const ast::Member_Value& v, Context& context)
{
    if (const auto* const d = v.try_as_directive()) {
        return get_static_type(*d, context);
    }
    return get_type(v.as_primary());
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
    static const Type group_anything = Type::group_of({ Type::pack_of(auto(Type::any)) });

    switch (primary.get_kind()) {
    case ast::Primary_Kind::unit_literal: return Type::unit;
    case ast::Primary_Kind::null_literal: return Type::null;
    case ast::Primary_Kind::bool_literal: return Type::boolean;
    case ast::Primary_Kind::int_literal: return Type::integer;
    case ast::Primary_Kind::decimal_float_literal:
    case ast::Primary_Kind::infinity: return Type::floating;
    case ast::Primary_Kind::unquoted_string:
    case ast::Primary_Kind::quoted_string: return Type::str;
    case ast::Primary_Kind::block: return Type::block;
    case ast::Primary_Kind::group: return group_anything;

    case ast::Primary_Kind::text:
    case ast::Primary_Kind::comment:
    case ast::Primary_Kind::escape:
        COWEL_ASSERT_UNREACHABLE(u8"Expected a value, not a markup element.");
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid primary kind.");
}

std::u8string_view expand_escape(std::u8string_view escape)
{
    COWEL_ASSERT(!escape.empty());
    COWEL_DEBUG_ASSERT(is_cowel_escapeable(escape[0]));
    return escape[0] == u8'\r' || escape[0] == u8'\n' ? u8"" : escape;
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

Processing_Status splice_all_trimmed(
    Content_Policy& out,
    std::span<const ast::Markup_Element> content,
    Frame_Index frame,
    Context& context
)
{
    content = trim_blank_text(content);

    struct Visitor {
        Content_Policy& out;
        Context& context;
        std::size_t i;
        std::size_t size;
        Frame_Index frame;

        void write_trimmed(std::u8string_view str) const
        {
            // Note that the following two conditions are not mutually exclusive
            // when content contains just one element.
            if (i == 0) {
                str = trim_ascii_blank_left(str);
            }
            if (i + 1 == size) {
                str = trim_ascii_blank_right(str);
            }
            // The trimming above should have gotten rid of entirely empty strings.
            COWEL_ASSERT(!str.empty());
            out.write(str, Output_Language::text);
        }

        [[nodiscard]]
        Processing_Status operator()(const ast::Primary& node) const
        {
            if (node.get_kind() != ast::Primary_Kind::text) {
                return out.consume(node, frame, context);
            }
            write_trimmed(node.get_source());
            return Processing_Status::ok;
        }

        [[nodiscard]]
        Processing_Status operator()(const ast::Directive& e) const
        {
            return out.consume(e, frame, context);
        }
    };

    return process_greedy(
        content,
        [&, i = 0uz](const ast::Markup_Element& c) mutable -> Processing_Status {
            const auto result = std::visit(Visitor { out, context, i, content.size(), frame }, c);
            ++i;
            return result;
        }
    );
}

Processing_Status splice_to_plaintext(
    std::pmr::vector<char8_t>& out,
    std::span<const ast::Markup_Element> content,
    Frame_Index frame,
    Context& context
)
{
    Capturing_Ref_Text_Sink sink { out, Output_Language::text };
    Plaintext_Content_Policy policy { sink };
    return splice_all(policy, content, frame, context);
}

[[nodiscard]]
Processing_Status splice_value(
    Content_Policy& out,
    const ast::Member_Value& value,
    Frame_Index frame,
    Context& context
)
{
    if (const auto* const directive = value.try_as_directive()) {
        return splice_directive_invocation(out, *directive, frame, context);
    }
    return splice_primary(out, value.as_primary(), frame, context);
}

[[nodiscard]]
Processing_Status splice_primary(
    Content_Policy& out,
    const ast::Primary& primary,
    Frame_Index frame,
    Context& context
)
{
    COWEL_ASSERT(primary.is_spliceable_value());

    switch (primary.get_kind()) {
    case ast::Primary_Kind::unit_literal:
    case ast::Primary_Kind::null_literal:
    case ast::Primary_Kind::bool_literal:
    case ast::Primary_Kind::unquoted_string: {
        out.write(primary.get_source(), Output_Language::text);
        return Processing_Status::ok;
    }
    case ast::Primary_Kind::int_literal:
    case ast::Primary_Kind::decimal_float_literal: {
        const Result<Value, Processing_Status> value = evaluate(primary, frame, context);
        COWEL_ASSERT(value);
        return splice_value(out, *value, context);
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

Processing_Status splice_value(Content_Policy& out, const Value& value, Context& context)
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
    case Type_Kind::group:
    case Type_Kind::null: {
        // FIXME: print diagnostic
        return Processing_Status::error;
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

void splice_bool(Content_Policy& out, bool value)
{
    const auto str = value ? u8"true"sv : u8"false"sv;
    out.write(str, Output_Language::text);
}

void splice_int(Content_Policy& out, Integer value)
{
    const auto chars = to_characters8(value);
    out.write(chars.as_string(), Output_Language::text);
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
    if (const auto* const block_and_frame = std::get_if<Block_And_Frame>(&m_value)) {
        return splice_all(
            out, block_and_frame->block->get_elements(), block_and_frame->frame, context
        );
    }
    if (const auto* const dir_and_frame = std::get_if<Directive_And_Frame>(&m_value)) {
        return splice_directive_invocation(
            out, *dir_and_frame->directive, dir_and_frame->frame, context
        );
    }
    COWEL_ASSERT_UNREACHABLE(u8"Expected block.");
}

Processing_Status
splice_value_to_plaintext(std::pmr::vector<char8_t>& out, const Value& value, Context& context)
{
    Capturing_Ref_Text_Sink sink { out, Output_Language::text };
    Plaintext_Content_Policy policy { sink };
    return splice_value(policy, value, context);
}

Processing_Status splice_value_to_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Member_Value& value,
    Frame_Index frame,
    Context& context
)
{
    COWEL_DEBUG_ASSERT(value.is_spliceable_value());

    Capturing_Ref_Text_Sink sink { out, Output_Language::text };
    Plaintext_Content_Policy policy { sink };
    return splice_value(policy, value, frame, context);
}

[[nodiscard]]
Plaintext_Result splice_value_to_plaintext_optimistic(
    std::pmr::vector<char8_t>& buffer,
    const ast::Member_Value& value,
    Frame_Index frame,
    Context& context
)
{
    const auto visitor
        = [&](const auto& v) { return splice_to_plaintext_optimistic(buffer, v, frame, context); };
    return std::visit(visitor, value);
}

Plaintext_Result splice_to_plaintext_optimistic(
    std::pmr::vector<char8_t>& buffer,
    const ast::Primary& value,
    Frame_Index frame,
    Context& context
)
{
    COWEL_ASSERT(buffer.empty());
    switch (value.get_kind()) {
    case ast::Primary_Kind::unit_literal: {
        return { Processing_Status::ok, {} };
    }

    case ast::Primary_Kind::null_literal:
    case ast::Primary_Kind::bool_literal:
    // FIXME: simply taking the original source representation is wrong for int and float
    case ast::Primary_Kind::int_literal:
    case ast::Primary_Kind::decimal_float_literal:
    case ast::Primary_Kind::infinity:
    case ast::Primary_Kind::unquoted_string: {
        return { Processing_Status::ok, value.get_source() };
    }

    case ast::Primary_Kind::quoted_string:
    case ast::Primary_Kind::block: {
        if (value.get_elements().empty()) {
            return { Processing_Status::ok, u8""sv };
        }
        if (value.get_elements().size() == 1) {
            if (const auto* const text = value.get_elements().front().try_as_primary()) {
                if (text->get_kind() == ast::Primary_Kind::text) {
                    return { Processing_Status::ok, text->get_source() };
                }
            }
        }
        const Processing_Status status
            = splice_to_plaintext(buffer, value.get_elements(), frame, context);
        return { status, as_u8string_view(buffer) };
    }

    case ast::Primary_Kind::text:
    case ast::Primary_Kind::comment:
    case ast::Primary_Kind::escape: {
        COWEL_ASSERT_UNREACHABLE(u8"Conversion from markup element to text requested.");
    }
    case ast::Primary_Kind::group: {
        COWEL_ASSERT_UNREACHABLE(u8"Conversion from group to text requested.");
    }
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid primary kind.");
}

Plaintext_Result splice_to_plaintext_optimistic(
    std::pmr::vector<char8_t>& buffer,
    const ast::Directive& directive,
    Frame_Index frame,
    Context& context
)
{
    // TODO: There's nothing actually optimistic about this.
    //       However, we could special-case certain directive behaviors
    //       which are known to produce short string constants.
    Capturing_Ref_Text_Sink sink { buffer, Output_Language::text };
    Plaintext_Content_Policy out { sink };
    const Processing_Status status = splice_directive_invocation(out, directive, frame, context);
    return { .status = status, .string = as_u8string_view(buffer) };
}

[[nodiscard]]
Result<Value, Processing_Status>
evaluate_member_value(const ast::Member_Value& value, Frame_Index frame, Context& context)
{
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
Result<Value, Processing_Status>
evaluate(const ast::Primary& value, Frame_Index frame, Context& context)
{
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
        const ast::Parsed_Int parsed = value.get_int_value();
        if (!parsed.in_range) {
            context.try_error(
                diagnostic::literal_out_of_range, value.get_source_span(),
                u8"The parsed value exceeds the implementation limit. "
                u8"Currently, at most signed 128-bit integers are supported."sv
            );
            return Processing_Status::error;
        }
        return Value::integer(parsed.value);
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
    case ast::Primary_Kind::unquoted_string: {
        return Value::static_string(value.get_source());
    }
    case ast::Primary_Kind::quoted_string: {
        std::pmr::vector<char8_t> text { context.get_transient_memory() };
        const Plaintext_Result result = splice_to_plaintext_optimistic(text, value, frame, context);
        if (result.status != Processing_Status::ok) {
            return result.status;
        }
        return text.empty() ? Value::static_string(result.string)
                            : Value::dynamic_string(std::move(text));
    }
    case ast::Primary_Kind::block: {
        return Value::block(value, frame);
    }
    case ast::Primary_Kind::group: {
        COWEL_ASSERT_UNREACHABLE(u8"Sorry, values of group type not yet supported :(");
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

Processing_Status
match_empty_arguments(const Invocation& call, Context& context, Processing_Status fail_status)
{
    Empty_Pack_Matcher args_matcher;
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto fail_callback = fail_status == Processing_Status::fatal
        ? make_fail_callback<Severity::fatal>()
        : make_fail_callback<Severity::error>();

    return call_matcher.match_call(call, context, fail_callback, fail_status);
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

Processing_Status named_arguments_to_attributes(
    Text_Buffer_Attribute_Writer& out,
    std::span<const ast::Group_Member> arguments,
    Frame_Index frame,
    Context& context,
    Attribute_Style style
)
{
    return process_greedy(arguments, [&](const ast::Group_Member& a) -> Processing_Status {
        if (a.get_kind() == ast::Member_Kind::ellipsis) {
            const Stack_Frame& ellipsis_frame = context.get_call_stack()[frame];
            return named_arguments_to_attributes(
                out, ellipsis_frame.invocation.get_arguments_span(),
                ellipsis_frame.invocation.content_frame, context, style
            );
        }
        COWEL_ASSERT(a.get_kind() == ast::Member_Kind::named);
        return named_argument_to_attribute(out, a, frame, context, style);
    });
}

Processing_Status named_argument_to_attribute(
    Text_Buffer_Attribute_Writer& out,
    const ast::Group_Member& a,
    Frame_Index frame,
    Context& context,
    Attribute_Style style
)
{
    COWEL_ASSERT(a.get_kind() == ast::Member_Kind::named);

    std::pmr::vector<char8_t> value { context.get_transient_memory() };
    // TODO: error handling
    value.clear();
    const auto status = splice_value_to_plaintext(value, a.get_value(), frame, context);
    const std::u8string_view value_string { value.data(), value.size() };
    const std::u8string_view name = a.get_name();
    // TODO: this is simply going to crash if the attribute name is not valid;
    // investigate whether this needs work, and possibly leave a comment here if it's okay
    out.write_attribute(HTML_Attribute_Name(name), value_string, style);
    return status;
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
