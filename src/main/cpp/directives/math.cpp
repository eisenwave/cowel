#include <string_view>
#include <vector>

#include "mmml/util/chars.hpp"
#include "mmml/util/strings.hpp"

#include "mmml/builtin_directive_set.hpp"
#include "mmml/directive_arguments.hpp"
#include "mmml/directive_processing.hpp"

namespace mmml {

void Math_Behavior::generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const
{
    constexpr std::u8string_view tag_name = u8"math";
    const std::u8string_view display_string
        = display == Directive_Display::block ? u8"block" : u8"inline";

    Attribute_Writer attributes = out.open_tag_with_attributes(tag_name);
    attributes.write_attribute(u8"display", display_string);
    arguments_to_attributes(attributes, d, context);
    attributes.end();

    out.close_tag(tag_name);
}

} // namespace mmml
