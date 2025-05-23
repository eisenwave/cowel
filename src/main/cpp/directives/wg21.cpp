#include <string_view>
#include <vector>

#include "cowel/util/strings.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/directive_processing.hpp"

namespace cowel {

void WG21_Block_Behavior::generate_html(HTML_Writer& out, const ast::Directive& d, Context& context)
    const
{
    constexpr std::u8string_view tag = u8"wg21-block";

    Attribute_Writer attributes = out.open_tag_with_attributes(tag);
    arguments_to_attributes(attributes, d, context);
    attributes.end();

    out.write_inner_html(u8"[<i>");
    out.write_inner_text(m_prefix);
    out.write_inner_html(u8"</i>: ");

    to_html(out, d.get_content(), context);

    out.write_inner_html(u8" \N{EM DASH} <i>");
    out.write_inner_text(m_suffix);
    out.write_inner_html(u8"</i>]");
    out.close_tag(tag);
}

void WG21_Head_Behavior::generate_html(HTML_Writer& out, const ast::Directive& d, Context& context)
    const
{
    static constexpr std::u8string_view parameters[] { u8"title" };
    Argument_Matcher args { parameters, context.get_transient_memory() };
    args.match(d.get_arguments());

    out.open_tag_with_attributes(u8"div") //
        .write_class(u8"wg21-head")
        .end();

    const int title_index = args.get_argument_index(u8"title");
    if (title_index < 0) {
        context.try_warning(
            diagnostic::wg21_head::no_title, d.get_source_span(),
            u8"A wg21-head directive requires a title argument"
        );
    }

    std::pmr::vector<char8_t> title_text { context.get_transient_memory() };
    HTML_Writer title_writer { title_text };
    to_html(title_writer, d.get_arguments()[std::size_t(title_index)].get_content(), context);
    const auto title_string = as_u8string_view(title_text);

    {
        const auto scope = context.get_sections().go_to_scoped(section_name::document_head);
        HTML_Writer head_writer = context.get_sections().current_html();
        head_writer.open_tag(u8"title");
        head_writer.write_inner_html(title_string);
        head_writer.close_tag(u8"title");
    }

    out.open_tag(u8"h1");
    out.write_inner_html(title_string);
    out.close_tag(u8"h1");
    out.write_inner_html(u8'\n');

    to_html(out, d.get_content(), context);

    out.close_tag(u8"div");
}

} // namespace cowel
