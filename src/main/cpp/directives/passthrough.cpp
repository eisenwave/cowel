#include <string_view>
#include <vector>

#include "cowel/util/strings.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/directive_processing.hpp"

namespace cowel {

void Wrap_Behavior::generate_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    Context& context
) const
{
    to_plaintext(out, d.get_content(), context);
}

void Wrap_Behavior::generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const
{
    to_html(out, d.get_content(), context, m_to_html_mode);
}

void Passthrough_Behavior::generate_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    Context& context
) const
{
    switch (category) {
    case Directive_Category::formatting:
    case Directive_Category::pure_plaintext: to_plaintext(out, d.get_content(), context); break;
    case Directive_Category::pure_html: break;
    case Directive_Category::meta:
    case Directive_Category::macro:
        COWEL_ASSERT_UNREACHABLE(u8"Passthrough_Behavior should not be meta or macro.");
    }
}

void Passthrough_Behavior::generate_html(
    HTML_Writer& out,
    const ast::Directive& d,
    Context& context
) const
{
    const std::u8string_view name = get_name(d, context);
    if (d.get_arguments().empty()) {
        out.open_tag(name);
    }
    else {
        Attribute_Writer attributes = out.open_tag_with_attributes(name);
        arguments_to_attributes(attributes, d, context);
        attributes.end();
    }
    to_html(out, d.get_content(), context);
    out.close_tag(name);
}

void In_Tag_Behavior::generate_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    Context& context
) const
{
    switch (category) {
    case Directive_Category::formatting:
    case Directive_Category::pure_plaintext: to_plaintext(out, d, context); break;
    case Directive_Category::pure_html: break;
    case Directive_Category::meta:
    case Directive_Category::macro:
        COWEL_ASSERT_UNREACHABLE(u8"In_Tag_Behavior should not be meta or macro.");
    }
}

void In_Tag_Behavior::generate_html(HTML_Writer& out, const ast::Directive& d, Context& context)
    const
{
    Attribute_Writer attributes = out.open_tag_with_attributes(m_tag_name);
    arguments_to_attributes(attributes, d, context);
    attributes.write_class(m_class_name);
    attributes.end();

    to_html(out, d.get_content(), context);
    out.close_tag(m_tag_name);
}

[[nodiscard]]
std::u8string_view
Directive_Name_Passthrough_Behavior::get_name(const ast::Directive& d, Context& context) const
{
    const std::u8string_view raw_name = d.get_name(context.get_source());
    const std::u8string_view name
        = raw_name.starts_with(builtin_directive_prefix) ? raw_name.substr(1) : raw_name;
    return name.substr(m_name_prefix.size());
}

void Special_Block_Behavior::generate_html(
    HTML_Writer& out,
    const ast::Directive& d,
    Context& context
) const
{
    if (d.get_arguments().empty()) {
        out.open_tag(m_name);
    }
    else {
        Attribute_Writer attributes = out.open_tag_with_attributes(m_name);
        arguments_to_attributes(attributes, d, context);
    }
    auto initial_state = Paragraphs_State::outside;
    if (m_emit_intro) {
        initial_state = Paragraphs_State::inside;
        out.open_tag(u8"p");
        out.open_and_close_tag(u8"intro-");
        // This space ensures that even if the user writes say,
        // \note{abc}, there is a space between </into> and abc.
        out.write_inner_html(u8' ');
    }
    to_html(out, d.get_content(), context, To_HTML_Mode::paragraphs, initial_state);
    out.close_tag(m_name);
}

void URL_Behavior::generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const
{
    std::pmr::vector<char8_t> url { context.get_transient_memory() };
    append(url, m_url_prefix);
    to_plaintext(url, d.get_content(), context);
    const auto url_string = as_u8string_view(url);

    Attribute_Writer attributes = out.open_tag_with_attributes(u8"a");
    arguments_to_attributes(attributes, d, context);
    attributes.write_href(url_string);
    attributes.write_class(u8"sans");
    attributes.end();

    COWEL_ASSERT(url_string.length() >= m_url_prefix.length());
    out.write_inner_text(url_string.substr(m_url_prefix.length()));

    out.close_tag(u8"a");
}

void Self_Closing_Behavior::generate_html(
    HTML_Writer& out,
    const ast::Directive& d,
    Context& context
) const
{
    if (!d.get_content().empty()) {
        const auto location = ast::get_source_span(d.get_content().front());
        context.try_warning(
            diagnostic::ignored_content, location,
            u8"Content was ignored. Use empty braces,"
            "i.e. {} to resolve this warning."
        );
    }

    Attribute_Writer attributes = out.open_tag_with_attributes(m_tag_name);
    arguments_to_attributes(attributes, d, context);
    attributes.end_empty();
}

void List_Behavior::generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const
{
    static Fixed_Name_Passthrough_Behavior item_behavior { u8"li", Directive_Category::pure_html,
                                                           Directive_Display::block };

    Attribute_Writer attributes = out.open_tag_with_attributes(m_tag_name);
    arguments_to_attributes(attributes, d, context);
    attributes.end();
    for (const ast::Content& c : d.get_content()) {
        if (const auto* const directive = std::get_if<ast::Directive>(&c)) {
            const std::u8string_view name = directive->get_name(context.get_source());
            if (name == u8"item" || name == u8"-item") {
                item_behavior.generate_html(out, *directive, context);
            }
            else {
                to_html(out, *directive, context);
            }
            continue;
        }
        to_html(out, c, context);
    }
    out.close_tag(m_tag_name);
}

} // namespace cowel
