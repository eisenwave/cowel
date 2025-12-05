#include <string_view>
#include <vector>

#include "cowel/parameters.hpp"
#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/html_writer.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/policy/content_policy.hpp"
#include "cowel/policy/literally.hpp"
#include "cowel/policy/unprocessed.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/invocation.hpp"

using namespace std::string_view_literals;

namespace cowel {

Processing_Status
Literally_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    const auto match_status = match_empty_arguments(call, context);
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    try_enter_paragraph(out);

    To_Source_Content_Policy policy { out };
    return splice_all(policy, call.get_content_span(), call.content_frame, context);
}

Processing_Status
Unprocessed_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    const auto match_status = match_empty_arguments(call, context);
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    try_enter_paragraph(out);

    Unprocessed_Content_Policy policy { out };
    return splice_all(policy, call.get_content_span(), call.content_frame, context);
}

Processing_Status
HTML_Raw_Text_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    Group_Pack_Named_Lazy_Spliceable_Matcher args_matcher {};
    Call_Matcher call_matcher { args_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    try_leave_paragraph(out);

    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    Text_Buffer_Attribute_Writer attributes = writer.open_tag_with_attributes(m_tag_name);
    const auto attributes_status = named_arguments_to_attributes(
        attributes, call.get_arguments_span(), call.content_frame, context
    );
    attributes.end();
    if (status_is_break(attributes_status)) {
        return attributes_status;
    }
    Processing_Status status = attributes_status;

    std::pmr::vector<char8_t> raw_text { context.get_transient_memory() };
    const auto content_status
        = splice_to_plaintext(raw_text, call.get_content_span(), call.content_frame, context);
    status = status_concat(status, content_status);
    if (status_is_continue(content_status)) {
        COWEL_ASSERT(m_tag_name == u8"style"sv || m_tag_name == u8"script"sv);
        const std::u8string_view needle
            = m_tag_name == u8"style"sv ? u8"</style"sv : u8"</script"sv;
        if (as_u8string_view(raw_text).contains(needle)) {
            context.try_error(
                diagnostic::raw_text_closing, call.directive.get_source_span(),
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
