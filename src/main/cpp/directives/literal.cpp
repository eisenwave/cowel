#include <vector>

#include "cowel/util/html_writer.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/context.hpp"
#include "cowel/directive_processing.hpp"

namespace cowel {
namespace {

void warn_all_args_ignored(const ast::Directive& d, Context& context)
{
    if (context.emits(Severity::warning)) {
        for (const ast::Argument& arg : d.get_arguments()) {
            context.emit_warning(
                diagnostic::ignored_args, arg.get_source_span(), u8"This argument was ignored."
            );
        }
    }
}

} // namespace

void Literally_Behavior::generate_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    Context& context
) const
{
    warn_all_args_ignored(d, context);

    if (d.get_content().empty()) {
        return;
    }
    const std::size_t begin = ast::get_source_span(d.get_content().front()).begin;
    const std::size_t end = ast::get_source_span(d.get_content().back()).end();
    COWEL_ASSERT(end >= begin);
    const std::u8string_view source = context.get_source().substr(begin, end - begin);
    append(out, source);
}

void Unprocessed_Behavior::generate_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    Context& context
) const
{
    warn_all_args_ignored(d, context);

    if (d.get_content().empty()) {
        return;
    }
    for (const ast::Content& c : d.get_content()) {
        if (const auto* const directive = std::get_if<ast::Directive>(&c)) {
            append(out, directive->get_source(context.get_source()));
        }
        else {
            to_plaintext(out, c, context);
        }
    }
}

void HTML_Literal_Behavior::generate_html(
    HTML_Writer& out,
    const ast::Directive& d,
    Context& context
) const
{
    warn_all_args_ignored(d, context);

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
    warn_all_args_ignored(d, context);

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
