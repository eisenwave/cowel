#include <string_view>
#include <vector>

#include "mmml/util/chars.hpp"
#include "mmml/util/strings.hpp"

#include "mmml/builtin_directive_set.hpp"
#include "mmml/directive_arguments.hpp"
#include "mmml/directive_processing.hpp"

namespace mmml {
namespace {

void trim_left(std::pmr::vector<char8_t>& text)
{
    const std::size_t amount = length_blank_left(as_u8string_view(text));
    MMML_ASSERT(amount <= text.size());
    text.erase(text.begin(), text.begin() + std::ptrdiff_t(amount));
}

void trim_right(std::pmr::vector<char8_t>& text)
{
    const std::size_t amount = length_blank_right(as_u8string_view(text));
    MMML_ASSERT(amount <= text.size());
    text.erase(text.end() - std::ptrdiff_t(amount), text.end());
}

void trim(std::pmr::vector<char8_t>& text)
{
    trim_left(text);
    trim_right(text);
}

void sanitize_id(std::pmr::vector<char8_t>& id)
{
    trim(id);
    for (char8_t& c : id) {
        if (is_ascii_upper_alpha(c)) {
            c = to_ascii_lower(c);
        }
        else if (is_html_whitespace(c)) {
            c = u8'-';
        }
    }
}

bool synthesize_id(
    std::pmr::vector<char8_t>& out,
    std::span<const ast::Content> content,
    Context& context
)
{
    // TODO: diagnostic on bad status
    const To_Plaintext_Status status
        = to_plaintext(out, content, context, To_Plaintext_Mode::no_side_effects);
    if (status == To_Plaintext_Status::error) {
        return false;
    }
    sanitize_id(out);
    return true;
}

} // namespace

void Heading_Behavior::generate_html(HTML_Writer& out, const ast::Directive& d, Context& context)
    const
{
    static constexpr std::u8string_view parameters[] { u8"id" };

    const auto level_char = char8_t(int(u8'0') + m_level);
    const char8_t tag_name_data[2] { u8'h', level_char };
    const std::u8string_view tag_name { tag_name_data, sizeof(tag_name_data) };

    Argument_Matcher args { parameters, context.get_transient_memory() };
    args.match(d.get_arguments(), context.get_source(), Parameter_Match_Mode::only_named);
    const int id_index = args.get_argument_index(u8"id");

    std::pmr::vector<char8_t> id_data { context.get_transient_memory() };
    bool has_id = false;

    // 1. Obtain or synthesize the id.
    Attribute_Writer attributes = out.open_tag_with_attributes(tag_name);
    if (id_index < 0) {
        if (synthesize_id(id_data, d.get_content(), context) && !id_data.empty()) {
            attributes.write_id(as_u8string_view(id_data));
            has_id = true;
        }
    }
    else {
        const ast::Argument& id_arg = d.get_arguments()[std::size_t(id_index)];
        const To_Plaintext_Status status = to_plaintext(id_data, id_arg.get_content(), context);
        if (status != To_Plaintext_Status::error) {
            attributes.write_id(as_u8string_view(id_data));
            has_id = !id_data.empty();
        }
    }
    for (std::size_t i = 0; i < d.get_arguments().size(); ++i) {
        if (int(i) != id_index) {
            argument_to_attribute(attributes, d.get_arguments()[i], context);
        }
    }
    attributes.end();

    // 2. Generate a paragraph symbol with anchor link.
    if (has_id) {
        id_data.insert(id_data.begin(), u8'#');
        out.open_tag_with_attributes(u8"a") //
            .write_class(u8"para")
            .write_href(as_u8string_view(id_data))
            .end();
        out.close_tag(u8"a");
    }

    // 3. Generate user content in the heading.
    to_html(out, d.get_content(), context);

    out.close_tag(tag_name);
}

} // namespace mmml
