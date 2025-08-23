#include <optional>
#include <string_view>

#include "cowel/parameters.hpp"
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
    String_Matcher lang_string_matcher { context.get_transient_memory() };
    Group_Member_Matcher lang_member { u8"lang"sv, Optionality::mandatory, lang_string_matcher };
    Boolean_Matcher nested_boolean;
    Group_Member_Matcher nested_member { u8"nested"sv, Optionality::optional, nested_boolean };
    Boolean_Matcher borders_boolean;
    Group_Member_Matcher borders_member { u8"borders"sv, Optionality::optional, borders_boolean };
    Group_Member_Matcher* parameters[] { &lang_member, &nested_member, &borders_member };
    Pack_Usual_Matcher args_matcher { parameters };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return status_is_error(match_status) ? try_generate_error(out, call, context, match_status)
                                             : match_status;
    }

    // While syntax highlighting generally outputs HTML,
    // when plaintext content is needed (e.g. for ID synthesis),
    // we still want \code to be "transparent" by simply outputting plaintext.
    // Note that for consistent side effects,
    // we still process all the arguments above.
    if (out.get_language() == Output_Language::text) {
        const Processing_Status text_status
            = consume_all(out, call.get_content_span(), call.content_frame, context);
        return text_status;
    }

    ensure_paragraph_matches_display(out, m_display);

    const bool should_trim = m_pre_compat_trim == Pre_Trimming::yes;

    // All content written to out is HTML anyway,
    // so we don't need an extra HTML_Content_Policy here.
    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    const bool has_enclosing_tags
        = m_display == Directive_Display::block || !nested_boolean.get_or_default(false);
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

    Syntax_Highlight_Policy highlight_policy //
        { context.get_transient_memory() };
    const auto highlight_status
        = consume_all(highlight_policy, call.get_content_span(), call.content_frame, context);

    const Result<void, Syntax_Highlight_Error> result = [&] {
        if (!should_trim) {
            return highlight_policy.dump_html_to(buffer, context, lang_string);
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

Processing_Status
Highlight_As_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context)
    const
{
    String_Matcher name_string { context.get_transient_memory() };
    Group_Member_Matcher name_member { u8"name"sv, Optionality::mandatory, name_string };
    Group_Member_Matcher* parameters[] { &name_member };
    Pack_Usual_Matcher args_matcher { parameters };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
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

    // We do the same special casing as for \code (see above for explanation).
    if (out.get_language() == Output_Language::text) {
        return consume_all(out, call.get_content_span(), call.content_frame, context);
    }

    HTML_Content_Policy policy = ensure_html_policy(out);
    HTML_Writer_Buffer buffer { policy, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    writer
        .open_tag_with_attributes(html_tag::h_) //
        .write_attribute(html_attr::data_h, short_name)
        .end();
    buffer.flush();
    const Processing_Status result
        = consume_all(policy, call.get_content_span(), call.content_frame, context);
    if (status_is_break(result)) {
        return result;
    }
    writer.close_tag(html_tag::h_);
    buffer.flush();
    return result;
}

} // namespace cowel
