#include <string_view>
#include <vector>

#include "cowel/parameters.hpp"
#include "cowel/util/html_writer.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/policy/content_policy.hpp"
#include "cowel/policy/html.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/invocation.hpp"

using namespace std::string_view_literals;

namespace cowel {

Processing_Status
WG21_Head_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context) const
{
    Lazy_Markup_Matcher title_markup;
    Group_Member_Matcher title_member { u8"title"sv, Optionality::mandatory, title_markup };
    Group_Member_Matcher* const parameters[] { &title_member };
    Pack_Usual_Matcher args_matcher { parameters };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    try_leave_paragraph(out);

    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Sink_HTML_Writer writer { buffer };
    writer
        .open_tag_with_attributes(html_tag::div) //
        .write_class(u8"wg21-head"sv)
        .end();

    {
        // FIXME: multiple evaluations of title input
        std::pmr::vector<char8_t> title_plaintext { context.get_transient_memory() };
        const auto status = to_plaintext(
            title_plaintext, title_markup.get().get_elements(), title_markup.get_frame(), context
        );
        if (status != Processing_Status::ok) {
            writer.close_tag(html_tag::div);
            return status;
        }

        const auto title_string = as_u8string_view(title_plaintext);

        const auto scope = context.get_sections().go_to_scoped(section_name::document_head);
        HTML_Writer_Buffer buffer { context.get_sections().current_policy(),
                                    Output_Language::html };
        Text_Buffer_HTML_Writer { buffer }
            .open_tag(html_tag::title)
            .write_inner_text(title_string)
            .close_tag(html_tag::title);
    }

    writer.open_tag(html_tag::h1);

    HTML_Content_Policy html_policy { buffer };
    const auto title_status = consume_all(
        html_policy, title_markup.get().get_elements(), title_markup.get_frame(), context
    );

    writer.close_tag(html_tag::h1);
    if (status_is_break(title_status)) {
        return title_status;
    }

    writer.write_inner_html(u8'\n');
    const auto status
        = consume_all(html_policy, call.get_content_span(), call.content_frame, context);
    writer.close_tag(html_tag::div);

    return status;
}

} // namespace cowel
