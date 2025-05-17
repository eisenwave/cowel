#include <vector>

#include "cowel/util/html_writer.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/context.hpp"
#include "cowel/directive_processing.hpp"

namespace cowel {

void HTML_Literal_Behavior::generate_html(
    HTML_Writer& out,
    const ast::Directive& d,
    Context& context
) const
{
    std::pmr::vector<char8_t> buffer { context.get_transient_memory() };
    to_plaintext(buffer, d.get_content(), context);
    out.write_inner_html(as_u8string_view(buffer));
}

void HTML_Raw_Text_Behavior::generate_html(
    HTML_Writer& out,
    const ast::Directive& d,
    Context& context
) const
{
    Attribute_Writer attributes = out.open_tag_with_attributes(m_tag_name);
    arguments_to_attributes(attributes, d, context);
    attributes.end();

    std::pmr::vector<char8_t> buffer { context.get_transient_memory() };
    to_plaintext(buffer, d.get_content(), context);
    // FIXME: this could produce malformed HTML if the generated CSS/JS contains a closing tag
    out.write_inner_html(as_u8string_view(buffer));

    out.close_tag(m_tag_name);
}

} // namespace cowel
