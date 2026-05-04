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

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/collecting_logger.hpp"
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
        joined_char_sequence(
            {
                u8"This directive is deprecated; use \\",
                m_replacement,
                u8" instead.",
            }
        )
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
Passthrough_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    Pack_Named_Str_Matcher attr_matcher;
    Parameter attr_param { u8"attr"sv, Optionality::optional, attr_matcher };
    Block_Matcher content_matcher;
    Parameter content_param { u8"content"sv, Optionality::optional, content_matcher };
    Parameter* const parameters[] { &attr_param, &content_param };

    const auto match_status = match_call(parameters, call, context, make_fail_callback());
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
    const auto attributes_status
        = named_arguments_to_attributes_or_error(attributes, attr_matcher, context);
    attributes.end();
    buffer.flush();

    const auto content_status = content_matcher.has_value()
        ? content_matcher.get().splice_block(policy, context)
        : Processing_Status::ok;
    writer.close_tag(name);
    buffer.flush();
    return status_concat(attributes_status, content_status);
}

Processing_Status
HTML_Element_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    Spliceable_To_String_Matcher name_matcher { context.get_transient_memory() };
    Parameter name_param { u8"name"sv, Optionality::mandatory, name_matcher };
    Group_Pack_Named_Str_Matcher attr_matcher;
    Parameter attr_param { u8"attr"sv, Optionality::optional, attr_matcher };
    Block_Matcher content_matcher;
    Parameter content_param { u8"content"sv, Optionality::optional, content_matcher };

    Parameter* const parameters[] { &name_param, &attr_param, &content_param };
    const std::size_t parameter_count
        = m_self_closing == HTML_Element_Self_Closing::self_closing ? 2 : 3;
    const auto actual_parameters = std::span { parameters }.subspan(0, parameter_count);

    const auto match_status = match_call(actual_parameters, call, context);
    if (match_status != Processing_Status::ok) {
        return status_is_error(match_status) ? try_generate_error(out, call, context, match_status)
                                             : match_status;
    }

    const auto name_string = as_u8string_view(name_matcher.get());
    const std::optional<HTML_Tag_Name> name = HTML_Tag_Name::make(name_string);
    if (!name) {
        context.try_error(
            diagnostic::html_element_name_invalid, name_matcher.get_location(),
            joined_char_sequence(
                {
                    u8"The given tag name \""sv,
                    name_string,
                    u8"\" is not a valid HTML tag name."sv,
                }
            )
        );
        return try_generate_error(out, call, context);
    }

    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    auto attributes = writer.open_tag_with_attributes(*name);
    auto status = named_arguments_to_attributes_or_error(attributes, attr_matcher, context);
    if (m_self_closing == HTML_Element_Self_Closing::self_closing) {
        COWEL_ASSERT(!content_matcher.was_matched());
        attributes.end_empty();
    }
    else {
        attributes.end();
        buffer.flush();
        if (status_is_continue(status) && content_matcher.was_matched()) {
            const auto content_status = content_matcher.get().splice_block(out, context);
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
    Pack_Named_Str_Matcher attr_matcher;
    Parameter attr_param { u8"attr"sv, Optionality::optional, attr_matcher };
    Block_Matcher content_matcher;
    Parameter content_param { u8"content"sv, Optionality::mandatory, content_matcher };
    Parameter* const parameters[] { &attr_param, &content_param };

    const auto match_status = match_call(parameters, call, context);
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
    const Processing_Status attributes_status
        = named_arguments_to_attributes_or_error(attributes, attr_matcher, context);
    attributes.end();
    buffer.flush();

    const auto content_status = content_matcher.get().splice_block(policy, context);

    writer.close_tag(m_tag_name);
    buffer.flush();
    return status_concat(attributes_status, content_status);
}

Processing_Status
Special_Block_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    Pack_Named_Str_Matcher attr_matcher;
    Parameter attr_param { u8"attr"sv, Optionality::optional, attr_matcher };
    Block_Matcher content_matcher;
    Parameter content_param { u8"content"sv, Optionality::mandatory, content_matcher };
    Parameter* const parameters[] { &attr_param, &content_param };

    const auto match_status = match_call(parameters, call, context);
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
    const Processing_Status attributes_status
        = named_arguments_to_attributes_or_error(attributes, attr_matcher, context);
    attributes.end();

    if (emit_intro) {
        writer.open_tag(html_tag::p);
        writer.open_and_close_tag(html_tag::intro_);
        // This space ensures that even if the user writes say,
        // \note{abc}, there is a space between </into> and abc.
        writer.write_inner_html(u8' ');
    }
    buffer.flush();

    const auto content_status = content_matcher.get().splice_block(policy, context);

    policy.leave_paragraph();
    writer.close_tag(m_name);
    buffer.flush();
    return status_concat(attributes_status, content_status);
}

