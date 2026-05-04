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

namespace cowel {
namespace {

template <typename T>
[[nodiscard]]
Processing_Status consume_simply(Content_Policy& out, const Invocation& call, Context& context)
{
    Block_Matcher content_matcher;
    Parameter content_param { u8"content"sv, Optionality::mandatory, content_matcher };
    Parameter* const parameters[] { &content_param };

    const auto match_status = match_call(parameters, call, context);
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    auto policy = [&] -> T {
        if constexpr (std::is_same_v<T, Paragraph_Split_Policy>) {
            return T { out, context.get_transient_memory() };
        }
        else if constexpr (std::is_same_v<T, HTML_Content_Policy>) {
            return ensure_html_policy(out);
        }
        else {
            return T { out };
        }
    }();
    const Processing_Status result = content_matcher.get().splice_block(policy, context);
    if constexpr (std::is_same_v<T, Paragraph_Split_Policy>) {
        policy.leave_paragraph();
    }
    return result;
}

[[nodiscard]]
Processing_Status consume_current(Content_Policy& out, const Invocation& call, Context& context)
{
    Block_Matcher content_matcher;
    Parameter content_param { u8"content"sv, Optionality::mandatory, content_matcher };
    Parameter* const parameters[] { &content_param };

    const auto match_status = match_call(parameters, call, context);
    if (match_status != Processing_Status::ok) {
        return match_status;
    }
    return content_matcher.get().splice_block(out, context);
}

[[nodiscard]]
Processing_Status
consume_syntax_highlighted(Content_Policy& out, const Invocation& call, Context& context)
{
    Spliceable_To_String_Matcher lang_string { context.get_transient_memory() };
    Parameter lang_param { u8"lang"sv, Optionality::mandatory, lang_string };
    Block_Matcher content_matcher;
    Parameter content_param { u8"content"sv, Optionality::mandatory, content_matcher };
    Parameter* const parameters[] { &lang_param, &content_param };

    const auto match_status = match_call(parameters, call, context);
    if (match_status != Processing_Status::ok) {
        return status_is_error(match_status) ? try_generate_error(out, call, context, match_status)
                                             : match_status;
    }

    Syntax_Highlight_Policy policy { context.get_transient_memory() };
    const Processing_Status consume_status = content_matcher.get().splice_block(policy, context);
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
        return consume_current(out, call, context);
    }
    case Known_Content_Policy::to_html: {
        return consume_simply<HTML_Content_Policy>(out, call, context);
    }
    case Known_Content_Policy::highlight: {
        return consume_syntax_highlighted(out, call, context);
    }
    case Known_Content_Policy::phantom: {
        return consume_simply<Phantom_Content_Policy>(out, call, context);
    }
    case Known_Content_Policy::paragraphs: {
        return consume_simply<Paragraph_Split_Policy>(out, call, context);
    }
    case Known_Content_Policy::no_invoke: {
        return consume_simply<Unprocessed_Content_Policy>(out, call, context);
    }
    case Known_Content_Policy::actions: {
        return consume_simply<Actions_Content_Policy>(out, call, context);
    }
    case Known_Content_Policy::text_only: {
        return consume_simply<Text_Only_Policy>(out, call, context);
    }
    case Known_Content_Policy::as_text: {
        return consume_simply<As_Text_Policy>(out, call, context);
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

[[nodiscard]]
Processing_Status Internal_Arg_Source_As_Text_Behavior::splice(
    Content_Policy& out,
    const Invocation& call,
    Context& context
) const
{
    Value_Of_Type_Matcher value_matcher { Type::block };
    Parameter value_param { u8"value"sv, Optionality::mandatory, value_matcher };
    Parameter* const parameters[] { &value_param };

    const auto match_status = match_call(parameters, call, context);
    if (match_status != Processing_Status::ok) {
        return status_is_error(match_status) ? try_generate_error(out, call, context, match_status)
                                             : match_status;
    }

    To_Source_Content_Policy policy { out };
    const Value& value = value_matcher.get();
    COWEL_ASSERT(value.is_block());
    return value.splice_block(policy, context);
}

} // namespace cowel
