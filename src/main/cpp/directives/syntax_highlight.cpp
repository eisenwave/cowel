#include <optional>
#include <string_view>
#include <vector>

#include "ulight/ulight.hpp"

#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/html_writer.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/policy/capture.hpp"
#include "cowel/policy/content_policy.hpp"
#include "cowel/policy/factory.hpp"
#include "cowel/policy/html.hpp"
#include "cowel/policy/syntax_highlight.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_arguments.hpp"
#include "cowel/directive_display.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/invocation.hpp"
#include "cowel/output_language.hpp"
#include "cowel/services.hpp"
#include "cowel/theme_to_css.hpp"

using namespace std::string_view_literals;

namespace cowel {

Processing_Status
Code_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context) const
{
    Argument_Matcher args { parameters, context.get_transient_memory() };
    args.match(call.arguments);

    const Greedy_Result<String_Argument> lang
        = get_string_argument(lang_parameter, call.arguments, args, context);
    if (status_is_break(lang.status())) {
        return lang.status();
    }

    const Greedy_Result<String_Argument> prefix
        = get_string_argument(prefix_parameter, call.arguments, args, context);
    if (status_is_break(prefix.status())) {
        return prefix.status();
    }

    const Greedy_Result<String_Argument> suffix
        = get_string_argument(suffix_parameter, call.arguments, args, context);
    if (status_is_break(suffix.status())) {
        return suffix.status();
    }

    const auto borders = m_display == Directive_Display::in_line
        ? Greedy_Result<bool> { true }
        : get_yes_no_argument(
              borders_parameter, diagnostic::codeblock::borders_invalid, call.arguments, args,
              context, true
          );
    if (status_is_break(borders.status())) {
        return borders.status();
    }

    const auto nested = m_display == Directive_Display::block
        ? Greedy_Result<bool> { false }
        : get_yes_no_argument(
              nested_parameter, diagnostic::code::nested_invalid, call.arguments, args, context,
              false
          );
    if (status_is_break(nested.status())) {
        return nested.status();
    }
    const Processing_Status args_status = status_concat(
        lang.status(), prefix.status(), suffix.status(), borders.status(), nested.status()
    );

    // While syntax highlighting generally outputs HTML,
    // when plaintext content is needed (e.g. for ID synthesis),
    // we still want \code to be "transparent" by simply outputting plaintext.
    // Note that for consistent side effects,
    // we still process all the arguments above.
    if (out.get_language() == Output_Language::text) {
        const Processing_Status text_status
            = consume_all(out, call.content, call.content_frame, context);
        return status_concat(args_status, text_status);
    }

    ensure_paragraph_matches_display(out, m_display);

    const bool should_trim = m_pre_compat_trim == Pre_Trimming::yes;

    // All content written to out is HTML anyway,
    // so we don't need an extra HTML_Content_Policy here.
    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    const bool has_enclosing_tags = !*nested;

    if (has_enclosing_tags) {
        auto attributes = writer.open_tag_with_attributes(m_tag_name);
        if (!*borders) {
            COWEL_ASSERT(m_display != Directive_Display::in_line);
            attributes.write_class(u8"borderless"sv);
        }
        attributes.end();
    }

    Syntax_Highlight_Policy highlight_policy //
        { context.get_transient_memory() };
    highlight_policy.write_phantom(prefix->string);
    const auto highlight_status
        = consume_all(highlight_policy, call.content, call.content_frame, context);
    highlight_policy.write_phantom(suffix->string);

    const Result<void, Syntax_Highlight_Error> result = [&] {
        if (!should_trim) {
            return highlight_policy.dump_html_to(buffer, context, lang->string);
        }
        Vector_Text_Sink vector_sink { Output_Language::html, context.get_transient_memory() };
        const Result<void, Syntax_Highlight_Error> result
            = highlight_policy.dump_html_to(vector_sink, context, lang->string);

        // https://html.spec.whatwg.org/dev/grouping-content.html#the-pre-element
        // Leading newlines immediately following <pre> are stripped anyway.
        // The same applies to any elements styled "white-space: pre".
        // In general, it is best to remove these.
        // To ensure portability, we need to trim away newlines (if any).
        auto inner_html = as_u8string_view(*vector_sink);
        while (inner_html.starts_with(u8'\n')) {
            inner_html.remove_prefix(1);
        }
        while (inner_html.ends_with(u8'\n')) {
            inner_html.remove_suffix(1);
        }
        buffer.write(inner_html, Output_Language::html);
        return result;
    }();
    if (!result) {
        diagnose(result.error(), lang->string, call, context);
    }

    if (has_enclosing_tags) {
        writer.close_tag(m_tag_name);
    }
    buffer.flush();

    return status_concat(args_status, highlight_status);
}

Processing_Status
Highlight_As_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context)
    const
{
    Argument_Matcher args { parameters, context.get_transient_memory() };
    args.match(call.arguments);

    std::pmr::vector<char8_t> name_data { context.get_transient_memory() };
    const Result<bool, Processing_Status> has_name_result
        = argument_to_plaintext(name_data, call.arguments, args, name_parameter, context);
    if (!has_name_result) {
        return has_name_result.error();
    }
    if (!*has_name_result) {
        context.try_error(
            diagnostic::highlight_name_missing, call.directive.get_source_span(),
            u8"A name parameter is required to specify the kind of highlight to apply."sv
        );
        return try_generate_error(out, call, context);
    }
    const auto name_string = as_u8string_view(name_data);

    const std::optional<ulight::Highlight_Type> type = highlight_type_by_long_string(name_string);
    if (!type) {
        const std::u8string_view message[] {
            u8"The given highlight name \"",
            name_string,
            u8"\" is not a valid ulight highlight name (long form).",
        };
        context.try_error(
            diagnostic::highlight_name_invalid, call.directive.get_source_span(),
            joined_char_sequence(message)
        );
        return try_generate_error(out, call, context);
    }

    const std::u8string_view short_name = ulight::highlight_type_short_string_u8(*type);
    COWEL_ASSERT(!short_name.empty());

    // We do the same special casing as for \code (see above for explanation).
    if (out.get_language() == Output_Language::text) {
        return consume_all(out, call.content, call.content_frame, context);
    }

    HTML_Content_Policy policy = ensure_html_policy(out);
    HTML_Writer_Buffer buffer { policy, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    writer
        .open_tag_with_attributes(html_tag::h_) //
        .write_attribute(html_attr::data_h, short_name)
        .end();
    buffer.flush();
    const Processing_Status result = consume_all(policy, call.content, call.content_frame, context);
    if (status_is_break(result)) {
        return result;
    }
    writer.close_tag(html_tag::h_);
    buffer.flush();
    return result;
}

} // namespace cowel
