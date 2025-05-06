#include "mmml/builtin_directive_set.hpp"
#include "mmml/directive_processing.hpp"

namespace mmml {

void Block_Behavior::generate_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    Context& context
) const
{
    to_plaintext(out, d, context);
}

void Block_Behavior::generate_html(HTML_Writer& out, const ast::Directive& d, Context& context)
    const
{
    to_html(out, d, context);
}

void Passthrough_Behavior::generate_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    Context& context
) const
{
    to_plaintext(out, d.get_content(), context);
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
    }
    to_html(out, d.get_content(), context);
    out.close_tag(name);
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
    out.open_tag(u8"p");
    if (m_emit_intro) {
        out.open_and_close_tag(u8"intro-");
        // This space ensures that even if the user writes say,
        // \note{abc}, there is a space between </into> and abc.
        out.write_inner_html(u8' ');
    }
    to_html(out, d.get_content(), context, To_HTML_Mode::paragraphs, Paragraphs_State::inside);
    out.close_tag(m_name);
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
            m_content_ignored_diagnostic, location,
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

} // namespace mmml
