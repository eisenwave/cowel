#include <string_view>
#include <vector>

#include "cowel/util/strings.hpp"

#include "cowel/policy/html.hpp"
#include "cowel/policy/paragraph_split.hpp"
#include "cowel/policy/plaintext.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/directive_processing.hpp"

using namespace std::string_view_literals;

namespace cowel {

Processing_Status
HTML_Wrapper_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context)
    const
{
    // TODO: warn about unused arguments
    ensure_paragraph_matches_display(out, m_display);

    Paragraph_Split_Policy split_policy { out, context.get_transient_memory() };
    auto& policy = m_is_paragraphed ? split_policy : out;

    const Processing_Status result = consume_all(policy, d.get_content(), context);
    if (m_is_paragraphed) {
        split_policy.leave_paragraph();
    }

    return result;
}

Processing_Status Plaintext_Wrapper_Behavior::operator()(
    Content_Policy& out,
    const ast::Directive& d,
    Context& context
) const
{
    // TODO: warn about unused arguments
    ensure_paragraph_matches_display(out, m_display);

    Plaintext_Content_Policy policy { out };
    return consume_all(policy, d.get_content(), context);
}

Processing_Status
Trim_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context) const
{
    // TODO: warn about unused arguments
    ensure_paragraph_matches_display(out, m_display);

    return consume_all_trimmed(out, d.get_content(), context);
}

Processing_Status
Passthrough_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context)
    const
{
    warn_ignored_argument_subset(d.get_arguments(), context, Argument_Subset::positional);

    ensure_paragraph_matches_display(out, m_display);

    HTML_Content_Policy html_policy { out };
    auto& policy = m_policy == Policy_Usage::html ? html_policy : out;

    const std::u8string_view name = get_name(d, context);
    HTML_Writer writer { policy };
    Attribute_Writer attributes = writer.open_tag_with_attributes(name);
    const auto attributes_status = named_arguments_to_attributes(attributes, d, context);
    attributes.end();
    if (status_is_break(attributes_status)) {
        writer.close_tag(name);
        return attributes_status;
    }

    const auto content_status = consume_all(policy, d.get_content(), context);
    writer.close_tag(name);
    return status_concat(attributes_status, content_status);
}

Processing_Status
HTML_Element_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context)
    const
{
    const ast::Argument* first_positional = get_first_positional_warn_rest(d, context);
    if (!first_positional) {
        context.try_error(
            diagnostic::html_element_name_missing, d.get_name_span(),
            u8"A tag name must be provided (in the form of a positional argument)."sv
        );
        return try_generate_error(out, d, context);
    }

    std::pmr::vector<char8_t> name_text { context.get_transient_memory() };
    const Processing_Status name_status
        = to_plaintext(name_text, first_positional->get_content(), context);
    if (name_status != Processing_Status::ok) {
        return name_status;
    }
    const auto name_string = as_u8string_view(name_text);
    if (!is_html_tag_name(name_string)) {
        context.try_error(
            diagnostic::html_element_name_invalid, first_positional->get_source_span(),
            joined_char_sequence({
                u8"The given tag name \""sv,
                name_string,
                u8"\" is not a valid HTML tag name."sv,
            })
        );
        return try_generate_error(out, d, context);
    }

    HTML_Writer writer { out };
    Attribute_Writer attributes = writer.open_tag_with_attributes(name_string);
    auto status = named_arguments_to_attributes(attributes, d, context);

    if (m_self_closing == HTML_Element_Self_Closing::self_closing) {
        attributes.end_empty();
        if (!d.get_content().empty()) {
            context.try_warning(
                diagnostic::ignored_content, d.get_source_span(),
                u8"Content in a self-closing HTML element is ignored."sv
            );
        }
    }
    else {
        attributes.end();
        if (status_is_continue(status)) {
            const auto content_status = consume_all(out, d.get_content(), context);
            status = status_concat(content_status, status);
        }
        writer.close_tag(name_string);
    }

    return status;
}

// TODO: Passthrough_Behavior and In_Tag_Behavior are virtually identical.
//       It would be better to merge them into one.
Processing_Status
In_Tag_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context) const
{
    warn_ignored_argument_subset(d.get_arguments(), context, Argument_Subset::positional);

    ensure_paragraph_matches_display(out, m_display);

    HTML_Content_Policy html_policy { out };
    auto& policy = m_policy == Policy_Usage::html ? html_policy : out;

    HTML_Writer writer { policy };
    Attribute_Writer attributes = writer.open_tag_with_attributes(m_tag_name);
    attributes.write_class(m_class_name);
    const auto attributes_status = named_arguments_to_attributes(attributes, d, context);
    attributes.end();
    if (status_is_break(attributes_status)) {
        writer.close_tag(m_tag_name);
        return attributes_status;
    }

    const auto content_status = consume_all(policy, d.get_content(), context);
    writer.close_tag(m_tag_name);
    return status_concat(attributes_status, content_status);
}

