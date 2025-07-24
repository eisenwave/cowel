#include <vector>

#include "cowel/policy/html_literal.hpp"
#include "cowel/policy/literally.hpp"
#include "cowel/policy/unprocessed.hpp"
#include "cowel/util/html_writer.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/context.hpp"
#include "cowel/directive_processing.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

void warn_all_args_ignored(const ast::Directive& d, Context& context)
{
    if (context.emits(Severity::warning)) {
        for (const ast::Argument& arg : d.get_arguments()) {
            context.emit_warning(
                diagnostic::ignored_args, arg.get_source_span(), u8"This argument was ignored."sv
            );
        }
    }
}

} // namespace

Processing_Status
Literally_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context) const
{
    warn_all_args_ignored(d, context);

    try_enter_paragraph(out);

    To_Source_Content_Policy policy { out };
    return consume_all(policy, d.get_content(), context);
}

Processing_Status
Unprocessed_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context)
    const
{
    warn_all_args_ignored(d, context);

    try_enter_paragraph(out);

    Unprocessed_Content_Policy policy { out };
    return consume_all(policy, d.get_content(), context);
}

Processing_Status
HTML_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context) const
{
    warn_all_args_ignored(d, context);

    ensure_paragraph_matches_display(out, m_display);

    HTML_Literal_Content_Policy policy { out };
    return consume_all(policy, d.get_content(), context);
}

Processing_Status
HTML_Raw_Text_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context)
    const
{
    warn_all_args_ignored(d, context);
    warn_ignored_argument_subset(d.get_arguments(), context, Argument_Subset::positional);

    try_leave_paragraph(out);

    HTML_Writer writer { out };
    Attribute_Writer attributes = writer.open_tag_with_attributes(m_tag_name);
    const auto attributes_status = named_arguments_to_attributes(attributes, d, context);
    attributes.end();
    if (status_is_break(attributes_status)) {
        return attributes_status;
    }
    Processing_Status status = attributes_status;

    std::pmr::vector<char8_t> buffer { context.get_transient_memory() };
    const auto content_status = to_plaintext(buffer, d.get_content(), context);
    status = status_concat(status, content_status);
    if (status_is_continue(content_status)) {
        COWEL_ASSERT(m_tag_name == u8"style"sv || m_tag_name == u8"script"sv);
        const std::u8string_view needle
            = m_tag_name == u8"style"sv ? u8"</style"sv : u8"</script"sv;
        if (as_u8string_view(buffer).contains(needle)) {
            context.try_error(
                diagnostic::raw_text_closing, d.get_source_span(),
                joined_char_sequence({
                    u8"The content within this directive unexpectedly contained a closing \"",
                    needle,
                    u8"\", which would result in producing malformed HTML.",
                })
            );
            status = status_concat(status, Processing_Status::error);
        }
        else {
            writer.write_inner_html(as_u8string_view(buffer));
        }
    }
    writer.close_tag(m_tag_name);
    return status;
}

} // namespace cowel
