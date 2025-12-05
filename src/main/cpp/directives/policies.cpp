#include <string_view>

#include "cowel/parameters.hpp"
#include "cowel/util/assert.hpp"
#include "cowel/util/result.hpp"

#include "cowel/policy/actions.hpp"
#include "cowel/policy/content_policy.hpp"
#include "cowel/policy/factory.hpp"
#include "cowel/policy/html.hpp"
#include "cowel/policy/html_literal.hpp"
#include "cowel/policy/literally.hpp"
#include "cowel/policy/paragraph_split.hpp"
#include "cowel/policy/phantom.hpp"
#include "cowel/policy/plaintext.hpp"
#include "cowel/policy/syntax_highlight.hpp"
#include "cowel/policy/unprocessed.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/invocation.hpp"
#include "cowel/services.hpp"

namespace cowel {
namespace {

template <typename T>
[[nodiscard]]
Processing_Status consume_simply(Content_Policy& out, const Invocation& call, Context& context)
{
    const auto match_status = match_empty_arguments(call, context);
    if (match_status != Processing_Status::ok) {
        return match_status;
    }
    T policy { out };
    return splice_all(policy, call.get_content_span(), call.content_frame, context);
}

[[nodiscard]]
Processing_Status consume_paragraphs(Content_Policy& out, const Invocation& call, Context& context)
{
    const auto match_status = match_empty_arguments(call, context);
    if (match_status != Processing_Status::ok) {
        return match_status;
    }
    Paragraph_Split_Policy policy { out, context.get_transient_memory() };
    const Processing_Status result
        = splice_all(policy, call.get_content_span(), call.content_frame, context);
    policy.leave_paragraph();
    return result;
}

[[nodiscard]]
Processing_Status
consume_syntax_highlighted(Content_Policy& out, const Invocation& call, Context& context)
{
    Spliceable_To_String_Matcher lang_string { context.get_transient_memory() };
    Group_Member_Matcher lang_member { u8"lang"sv, Optionality::mandatory, lang_string };
    Group_Member_Matcher* parameters[] { &lang_member };
    Pack_Usual_Matcher args_matcher { parameters };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return status_is_error(match_status) ? try_generate_error(out, call, context, match_status)
                                             : match_status;
    }

    Syntax_Highlight_Policy policy { context.get_transient_memory() };
    const Processing_Status consume_status
        = splice_all(policy, call.get_content_span(), call.content_frame, context);
    const Result<void, Syntax_Highlight_Error> result
        = policy.dump_html_to(out, context, lang_string.get());
    if (!result) {
        diagnose(result.error(), lang_string.get(), call, context);
    }

    return consume_status;
}

} // namespace

Processing_Status
Policy_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    switch (m_policy) {
    case Known_Content_Policy::current: {
        const auto match_status = match_empty_arguments(call, context);
        if (match_status != Processing_Status::ok) {
            return match_status;
        }
        return splice_all(out, call.get_content_span(), call.content_frame, context);
    }
    case Known_Content_Policy::to_html: {
        const auto match_status = match_empty_arguments(call, context);
        if (match_status != Processing_Status::ok) {
            return match_status;
        }
        HTML_Content_Policy policy = ensure_html_policy(out);
        return splice_all(policy, call.get_content_span(), call.content_frame, context);
    }
    case Known_Content_Policy::highlight: {
        return consume_syntax_highlighted(out, call, context);
    }
    case Known_Content_Policy::phantom: {
        return consume_simply<Phantom_Content_Policy>(out, call, context);
    }
    case Known_Content_Policy::paragraphs: {
        return consume_paragraphs(out, call, context);
    }
    case Known_Content_Policy::no_invoke: {
        return consume_simply<Unprocessed_Content_Policy>(out, call, context);
    }
    case Known_Content_Policy::actions: {
        return consume_simply<Actions_Content_Policy>(out, call, context);
    }
    case Known_Content_Policy::text_only: {
        return consume_simply<Plaintext_Content_Policy>(out, call, context);
    }
    case Known_Content_Policy::text_as_html: {
        return consume_simply<HTML_Literal_Content_Policy>(out, call, context);
    }
    case Known_Content_Policy::source_as_text: {
        return consume_simply<To_Source_Content_Policy>(out, call, context);
    }
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid m_policy");
}

} // namespace cowel
