#include <string_view>

#include "cowel/policy/html.hpp"
#include "cowel/policy/html_literal.hpp"
#include "cowel/policy/literally.hpp"
#include "cowel/policy/paragraph_split.hpp"
#include "cowel/policy/phantom.hpp"
#include "cowel/policy/plaintext.hpp"
#include "cowel/policy/syntax_highlight.hpp"
#include "cowel/policy/unprocessed.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/context.hpp"
#include "cowel/directive_arguments.hpp"
#include "cowel/directive_processing.hpp"

namespace cowel {
namespace {

template <typename T>
[[nodiscard]]
Processing_Status consume_simply(Content_Policy& out, const ast::Directive& d, Context& context)
{
    warn_all_args_ignored(d, context);
    T policy { out };
    return consume_all(policy, d.get_content(), context);
}

[[nodiscard]]
Processing_Status consume_paragraphs(Content_Policy& out, const ast::Directive& d, Context& context)
{
    warn_all_args_ignored(d, context);
    Paragraph_Split_Policy policy { out, context.get_transient_memory() };
    const Processing_Status result = consume_all(policy, d.get_content(), context);
    policy.leave_paragraph();
    return result;
}

[[nodiscard]]
Processing_Status
consume_syntax_highlighted(Content_Policy& out, const ast::Directive& d, Context& context)
{
    static constexpr auto lang_parameter = u8"lang"sv;
    static constexpr std::u8string_view parameters[] { lang_parameter };
    Argument_Matcher args { parameters, context.get_transient_memory() };
    args.match(d.get_arguments());

    warn_ignored_argument_subset(d.get_arguments(), args, context, Argument_Subset::unmatched);

    const Greedy_Result<String_Argument> lang
        = get_string_argument(lang_parameter, d, args, context);
    if (status_is_break(lang.status())) {
        return lang.status();
    }

    Syntax_Highlight_Policy policy { context.get_transient_memory() };
    const Processing_Status consume_status = consume_all(policy, d.get_content(), context);
    const Result<void, Syntax_Highlight_Error> result = policy.dump_to(out, context, lang->string);
    if (!result) {
        diagnose(result.error(), lang->string, d, context);
    }

    return status_concat(lang.status(), consume_status);
}

} // namespace

Processing_Status
Policy_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context) const
{
    switch (m_policy) {
    case Known_Content_Policy::current: {
        warn_all_args_ignored(d, context);
        return consume_all(out, d.get_content(), context);
    }
    case Known_Content_Policy::to_html: {
        return consume_simply<HTML_Content_Policy>(out, d, context);
    }
    case Known_Content_Policy::highlight: {
        return consume_syntax_highlighted(out, d, context);
    }
    case Known_Content_Policy::phantom: {
        return consume_simply<Phantom_Content_Policy>(out, d, context);
    }
    case Known_Content_Policy::paragraphs: {
        return consume_paragraphs(out, d, context);
    }
    case Known_Content_Policy::no_invoke: {
        return consume_simply<Unprocessed_Content_Policy>(out, d, context);
    }
    case Known_Content_Policy::text_only: {
        return consume_simply<Plaintext_Content_Policy>(out, d, context);
    }
    case Known_Content_Policy::text_as_html: {
        return consume_simply<HTML_Literal_Content_Policy>(out, d, context);
    }
    case Known_Content_Policy::source_as_text: {
        return consume_simply<To_Source_Content_Policy>(out, d, context);
    }
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid m_policy");
}

} // namespace cowel
