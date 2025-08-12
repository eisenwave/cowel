#include <cstddef>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/chars.hpp"
#include "cowel/util/from_chars.hpp"
#include "cowel/util/html_writer.hpp"
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
#include "cowel/directive_arguments.hpp"
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

Processing_Status invoke( //
    Content_Policy& out,
    std::u8string_view name,
    const ast::Directive& directive,
    const Arguments_View args,
    std::span<const ast::Content> content,
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

namespace {

[[nodiscard]]
bool expand_ellipses_recursively(
    std::pmr::vector<Argument_Ref>& out,
    Arguments_View args,
    Context& context
)
{
    for (const Argument_Ref arg : args) {
        if (arg.ast_node.get_type() != ast::Argument_Type::ellipsis) {
            out.push_back(arg);
            continue;
        }

        if (arg.frame_index == Frame_Index::root) {
            context.try_error(
                diagnostic::ellipsis_outside, arg.ast_node.get_source_span(),
                u8"Ellipsis arguments cannot be used outside of a macro expansion, "
                u8"similar to \\cowel_put."sv
            );
            break;
        }

        const Stack_Frame& frame = context.get_call_stack()[arg.frame_index];
        if (!expand_ellipses_recursively(out, frame.invocation.arguments, context)) {
            return false;
        }
    }
    return true;
}

} // namespace

Processing_Status invoke_directive( //
    Content_Policy& out,
    const ast::Directive& d,
    Frame_Index content_frame,
    Context& context
)
{
    const auto do_invoke = [&](Arguments_View args) {
        return invoke(out, d.get_name(), d, args, d.get_content(), content_frame, context);
    };
    if (!d.has_ellipsis()) {
        return do_invoke(Homogeneous_Call_Arguments { d.get_arguments(), content_frame });
    }

    if (content_frame == Frame_Index::root) {
        for (const ast::Argument& a : d.get_arguments()) {
            if (a.get_type() == ast::Argument_Type::ellipsis) {
                context.try_error(
                    diagnostic::ellipsis_outside, a.get_source_span(),
                    u8"Ellipsis arguments cannot be used outside of a macro expansion, "
                    u8"similar to \\cowel_put."sv
                );
                break;
            }
        }
        // TODO: generate error
        return Processing_Status::error;
    }
    const Call_Stack& stack = context.get_call_stack();
    const Stack_Frame& frame = stack[content_frame];

    // A common, special case is to simply forward all arguments, like \d[...].
    // In this case, we can simply yoink the arguments provided to the macro.
    if (d.get_arguments().size() == 1) {
        const ast::Argument& arg = d.get_arguments().front();
        COWEL_ASSERT(arg.get_type() == ast::Argument_Type::ellipsis);
        return do_invoke(frame.invocation.arguments);
    }

    // TODO: It's likely worth supporting the common case of exactly one ellipsis,
    //       like in \cowel_html_element[div, ...].
    //       For only one ellipsis, we don't need to allocate and could imply form a
    //       joined view of the ellipsis and the surrounding arguments.
    //
    //       Currently, which is not ideal, we recursively flatten all the expansions
    //       into a vector, which requires dynamic allocations.
    //       I suspect the performance downsides are relatively minimal though.

    std::pmr::vector<Argument_Ref> recursively_flattened_args { context.get_transient_memory() };
    recursively_flattened_args.reserve(64);

    for (const ast::Argument& arg : d.get_arguments()) {
        if (arg.get_type() != ast::Argument_Type::ellipsis) {
            recursively_flattened_args.push_back({ arg, content_frame });
            continue;
        }
        if (!expand_ellipses_recursively(
                recursively_flattened_args, frame.invocation.arguments, context
            )) {
            // TODO: generate error
            return Processing_Status::error;
        }
    }

    return do_invoke(Heterogeneous_Call_Arguments { recursively_flattened_args });
}

void warn_all_args_ignored(const Invocation& call, Context& context)
{
    if (context.emits(Severity::warning)) {
        for (Argument_Ref arg : call.arguments) {
            context.emit_warning(
                diagnostic::ignored_args, arg.ast_node.get_source_span(),
                u8"This argument was ignored."sv
            );
        }
    }
}

void warn_ignored_argument_subset(
    Arguments_View args,
    const Argument_Matcher& matcher,
    Context& context,
    Argument_Subset ignored_subset
)
{
    const std::span<const Argument_Status> statuses = matcher.argument_statuses();
    COWEL_ASSERT(args.size() == statuses.size());

    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];
        const bool is_matched = statuses[i] != Argument_Status::unmatched;
        const bool is_named = arg.ast_node.get_type() == ast::Argument_Type::named;
        const Argument_Subset subset = argument_subset_matched_named(is_matched, is_named);
        if (argument_subset_contains(ignored_subset, subset)) {
            context.try_warning(
                diagnostic::ignored_args, arg.ast_node.get_source_span(),
                u8"This argument was ignored."sv
            );
        }
    }
}

