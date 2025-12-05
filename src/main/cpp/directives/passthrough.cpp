#include <string_view>
#include <vector>

#include "cowel/parameters.hpp"
#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/html_writer.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/policy/content_policy.hpp"
#include "cowel/policy/factory.hpp"
#include "cowel/policy/html.hpp"
#include "cowel/policy/paragraph_split.hpp"
#include "cowel/policy/plaintext.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/invocation.hpp"

using namespace std::string_view_literals;

namespace cowel {

void Deprecated_Behavior::warn(const Invocation& call, Context& context) const
{
    context.try_warning(
        diagnostic::deprecated, call.directive.get_name_span(),
        joined_char_sequence({
            u8"This directive is deprecated; use \\",
            m_replacement,
            u8" instead.",
        })
    );
}

Processing_Status
Error_Behavior::splice(Content_Policy& out, const Invocation& call, Context&) const
{
    // TODO: inline display
    switch (out.get_language()) {
    case Output_Language::none: {
        return Processing_Status::ok;
    }
    case Output_Language::html: {
        Text_Sink_HTML_Writer writer { out };
        writer.open_tag(id);
        writer.write_inner_text(call.directive.get_source());
        writer.close_tag(id);
        return Processing_Status::ok;
    }
    default: {
        return Processing_Status::ok;
    }
    }
}

Processing_Status
Plaintext_Wrapper_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context)
    const
{
    // TODO: warn about unused arguments
    ensure_paragraph_matches_display(out, m_display);

    Plaintext_Content_Policy policy { out };
    return splice_all(policy, call.get_content_span(), call.content_frame, context);
}

Processing_Status
Trim_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    // TODO: warn about unused arguments
    ensure_paragraph_matches_display(out, m_display);

    return splice_all_trimmed(out, call.get_content_span(), call.content_frame, context);
}

Processing_Status
Passthrough_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    Group_Pack_Named_Lazy_Spliceable_Matcher group_matcher;
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    ensure_paragraph_matches_display(out, m_display);

    HTML_Content_Policy html_policy = ensure_html_policy(out);
    auto& policy = m_policy == Policy_Usage::html ? html_policy : out;

    const HTML_Tag_Name name = get_name(call, context);
    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    Text_Buffer_Attribute_Writer attributes = writer.open_tag_with_attributes(name);
    const auto attributes_status = named_arguments_to_attributes(
        attributes, call.get_arguments_span(), call.content_frame, context
    );
    attributes.end();
    if (status_is_break(attributes_status)) {
        writer.close_tag(name);
        buffer.flush();
        return attributes_status;
    }
    buffer.flush();

    const auto content_status
        = splice_all(policy, call.get_content_span(), call.content_frame, context);
    writer.close_tag(name);
    buffer.flush();
    return status_concat(attributes_status, content_status);
}

Processing_Status
HTML_Element_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    Spliceable_To_String_Matcher name_string_matcher { context.get_transient_memory() };
    Group_Member_Matcher name_member_matcher { u8"name"sv, Optionality::mandatory,
                                               name_string_matcher };
    Group_Pack_Named_Lazy_Spliceable_Matcher attributes_group_matcher;
    Group_Member_Matcher attributes_matcher { u8"attr"sv, Optionality::optional,
                                              attributes_group_matcher };
    Group_Member_Matcher* parameters[] { &name_member_matcher, &attributes_matcher };
    Pack_Usual_Matcher args_matcher { parameters };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return status_is_error(match_status) ? try_generate_error(out, call, context, match_status)
                                             : match_status;
    }

    const auto name_string = as_u8string_view(name_string_matcher.get());
    const std::optional<HTML_Tag_Name> name = HTML_Tag_Name::make(name_string);
    if (!name) {
        context.try_error(
            diagnostic::html_element_name_invalid, name_string_matcher.get_location(),
            joined_char_sequence({
                u8"The given tag name \""sv,
                name_string,
                u8"\" is not a valid HTML tag name."sv,
            })
        );
        return try_generate_error(out, call, context);
    }

    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    auto attributes = writer.open_tag_with_attributes(*name);
    auto status = [&] -> Processing_Status {
        if (!attributes_group_matcher.was_matched()) {
            return Processing_Status ::ok;
        }
        return named_arguments_to_attributes(
            attributes, attributes_group_matcher.get().get_members(),
            attributes_group_matcher.get_frame(), context
        );
    }();

    if (m_self_closing == HTML_Element_Self_Closing::self_closing) {
        attributes.end_empty();
        if (!call.get_content_span().empty()) {
            context.try_warning(
                diagnostic::ignored_content, call.content->get_source_span(),
                u8"Content in a self-closing HTML element is ignored."sv
            );
        }
    }
    else {
        attributes.end();
        buffer.flush();
        if (status_is_continue(status)) {
            const auto content_status
                = splice_all(out, call.get_content_span(), call.content_frame, context);
            status = status_concat(status, content_status);
        }
        writer.close_tag(*name);
    }

    buffer.flush();
    return status;
}

