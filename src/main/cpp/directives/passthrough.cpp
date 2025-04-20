#include "mmml/builtin_directive_set.hpp"
#include "mmml/directive_processing.hpp"

namespace mmml {

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

[[nodiscard]]
std::u8string_view
Directive_Name_Passthrough_Behavior::get_name(const ast::Directive& d, Context& context) const
{
    const std::u8string_view raw_name = d.get_name(context.get_source());
    const std::u8string_view name
        = raw_name.starts_with(builtin_directive_prefix) ? raw_name.substr(1) : raw_name;
    return name.substr(m_name_prefix.size());
}

} // namespace mmml