void warn_ignored_argument_subset(
    Arguments_View args,
    Context& context,
    Argument_Subset ignored_subset
)
{
    COWEL_ASSERT(
        argument_subset_contains(ignored_subset, Argument_Subset::matched)
        == argument_subset_contains(ignored_subset, Argument_Subset::unmatched)
    );

    for (const auto& arg : args) {
        const auto subset = arg.ast_node.get_type() == ast::Argument_Type::named
            ? Argument_Subset::named
            : Argument_Subset::positional;
        if (argument_subset_contains(ignored_subset, subset)) {
            context.try_warning(
                diagnostic::ignored_args, arg.ast_node.get_source_span(),
                u8"This argument was ignored."sv
            );
        }
    }
}

void warn_deprecated_directive_names(std::span<const ast::Content> content, Context& context)
{
    for (const ast::Content& c : content) {
        if (const auto* const d = std::get_if<ast::Directive>(&c)) {
            if (d->get_name().contains(u8'-')) {
                context.try_warning(
                    diagnostic::deprecated, d->get_name_span(),
                    u8"The use of '-' in directive names is deprecated."sv
                );
            }
            for (const ast::Argument& arg : d->get_arguments()) {
                warn_deprecated_directive_names(arg.get_content(), context);
            }
            warn_deprecated_directive_names(d->get_content(), context);
        }
    }
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
    Arguments_View arguments,
    Context& context,
    Argument_Filter filter,
    Attribute_Style style
)
{
    std::size_t i = 0;
    return process_greedy(arguments, [&](const Argument_Ref a) -> Processing_Status {
        if (a.ast_node.get_type() != ast::Argument_Type::named) {
            return Processing_Status::ok;
        }
        const bool passed_filter = !filter || filter(i, a);
        ++i;
        return passed_filter ? named_argument_to_attribute(out, a, context, style)
                             : Processing_Status::ok;
    });
}

Processing_Status named_arguments_to_attributes(
    Text_Buffer_Attribute_Writer& out,
    Arguments_View arguments,
    const Argument_Matcher& matcher,
    Context& context,
    Argument_Subset subset,
    Attribute_Style style
)
{
    COWEL_ASSERT(!argument_subset_intersects(subset, Argument_Subset::positional));

    const auto filter = [&](std::size_t index, const Argument_Ref a) -> bool {
        const Argument_Status status = matcher.argument_statuses()[index];
        if (status == Argument_Status::duplicate_named) {
            const std::u8string_view message[] = {
                u8"This argument is a duplicate of a previous named argument also named \"",
                a.ast_node.get_name(),
                u8"\", and will be ignored.",
            };
            context.try_warning(
                diagnostic::duplicate_args, a.ast_node.get_source_span(),
                joined_char_sequence(message)
            );
            return false;
        }
        const auto arg_subset = status == Argument_Status::unmatched
            ? Argument_Subset::unmatched_named
            : Argument_Subset::matched_named;
        return argument_subset_contains(subset, arg_subset);
    };
    return named_arguments_to_attributes(out, arguments, context, filter, style);
}

Processing_Status named_argument_to_attribute(
    Text_Buffer_Attribute_Writer& out,
    Argument_Ref a,
    Context& context,
    Attribute_Style style
)
{
    COWEL_ASSERT(a.ast_node.get_type() == ast::Argument_Type::named);
    std::pmr::vector<char8_t> value { context.get_transient_memory() };
    // TODO: error handling
    value.clear();
    const auto status = to_plaintext(value, a.ast_node.get_content(), a.frame_index, context);
    const std::u8string_view value_string { value.data(), value.size() };
    const std::u8string_view name = a.ast_node.get_name();
    // TODO: this is simply going to crash if the attribute name is not valid;
    // investigate whether this needs work, and possibly leave a comment here if it's okay
    out.write_attribute(HTML_Attribute_Name(name), value_string, style);
    return status;
}

Result<bool, Processing_Status> argument_to_plaintext(
    std::pmr::vector<char8_t>& out,
    Arguments_View arguments,
    const Argument_Matcher& args,
    std::u8string_view parameter,
    Context& context
)
{
    const int i = args.get_argument_index(parameter);
    if (i < 0) {
        return false;
    }
    const Argument_Ref& arg = arguments[std::size_t(i)];
    // TODO: warn when pure HTML argument was used as variable name
    const auto status = to_plaintext(out, arg.ast_node.get_content(), arg.frame_index, context);
    if (status != Processing_Status::ok) {
        return status;
    }
    return true;
}

