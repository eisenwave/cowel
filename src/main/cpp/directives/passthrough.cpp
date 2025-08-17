#include <string_view>
#include <vector>

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
Error_Behavior::operator()(Content_Policy& out, const Invocation& call, Context&) const
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

Processing_Status Plaintext_Wrapper_Behavior::operator()(
    Content_Policy& out,
    const Invocation& call,
    Context& context
) const
{
    // TODO: warn about unused arguments
    ensure_paragraph_matches_display(out, m_display);

    Plaintext_Content_Policy policy { out };
    return consume_all(policy, call.get_content_span(), call.content_frame, context);
}

Processing_Status
Trim_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context) const
{
    // TODO: warn about unused arguments
    ensure_paragraph_matches_display(out, m_display);

    return consume_all_trimmed(out, call.get_content_span(), call.content_frame, context);
}

Processing_Status
Passthrough_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context)
    const
{
    warn_ignored_argument_subset(call.arguments, context, Argument_Subset::positional);

    ensure_paragraph_matches_display(out, m_display);

    HTML_Content_Policy html_policy = ensure_html_policy(out);
    auto& policy = m_policy == Policy_Usage::html ? html_policy : out;

    const HTML_Tag_Name name = get_name(call, context);
    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    Text_Buffer_Attribute_Writer attributes = writer.open_tag_with_attributes(name);
    const auto attributes_status
        = named_arguments_to_attributes(attributes, call.arguments, context);
    attributes.end();
    if (status_is_break(attributes_status)) {
        writer.close_tag(name);
        buffer.flush();
        return attributes_status;
    }
    buffer.flush();

    const auto content_status
        = consume_all(policy, call.get_content_span(), call.content_frame, context);
    writer.close_tag(name);
    buffer.flush();
    return status_concat(attributes_status, content_status);
}

Processing_Status
HTML_Element_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context)
    const
{
    const std::optional<Argument_Ref> first_positional
        = get_first_positional_warn_rest(call.arguments, context);
    if (!first_positional) {
        context.try_error(
            diagnostic::html_element_name_missing, call.directive.get_name_span(),
            u8"A tag name must be provided (in the form of a positional argument)."sv
        );
        return try_generate_error(out, call, context);
    }
    const auto* const first_positional_content
        = as_content_or_error(first_positional->ast_node.get_value(), context);
    if (!first_positional_content) {
        return try_generate_error(out, call, context);
    }

    std::pmr::vector<char8_t> name_text { context.get_transient_memory() };
    const Processing_Status name_status = to_plaintext(
        name_text, first_positional_content->get_elements(), first_positional->frame_index, context
    );
    if (name_status != Processing_Status::ok) {
        return name_status;
    }
    const auto name_string = as_u8string_view(name_text);
    const std::optional<HTML_Tag_Name> name = HTML_Tag_Name::make(name_string);
    if (!name) {
        context.try_error(
            diagnostic::html_element_name_invalid, first_positional->ast_node.get_source_span(),
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
    auto status = named_arguments_to_attributes(attributes, call.arguments, context);

    if (m_self_closing == HTML_Element_Self_Closing::self_closing) {
        attributes.end_empty();
        if (call.content && !call.content->empty()) {
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
                = consume_all(out, call.get_content_span(), call.content_frame, context);
            status = status_concat(content_status, status);
        }
        writer.close_tag(*name);
    }

    buffer.flush();
    return status;
}

// TODO: Passthrough_Behavior and In_Tag_Behavior are virtually identical.
//       It would be better to merge them into one.
Processing_Status
In_Tag_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context) const
{
    warn_ignored_argument_subset(call.arguments, context, Argument_Subset::positional);

    ensure_paragraph_matches_display(out, m_display);

    HTML_Content_Policy html_policy = ensure_html_policy(out);
    auto& policy = m_policy == Policy_Usage::html ? html_policy : out;

    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    auto attributes = writer.open_tag_with_attributes(m_tag_name);
    attributes.write_class(m_class_name);
    const auto attributes_status
        = named_arguments_to_attributes(attributes, call.arguments, context);
    attributes.end();
    if (status_is_break(attributes_status)) {
        writer.close_tag(m_tag_name);
        buffer.flush();
        return attributes_status;
    }
    buffer.flush();

    const auto content_status
        = consume_all(policy, call.get_content_span(), call.content_frame, context);

    writer.close_tag(m_tag_name);
    buffer.flush();
    return status_concat(attributes_status, content_status);
}

Processing_Status
Special_Block_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context)
    const
{
    warn_ignored_argument_subset(call.arguments, context, Argument_Subset::positional);

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
    const auto attributes_status
        = named_arguments_to_attributes(attributes, call.arguments, context);
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
        = consume_all(policy, call.get_content_span(), call.content_frame, context);

    policy.leave_paragraph();
    writer.close_tag(m_name);
    buffer.flush();
    return status_concat(attributes_status, content_status);
}

Processing_Status
URL_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context) const
{
    warn_ignored_argument_subset(call.arguments, context, Argument_Subset::positional);

    try_enter_paragraph(out);

    std::pmr::vector<char8_t> url { context.get_transient_memory() };
    append(url, m_url_prefix);
    const auto text_status
        = to_plaintext(url, call.get_content_span(), call.content_frame, context);
    if (text_status != Processing_Status::ok) {
        return text_status;
    }

    const auto url_string = as_u8string_view(url);

    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    auto attributes = writer.open_tag_with_attributes(html_tag::a);
    const auto attributes_status
        = named_arguments_to_attributes(attributes, call.arguments, context);
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
Self_Closing_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context)
    const
{
    warn_ignored_argument_subset(call.arguments, context, Argument_Subset::positional);

    // TODO: this should use some utility function
    if (call.content && !call.content->empty()) {
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
    const auto status = named_arguments_to_attributes(attributes, call.arguments, context);
    attributes.end_empty();
    return status;
}

} // namespace cowel