// TODO: Passthrough_Behavior and In_Tag_Behavior are virtually identical.
//       It would be better to merge them into one.
Processing_Status
In_Tag_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    Group_Pack_Named_Lazy_Spliceable_Matcher group_matcher;
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    ensure_paragraph_matches_display(out, m_display);

    HTML_Content_Policy html_policy = ensure_html_policy(out);
    auto& policy = m_policy == Policy_Usage::html ? html_policy : out;

    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    auto attributes = writer.open_tag_with_attributes(m_tag_name);
    attributes.write_class(m_class_name);
    const auto attributes_status = named_arguments_to_attributes(
        attributes, call.get_arguments_span(), call.content_frame, context
    );
    attributes.end();
    if (status_is_break(attributes_status)) {
        writer.close_tag(m_tag_name);
        buffer.flush();
        return attributes_status;
    }
    buffer.flush();

    const auto content_status
        = splice_all(policy, call.get_content_span(), call.content_frame, context);

    writer.close_tag(m_tag_name);
    buffer.flush();
    return status_concat(attributes_status, content_status);
}

Processing_Status
Special_Block_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    Group_Pack_Named_Lazy_Spliceable_Matcher group_matcher;
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    try_leave_paragraph(out);

    const bool emit_intro = m_intro == Intro_Policy::yes;
    const auto initial_state = emit_intro ? Paragraphs_State::inside : Paragraphs_State::outside;

    // TODO: I'm pretty sure we don't need an HTML policy if we use a paragraph split policy.
    HTML_Content_Policy html_policy = ensure_html_policy(out);
    Paragraph_Split_Policy policy { html_policy, context.get_transient_memory(), initial_state };

    // Note that it's okay to bypass the paragraph split policy here
    // because all the output HTML would pass through it anyway.
    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    auto attributes = writer.open_tag_with_attributes(m_name);
    const auto attributes_status = named_arguments_to_attributes(
        attributes, call.get_arguments_span(), call.content_frame, context
    );
    attributes.end();
    if (status_is_break(attributes_status)) {
        writer.close_tag(m_name);
        buffer.flush();
        return attributes_status;
    }

    if (emit_intro) {
        writer.open_tag(html_tag::p);
        writer.open_and_close_tag(html_tag::intro_);
        // This space ensures that even if the user writes say,
        // \note{abc}, there is a space between </into> and abc.
        writer.write_inner_html(u8' ');
    }
    buffer.flush();

    const auto content_status
        = splice_all(policy, call.get_content_span(), call.content_frame, context);

    policy.leave_paragraph();
    writer.close_tag(m_name);
    buffer.flush();
    return status_concat(attributes_status, content_status);
}

Processing_Status
URL_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    Group_Pack_Named_Lazy_Spliceable_Matcher group_matcher;
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    try_enter_paragraph(out);

    std::pmr::vector<char8_t> url { context.get_transient_memory() };
    append(url, m_url_prefix);
    const auto text_status
        = splice_to_plaintext(url, call.get_content_span(), call.content_frame, context);
    if (text_status != Processing_Status::ok) {
        return text_status;
    }

    const auto url_string = as_u8string_view(url);

    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    auto attributes = writer.open_tag_with_attributes(html_tag::a);
    const auto attributes_status = named_arguments_to_attributes(
        attributes, call.get_arguments_span(), call.content_frame, context
    );
    attributes.write_href(url_string);
    attributes.write_class(u8"sans"sv);
    attributes.end();

    COWEL_ASSERT(url_string.length() >= m_url_prefix.length());
    writer.write_inner_text(url_string.substr(m_url_prefix.length()));

    writer.close_tag(html_tag::a);
    buffer.flush();
    return attributes_status;
}

Processing_Status
Self_Closing_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    Group_Pack_Named_Lazy_Spliceable_Matcher group_matcher;
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    // TODO: this should use some utility function
    if (!call.get_content_span().empty()) {
        context.try_warning(
            diagnostic::ignored_content, call.content->get_source_span(),
            u8"Content was ignored. Use empty braces,"
            "i.e. {} to resolve this warning."sv
        );
    }

    ensure_paragraph_matches_display(out, m_display);

    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    auto attributes = writer.open_tag_with_attributes(m_tag_name);
    const auto status = named_arguments_to_attributes(
        attributes, call.get_arguments_span(), call.content_frame, context
    );
    attributes.end_empty();
    return status;
}

} // namespace cowel
