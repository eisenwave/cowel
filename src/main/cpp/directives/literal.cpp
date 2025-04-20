#include "mmml/builtin_directive_set.hpp"
#include "mmml/directive_processing.hpp"

namespace mmml {

void HTML_Literal_Behavior::generate_html(
    HTML_Writer& out,
    const ast::Directive& d,
    Context& context
) const
{
    std::pmr::vector<char8_t> buffer { context.get_transient_memory() };
    HTML_Writer buffer_writer { buffer };
    to_html_literally(buffer_writer, d.get_content(), context);
    const std::u8string_view buffer_string { buffer.data(), buffer.size() };
    out.write_inner_html(buffer_string);
}

} // namespace mmml
