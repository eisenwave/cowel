#include <string_view>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/html_writer.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/policy/content_policy.hpp"
#include "cowel/policy/html_literal.hpp"
#include "cowel/policy/literally.hpp"
#include "cowel/policy/unprocessed.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_processing.hpp"

using namespace std::string_view_literals;

namespace cowel {

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

    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    Text_Buffer_Attribute_Writer attributes = writer.open_tag_with_attributes(m_tag_name);
    const auto attributes_status = named_arguments_to_attributes(attributes, d, context);
    attributes.end();
    if (status_is_break(attributes_status)) {
        return attributes_status;
    }
    Processing_Status status = attributes_status;

    std::pmr::vector<char8_t> raw_text { context.get_transient_memory() };
    const auto content_status = to_plaintext(raw_text, d.get_content(), context);
    status = status_concat(status, content_status);
    if (status_is_continue(content_status)) {
        COWEL_ASSERT(m_tag_name == u8"style"sv || m_tag_name == u8"script"sv);
        const std::u8string_view needle
            = m_tag_name == u8"style"sv ? u8"</style"sv : u8"</script"sv;
        if (as_u8string_view(raw_text).contains(needle)) {
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
            writer.write_inner_html(as_u8string_view(raw_text));
        }
    }

    writer.close_tag(m_tag_name);
    buffer.flush();
    return status;
}

} // namespace cowel
