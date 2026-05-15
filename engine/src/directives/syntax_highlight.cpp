#include <optional>
#include <string_view>

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

#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_display.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/invocation.hpp"
#include "cowel/output_language.hpp"
#include "cowel/theme_to_css.hpp"

#include "cowel/syntax/ast.hpp"

#include "cowel/parameters.hpp"

using namespace std::string_view_literals;

namespace cowel {

Processing_Status
Code_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    Spliceable_To_String_Matcher lang_string_matcher { context.get_transient_memory() };
    Parameter lang_param { u8"lang"sv, Optionality::mandatory, lang_string_matcher };
    Boolean_Matcher nested_boolean;
    Parameter nested_param { u8"nested"sv, Optionality::optional, nested_boolean };
    Boolean_Matcher borders_boolean;
    Parameter borders_param { u8"borders"sv, Optionality::optional, borders_boolean };
    Block_Matcher content_matcher;
    Parameter content_param { u8"content"sv, Optionality::mandatory, content_matcher };
    Parameter* const parameters[] {
        &lang_param,
        &nested_param,
        &borders_param,
        &content_param,
    };

    const auto match_status = match_call(parameters, call, context);
    if (match_status != Processing_Status::ok) {
        return status_is_error(match_status) ? try_generate_error(out, call, context, match_status)
                                             : match_status;
    }

    ensure_paragraph_matches_display(out, m_display);

    const bool should_trim = m_pre_compat_trim == Pre_Trimming::yes;

    // The buffer is used to write the enclosing tag(s) to `out` as HTML.
    // For inline code (non-opaque), `buffer` is flushed before content is written,
    // and content is then written directly to `out` as a mix of text and HTML.
    // For block code with trimming (opaque), everything goes through `buffer`.
    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    const bool is_nested = nested_boolean.get_or_default(false);
    const bool has_enclosing_tags = m_display == Directive_Display::block || !is_nested;
    const bool has_borders
        = m_display == Directive_Display::in_line || borders_boolean.get_or_default(true);

    if (has_enclosing_tags) {
        auto attributes = writer.open_tag_with_attributes(m_tag_name);
        if (!has_borders) {
            COWEL_ASSERT(m_display != Directive_Display::in_line);
            attributes.write_class(u8"borderless"sv);
        }
        attributes.end();
    }

    const std::u8string_view lang_string = lang_string_matcher.get();

    // Non-opaque (false) for inline code: text portions are written as Output_Language::text,
    // which allows downstream policies such as Text_Only_Policy to capture them
    // (e.g. for heading ID synthesis).
    // Opaque (true) when:
    // - block code with pre-compat trimming: the output is collected as an HTML blob first
    //   so that leading/trailing newlines can be stripped before writing; or
    // - nested code: when embedded inside another highlighted context, opaque output
    //   prevents the outer highlighter from re-highlighting the inner text.
    const bool opaque = should_trim || is_nested;
    Syntax_Highlight_Policy highlight_policy //
        { context.get_transient_memory(), opaque, out.get_flags() };
    const auto highlight_status = content_matcher.get().splice_block(highlight_policy, context);

    const Result<void, Syntax_Highlight_Error> result = [&] {
        if (!should_trim) {
            // Flush the open tag to `out` before writing non-opaque mixed content directly.
            buffer.flush();
            return highlight_policy.dump_html_to(out, context, lang_string);
        }
        Vector_Text_Sink vector_sink { Output_Language::html, context.get_transient_memory() };
        const Result<void, Syntax_Highlight_Error> result
            = highlight_policy.dump_html_to(vector_sink, context, lang_string);

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
        diagnose(result.error(), lang_string, call, context);
    }

    if (has_enclosing_tags) {
        writer.close_tag(m_tag_name);
    }
    buffer.flush();

    return highlight_status;
}

[[nodiscard]]
Processing_Status
Highlight_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    Spliceable_To_String_Matcher lang_string { context.get_transient_memory() };
    Parameter lang_param { u8"lang"sv, Optionality::mandatory, lang_string };
    Boolean_Matcher opaque_matcher;
    Parameter opaque_param { u8"opaque"sv, Optionality::optional, opaque_matcher };
    Block_Matcher content_matcher;
    Parameter content_param { u8"content"sv, Optionality::mandatory, content_matcher };
    Parameter* const parameters[] { &lang_param, &opaque_param, &content_param };

    const auto match_status = match_call(parameters, call, context);
    if (match_status != Processing_Status::ok) {
        return status_is_error(match_status) ? try_generate_error(out, call, context, match_status)
                                             : match_status;
    }

    Syntax_Highlight_Policy policy {
        context.get_transient_memory(),
        opaque_matcher.get_or_default(false),
        out.get_flags(),
    };
    const Processing_Status consume_status = content_matcher.get().splice_block(policy, context);
    const Result<void, Syntax_Highlight_Error> result
        = policy.dump_html_to(out, context, lang_string.get());
    if (!result) {
        diagnose(result.error(), lang_string.get(), call, context);
    }

    return consume_status;
}

Processing_Status
Highlight_As_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    Spliceable_To_String_Matcher name_string { context.get_transient_memory() };
    Parameter name_param { u8"name"sv, Optionality::mandatory, name_string };
    Boolean_Matcher opaque_matcher;
    Parameter opaque_param { u8"opaque"sv, Optionality::optional, opaque_matcher };
    Block_Matcher content_matcher;
    Parameter content_param { u8"content"sv, Optionality::mandatory, content_matcher };
    Parameter* const parameters[] { &name_param, &opaque_param, &content_param };

    const auto match_status = match_call(parameters, call, context);
    if (match_status != Processing_Status::ok) {
        return status_is_error(match_status) ? try_generate_error(out, call, context, match_status)
                                             : match_status;
    }

    const std::optional<ulight::Highlight_Type> type
        = highlight_type_by_long_string(name_string.get());
    if (!type) {
        const std::u8string_view message[] {
            u8"The given highlight name \"",
            name_string.get(),
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

    HTML_Content_Policy policy = ensure_html_policy(out);
    HTML_Writer_Buffer buffer { policy, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    writer
        .open_tag_with_attributes(html_tag::h_) //
        .write_attribute(html_attr::data_h, short_name)
        .end();
    buffer.flush();
    Content_Policy& block_out = opaque_matcher.get_or_default(true) ? policy : out;
    const Processing_Status result = content_matcher.get().splice_block(block_out, context);
    if (status_is_break(result)) {
        return result;
    }
    writer.close_tag(html_tag::h_);
    buffer.flush();
    return result;
}

} // namespace cowel
