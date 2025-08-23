#include <cstddef>
#include <cstring>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include "cowel/parameters.hpp"
#include "cowel/util/assert.hpp"
#include "cowel/util/chars.hpp"
#include "cowel/util/html_writer.hpp"
#include "cowel/util/strings.hpp"

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

using namespace std::string_view_literals;

namespace cowel {

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

std::span<const ast::Content> trim_blank_text_left(std::span<const ast::Content> content)
{
    while (!content.empty()) {
        if (const auto* const text = std::get_if<ast::Text>(&content.front())) {
            if (is_ascii_blank(text->get_source())) {
                content = content.subspan(1);
                continue;
            }
        }
        if (const auto* const text = std::get_if<ast::Generated>(&content.front())) {
            if (is_ascii_blank(text->as_string())) {
                content = content.subspan(1);
                continue;
            }
        }
        break;
    }
    return content;
}

std::span<const ast::Content> trim_blank_text_right(std::span<const ast::Content> content)
{
    while (!content.empty()) {
        if (const auto* const text = std::get_if<ast::Text>(&content.back())) {
            if (is_ascii_blank(text->get_source())) {
                content = content.subspan(0, content.size() - 1);
                continue;
            }
        }
        if (const auto* const text = std::get_if<ast::Generated>(&content.back())) {
            if (is_ascii_blank(text->as_string())) {
                content = content.subspan(0, content.size() - 1);
                continue;
            }
        }
        break;
    }
    return content;
}

std::span<const ast::Content> trim_blank_text(std::span<const ast::Content> content)
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

Processing_Status consume_all_trimmed(
    Content_Policy& out,
    std::span<const ast::Content> content,
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
        Processing_Status operator()(const ast::Text& text) const
        {
            write_trimmed(text.get_source());
            return Processing_Status::ok;
        }

        [[nodiscard]]
        Processing_Status operator()(const ast::Comment& c) const
        {
            return out.consume(c, frame, context);
        }

        [[nodiscard]]
        Processing_Status operator()(const ast::Generated& g) const
        {
            if (g.get_type() == Output_Language::text) {
                write_trimmed(g.as_string());
                return Processing_Status::ok;
            }
            return out.consume(g, frame, context);
        }

        [[nodiscard]]
        Processing_Status operator()(const ast::Escaped& e) const
        {
            return out.consume(e, frame, context);
        }

        [[nodiscard]]
        Processing_Status operator()(const ast::Directive& e) const
        {
            return out.consume(e, frame, context);
        }
    };

    return process_greedy(
        content,
        [&, i = 0uz](const ast::Content& c) mutable -> Processing_Status {
            const auto result = std::visit(Visitor { out, context, i, content.size(), frame }, c);
            ++i;
            return result;
        }
    );
}

Processing_Status to_plaintext(
    std::pmr::vector<char8_t>& out,
    std::span<const ast::Content> content,
    Frame_Index frame,
    Context& context
)
{
    Capturing_Ref_Text_Sink sink { out, Output_Language::text };
    Plaintext_Content_Policy policy { sink };
    return consume_all(policy, content, frame, context);
}

Processing_Status to_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Value& value,
    Frame_Index frame,
    Context& context
)
{
    Capturing_Ref_Text_Sink sink { out, Output_Language::text };
    Plaintext_Content_Policy policy { sink };
    return consume_all(policy, value, frame, context);
}

Plaintext_Result to_plaintext_optimistic(
    std::pmr::vector<char8_t>& buffer,
    std::span<const ast::Content> content,
    Frame_Index frame,
    Context& context
)
{
    COWEL_ASSERT(buffer.empty());
    if (content.empty()) {
        return { Processing_Status::ok, u8""sv };
    }
    if (content.size() == 1) {
        if (const auto* const text = std::get_if<ast::Text>(content.data())) {
            return { Processing_Status::ok, text->get_source() };
        }
    }
    const Processing_Status status = to_plaintext(buffer, content, frame, context);
    return { status, as_u8string_view(buffer) };
}

Processing_Status invoke( //
    Content_Policy& out,
    const ast::Directive& directive,
    std::u8string_view name,
    const ast::Group* args,
    const ast::Content_Sequence* content,
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
    return (*behavior)(out, call, context);
}

Processing_Status invoke_directive( //
    Content_Policy& out,
    const ast::Directive& d,
    Frame_Index content_frame,
    Context& context
)
{
    return invoke(out, d, d.get_name(), d.get_arguments(), d.get_content(), content_frame, context);
}

[[nodiscard]]
const ast::Content_Sequence*
as_content_or_error(const ast::Value& value, Context& context, Severity error_severity)
{
    const auto* const arg_content = std::get_if<ast::Content_Sequence>(&value);
    if (!arg_content) {
        COWEL_ASSERT(std::holds_alternative<ast::Group>(value));
        context.try_emit(
            error_severity, diagnostic::type_mismatch, value.get_source_span(),
            u8"Expected value to be content sequence, but found group."sv
        );
        return nullptr;
    }
    return arg_content;
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
        COWEL_ASSERT(a.get_kind() != ast::Member_Kind::positional);
        if (a.get_kind() == ast::Member_Kind::ellipsis) {
            const Stack_Frame& ellipsis_frame = context.get_call_stack()[frame];
            return named_arguments_to_attributes(
                out, ellipsis_frame.invocation.get_arguments_span(),
                ellipsis_frame.invocation.content_frame, context, style
            );
        }
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
    const auto status = to_plaintext(value, a.get_value(), frame, context);
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
        const Processing_Status result = (*behavior)(out, call, context);
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
