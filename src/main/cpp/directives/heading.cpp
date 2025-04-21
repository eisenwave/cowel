#include "mmml/builtin_directive_set.hpp"
#include "mmml/directive_processing.hpp"

namespace mmml {

void Heading_Behavior::generate_html(HTML_Writer& out, const ast::Directive& d, Context& context)
    const
{
    const auto level_char = char8_t(int(u8'0') + m_level);
    const char8_t tag_name_data[2] { u8'h', level_char };
    const std::u8string_view tag_name { tag_name_data, sizeof(tag_name_data) };

    // TODO: synthesize id from heading plaintext content if none provided

    Attribute_Writer attributes = out.open_tag_with_attributes(tag_name);
    arguments_to_attributes(attributes, d, context);
    attributes.end();
    to_html(out, d.get_content(), context);
    out.close_tag(tag_name);
}

} // namespace mmml