std::optional<Argument_Ref> get_first_positional_warn_rest(Arguments_View args, Context& context)
{
    std::optional<Argument_Ref> result;
    for (const Argument_Ref arg : args) {
        if (arg.ast_node.get_type() != ast::Argument_Type::positional) {
            continue;
        }
        if (!result) {
            result.emplace(arg);
            continue;
        }
        context.try_warning(
            diagnostic::ignored_args, arg.ast_node.get_source_span(),
            u8"This positional argument is ignored. "
            u8"Only the first positional argument is used in this directive."sv
        );
    }
    return result;
}

Greedy_Result<bool> get_yes_no_argument(
    std::u8string_view name,
    std::u8string_view diagnostic_id,
    Arguments_View arguments,
    const Argument_Matcher& args,
    Context& context,
    bool fallback
)
{
    const int index = args.get_argument_index(name);
    if (index < 0) {
        return fallback;
    }
    const Argument_Ref arg = arguments[std::size_t(index)];
    std::pmr::vector<char8_t> data { context.get_transient_memory() };
    const auto text_status
        = to_plaintext(data, arg.ast_node.get_content(), arg.frame_index, context);
    if (text_status != Processing_Status::ok) {
        return { fallback, text_status };
    }

    const auto string = as_u8string_view(data);
    if (string == u8"yes") {
        return true;
    }
    if (string == u8"no") {
        return false;
    }
    const std::u8string_view message[] {
        u8"Argument has to be \"yes\" or \"no\", but \"",
        string,
        u8"\" was given.",
    };
    context.try_warning(
        diagnostic_id, arg.ast_node.get_source_span(), joined_char_sequence(message)
    );
    return fallback;
}

Greedy_Result<std::size_t> get_integer_argument(
    std::u8string_view name,
    std::u8string_view parse_error_diagnostic,
    std::u8string_view range_error_diagnostic,
    Arguments_View arguments,
    const Argument_Matcher& args,
    Context& context,
    std::size_t fallback,
    std::size_t min,
    std::size_t max
)
{
    COWEL_ASSERT(fallback >= min && fallback <= max);

    const int index = args.get_argument_index(name);
    if (index < 0) {
        return fallback;
    }
    const Argument_Ref arg = arguments[std::size_t(index)];
    std::pmr::vector<char8_t> arg_text { context.get_transient_memory() };
    const auto text_status
        = to_plaintext(arg_text, arg.ast_node.get_content(), arg.frame_index, context);
    if (text_status != Processing_Status::ok) {
        return { fallback, text_status };
    }
    const auto arg_string = as_u8string_view(arg_text);

    const std::optional<std::size_t> value = from_chars<std::size_t>(arg_string);
    if (!value) {
        const std::u8string_view message[] {
            u8"The specified ",
            name,
            u8" \"",
            arg_string,
            u8"\" is ignored because it could not be parsed as a (positive) integer.",
        };
        context.try_warning(
            parse_error_diagnostic, arg.ast_node.get_source_span(), joined_char_sequence(message)
        );
        return fallback;
    }
    if (value < min || value > max) {
        const Characters8 min_chars = to_characters8(min);
        const Characters8 max_chars = to_characters8(max);
        const std::u8string_view message[] {
            u8"The specified ",
            name,
            u8" \"",
            arg_string,
            u8"\" is ignored because it is outside of the valid range [",
            min_chars.as_string(),
            u8", ",
            max_chars.as_string(),
            u8"].",
        };
        context.try_warning(
            range_error_diagnostic, arg.ast_node.get_source_span(), joined_char_sequence(message)
        );
        return fallback;
    }

    return *value;
}

Greedy_Result<String_Argument> get_string_argument(
    std::u8string_view name,
    Arguments_View arguments,
    const Argument_Matcher& args,
    Context& context,
    std::u8string_view fallback
)
{
    String_Argument result { .data = std::pmr::vector<char8_t>(context.get_transient_memory()),
                             .string = {} };
    const int index = args.get_argument_index(name);
    if (index < 0) {
        result.string = fallback;
        return result;
    }
    const Argument_Ref arg = arguments[std::size_t(index)];
    const auto status
        = to_plaintext(result.data, arg.ast_node.get_content(), arg.frame_index, context);
    if (status != Processing_Status::ok) {
        result.string = fallback;
        return { result, status };
    }
    result.string = as_u8string_view(result.data);
    return result;
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
