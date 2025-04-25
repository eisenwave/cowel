#include <algorithm>
#include <string_view>

#include "mmml/builtin_directive_set.hpp"
#include "mmml/directive_processing.hpp"

namespace mmml {
namespace {

// clang-format off
constexpr std::u8string_view mathml_names[] {
    u8"annotation",
    u8"maction",
    u8"menclose",
    u8"merror",
    u8"mfenced",
    u8"mfrac",
    u8"mi",
    u8"mmultiscripts",
    u8"mn",
    u8"mo",
    u8"mover",
    u8"mpadded",
    u8"mphantom",
    u8"mprescripts",
    u8"mroot",
    u8"mrow",
    u8"ms",
    u8"mspace",
    u8"msqrt",
    u8"mstyle",
    u8"msub",
    u8"msubsup",
    u8"msup",
    u8"mtable",
    u8"mtd",
    u8"mtext",
    u8"mtr",
    u8"munder",
    u8"munderover",
    u8"semantics",
};
// clang-format on

static_assert(std::ranges::is_sorted(mathml_names));

void to_math_html(HTML_Writer& out, std::span<const ast::Content> contents, Context& context)
{
    for (const ast::Content& c : contents) {
        const auto* const d = std::get_if<ast::Directive>(&c);
        if (!d) {
            to_html(out, c, context);
            continue;
        }
        const std::u8string_view name = d->get_name(context.get_source());
        if (!std::ranges::binary_search(mathml_names, name)) {
            to_html(out, *d, context);
            continue;
        }
        Attribute_Writer attributes = out.open_tag_with_attributes(name);
        arguments_to_attributes(attributes, *d, context);
        attributes.end();
        to_math_html(out, d->get_content(), context);
        out.close_tag(name);
    }
}

} // namespace

void Math_Behavior::generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const
{
    constexpr std::u8string_view tag_name = u8"math";
    const std::u8string_view display_string
        = display == Directive_Display::block ? u8"block" : u8"inline";

    Attribute_Writer attributes = out.open_tag_with_attributes(tag_name);
    attributes.write_attribute(u8"display", display_string);
    arguments_to_attributes(attributes, d, context);
    attributes.end();

    to_math_html(out, d.get_content(), context);

    out.close_tag(tag_name);
}

} // namespace mmml