Processing_Status
URL_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    Pack_Named_Str_Matcher attr_matcher;
    Parameter attr_param { u8"attr"sv, Optionality::optional, attr_matcher };
    Block_Matcher content_matcher;
    Parameter content_param { u8"content"sv, Optionality::mandatory, content_matcher };
    Parameter* const parameters[] { &attr_param, &content_param };

    const auto match_status = match_call(parameters, call, context);
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    try_enter_paragraph(out);

    std::pmr::vector<char8_t> url { context.get_transient_memory() };
    append(url, m_url_prefix);
    const auto text_status = splice_value_to_plaintext(
        url, content_matcher.get(), content_matcher.get_location(), context
    );
    if (text_status != Processing_Status::ok) {
        return text_status;
    }

    const auto url_string = as_u8string_view(url);

    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    auto attributes = writer.open_tag_with_attributes(html_tag::a);
    const Processing_Status attributes_status
        = named_arguments_to_attributes_or_error(attributes, attr_matcher, context);
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
    Pack_Named_Str_Matcher attr_matcher;
    Parameter attr_param { u8"attr"sv, Optionality::optional, attr_matcher };
    Block_Matcher content_matcher;
    Parameter content_param { u8"content"sv, Optionality::optional, content_matcher };
    Parameter* const parameters[] { &attr_param, &content_param };

    const auto match_status = match_call(parameters, call, context);
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    ensure_paragraph_matches_display(out, m_display);

    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    auto attributes = writer.open_tag_with_attributes(m_tag_name);
    const Processing_Status status
        = named_arguments_to_attributes_or_error(attributes, attr_matcher, context);
    attributes.end_empty();
    return status;
}

[[nodiscard]]
Processing_Status Internal_Expect_Diagnostic_Behavior::splice(
    Content_Policy& out,
    const Invocation& call,
    Context& context
) const
{
    String_Matcher id_matcher { context.get_transient_memory() };
    Parameter id_param { u8"id"sv, Optionality::mandatory, id_matcher };
    Block_Matcher content_matcher;
    Parameter content_param { u8"content"sv, Optionality::mandatory, content_matcher };
    Parameter* const parameters[] { &id_param, &content_param };

    if (const auto match_status = match_call_fatal_error(parameters, call, context);
        match_status != Processing_Status::ok) {
        return match_status;
    }
    const std::u8string_view expected_id = id_matcher.get();

    Logger* const old_logger = &context.get_logger();
    COWEL_ASSERT(old_logger);
    COWEL_ASSERT(old_logger->can_log(Severity::error));

    Processing_Status result = Processing_Status::ok;

    Expecting_Logger expecting_logger {
        old_logger->get_min_severity(),
        m_expected_severity,
        expected_id,
        context.get_transient_memory(),
    };
    context.set_logger(expecting_logger);
    const Processing_Status status = content_matcher.get().splice_block(out, context);

    if (status != m_expected_status) {
        (*old_logger)(Diagnostic {
            .severity = Severity::error,
            .id = u8"test.diagnostic"sv,
            .location = call.directive.get_source_span(),
            .message = joined_char_sequence(
                {
                    u8"Expected the block to evaluate to status \""sv,
                    status_name(m_expected_status),
                    u8"\", but got \""sv,
                    status_name(status),
                    u8"\"."sv,
                }
            ),
        });
        result = Processing_Status::error;
    }
    for (const Collected_Diagnostic& violation : expecting_logger.get_violations()) {
        (*old_logger)(Diagnostic {
            std::max(Severity::error, violation.severity),
            as_u8string_view(violation.id),
            violation.location,
            as_u8string_view(violation.message),
        });
        result = Processing_Status::error;
    }
    if (!expecting_logger.was_expected_logged()) {
        std::pmr::u8string message { context.get_transient_memory() };
        message += u8"Expected the block to produce the diagnostic \""sv;
        message += expected_id;
        message += u8"\", but it was not logged (with the expected severity)."sv;
        if (!expecting_logger.get_extra_diagnostics().empty()) {
            message += u8" However, the following additional diagnostics were logged: "sv;
            for (bool first = true;
                 const Collected_Diagnostic& extra : expecting_logger.get_extra_diagnostics()) {
                if (!first) {
                    message += u8", "sv;
                }
                message += extra.id;
                first = false;
            }
        }

        (*old_logger)(Diagnostic {
            .severity = Severity::error,
            .id = u8"test.diagnostic"sv,
            .location = call.directive.get_source_span(),
            .message = as_u8string_view(message),
        });
        result = Processing_Status::error;
    }

    context.set_logger(*old_logger);
    return result;
}

Result<Short_String_Value, Processing_Status>
Internal_Typeof_Behavior::do_evaluate(const Invocation& call, Context& context) const
{
    Value_Of_Type_Matcher expr_matcher { Type::any };
    Parameter expr_param { u8"expr"sv, Optionality::mandatory, expr_matcher };
    Parameter* const parameters[] { &expr_param };

    if (const auto match_status = match_call_fatal_error(parameters, call, context);
        match_status != Processing_Status::ok) {
        return match_status;
    }

    const std::u8string_view name
        = type_kind_display_name(expr_matcher.get().get_type().get_kind());
    return Short_String_Value { name };
}

} // namespace cowel
