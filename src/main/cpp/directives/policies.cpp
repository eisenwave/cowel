#include <string_view>

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
#include "cowel/directive_arguments.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/invocation.hpp"
#include "cowel/services.hpp"

namespace cowel {
namespace {

template <typename T>
[[nodiscard]]
Processing_Status consume_simply(Content_Policy& out, const Invocation& call, Context& context)
{
    warn_all_args_ignored(call, context);
    T policy { out };
    return consume_all(policy, call.content, call.content_frame, context);
}

[[nodiscard]]
Processing_Status consume_paragraphs(Content_Policy& out, const Invocation& call, Context& context)
{
    warn_all_args_ignored(call, context);
    Paragraph_Split_Policy policy { out, context.get_transient_memory() };
    const Processing_Status result = consume_all(policy, call.content, call.content_frame, context);
    policy.leave_paragraph();
    return result;
}

[[nodiscard]]
Processing_Status
consume_syntax_highlighted(Content_Policy& out, const Invocation& call, Context& context)
{
    static constexpr auto lang_parameter = u8"lang"sv;
    static constexpr std::u8string_view parameters[] { lang_parameter };
    Argument_Matcher args { parameters, context.get_transient_memory() };
    args.match(call.arguments);

    warn_ignored_argument_subset(call.arguments, args, context, Argument_Subset::unmatched);

    const Greedy_Result<String_Argument> lang
        = get_string_argument(lang_parameter, call.arguments, args, context);
    if (status_is_break(lang.status())) {
        return lang.status();
    }

    Syntax_Highlight_Policy policy { context.get_transient_memory() };
    const Processing_Status consume_status
        = consume_all(policy, call.content, call.content_frame, context);
    const Result<void, Syntax_Highlight_Error> result
        = policy.dump_html_to(out, context, lang->string);
    if (!result) {
        diagnose(result.error(), lang->string, call, context);
    }

    return status_concat(lang.status(), consume_status);
}

} // namespace

Processing_Status
Policy_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context) const
{
    switch (m_policy) {
    case Known_Content_Policy::current: {
        warn_all_args_ignored(call, context);
        return consume_all(out, call.content, call.content_frame, context);
    }
    case Known_Content_Policy::to_html: {
        warn_all_args_ignored(call, context);
        HTML_Content_Policy policy = ensure_html_policy(out);
        return consume_all(policy, call.content, call.content_frame, context);
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