[[nodiscard]]
std::u8string_view
Directive_Name_Passthrough_Behavior::get_name(const ast::Directive& d, Context& context) const
{
    context.try_warning(
        diagnostic::deprecated, d.get_source_span(),
        u8"\\html-NAME directives are deprecated. "
        u8"Use \\cowel_html_element[NAME] instead."sv
    );

    const std::u8string_view raw_name = d.get_name();
    const std::u8string_view name
        = raw_name.starts_with(builtin_directive_prefix) ? raw_name.substr(1) : raw_name;
    return name.substr(m_name_prefix.size());
}

Processing_Status
Special_Block_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context)
    const
{
    warn_ignored_argument_subset(d.get_arguments(), context, Argument_Subset::positional);

    try_leave_paragraph(out);

    const bool emit_intro = m_intro == Intro_Policy::yes;
    const auto initial_state = emit_intro ? Paragraphs_State::inside : Paragraphs_State::outside;

    HTML_Content_Policy html_policy { out };
    Paragraph_Split_Policy policy { html_policy, context.get_transient_memory(), initial_state };
    HTML_Writer writer { policy };
    Attribute_Writer attributes = writer.open_tag_with_attributes(m_name);
    const auto attributes_status = named_arguments_to_attributes(attributes, d, context);
    attributes.end();
    if (status_is_break(attributes_status)) {
        writer.close_tag(m_name);
        return attributes_status;
    }

    if (emit_intro) {
        writer.open_tag(u8"p");
        writer.open_and_close_tag(u8"intro-");
        // This space ensures that even if the user writes say,
        // \note{abc}, there is a space between </into> and abc.
        writer.write_inner_html(u8' ');
    }
    // TODO: add paragraph splitting content policy here
    const auto content_status = consume_all(policy, d.get_content(), context);
    policy.leave_paragraph();
    writer.close_tag(m_name);
    return status_concat(attributes_status, content_status);
}

Processing_Status
URL_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context) const
{
    // TODO: warn about unused arguments

    try_enter_paragraph(out);

    std::pmr::vector<char8_t> url { context.get_transient_memory() };
    append(url, m_url_prefix);
    const auto text_status = to_plaintext(url, d.get_content(), context);
    if (text_status != Processing_Status::ok) {
        return text_status;
    }

    const auto url_string = as_u8string_view(url);

    HTML_Writer writer { out };
    Attribute_Writer attributes = writer.open_tag_with_attributes(u8"a");
    const auto attributes_status = named_arguments_to_attributes(attributes, d, context);
    attributes.write_href(url_string);
    attributes.write_class(u8"sans"sv);
    attributes.end();
    warn_ignored_argument_subset(d.get_arguments(), context, Argument_Subset::positional);

    COWEL_ASSERT(url_string.length() >= m_url_prefix.length());
    writer.write_inner_text(url_string.substr(m_url_prefix.length()));

    writer.close_tag(u8"a");
    return attributes_status;
}

Processing_Status
Self_Closing_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context)
    const
{
    warn_ignored_argument_subset(d.get_arguments(), context, Argument_Subset::positional);

    // TODO: this should use some utility function
    if (!d.get_content().empty()) {
        const auto location = ast::get_source_span(d.get_content().front());
        context.try_warning(
            diagnostic::ignored_content, location,
            u8"Content was ignored. Use empty braces,"
            "i.e. {} to resolve this warning."sv
        );
    }

    ensure_paragraph_matches_display(out, m_display);

    HTML_Writer writer { out };
    Attribute_Writer attributes = writer.open_tag_with_attributes(m_tag_name);
    const auto status = named_arguments_to_attributes(attributes, d, context);
    attributes.end_empty();
    return status;
}

Processing_Status
List_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context) const
{
    warn_ignored_argument_subset(d.get_arguments(), context, Argument_Subset::positional);

    try_leave_paragraph(out);

    HTML_Content_Policy policy { out };
    HTML_Writer writer { policy };
    Attribute_Writer attributes = writer.open_tag_with_attributes(m_tag_name);
    const auto attributes_status = named_arguments_to_attributes(attributes, d, context);
    attributes.end();
    if (status_is_break(attributes_status)) {
        return attributes_status;
    }

    const auto content_status = process_greedy(d.get_content(), [&](const ast::Content& c) {
        if (const auto* const directive = std::get_if<ast::Directive>(&c)) {
            const std::u8string_view name = directive->get_name();
            return [&] {
                if (name == u8"item" || name == u8"-item") {
                    context.try_warning(
                        diagnostic::deprecated, directive->get_name_span(),
                        u8"Use of \\item is deprecated. Use \\li in lists instead."sv
                    );
                    return m_item_behavior(policy, *directive, context);
                }
                return policy.consume(*directive, context);
            }();
        }
        return policy.consume_content(c, context);
    });

    writer.close_tag(m_tag_name);
    return status_concat(attributes_status, content_status);
}

} // namespace cowel
