#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/from_chars.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/to_chars.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_arguments.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"

using namespace std::string_view_literals;

namespace cowel {

Processing_Status
Macro_Behavior::operator()(Content_Policy&, const Invocation& call, Context& context) const
{
    warn_ignored_argument_subset(call.arguments, context, Argument_Subset::named);

    std::pmr::vector<char8_t> alias_text { context.get_transient_memory() };
    for (const Argument_Ref ref : call.arguments) {
        const auto* const ref_content
            = as_content_or_fatal_error(ref.ast_node.get_value(), context);
        if (!ref_content) {
            return Processing_Status::fatal;
        }

        const Processing_Status name_status
            = to_plaintext(alias_text, ref_content->get_elements(), ref.frame_index, context);
        switch (name_status) {
        case Processing_Status::ok: break;
        case Processing_Status::brk:
        case Processing_Status::fatal: return name_status;
        case Processing_Status::error:
        case Processing_Status::error_brk: {
            context.try_fatal(
                diagnostic::alias_name_invalid, ref.ast_node.get_value().get_source_span(),
                u8"Fatal error because generation of an alias failed."sv
            );
            return Processing_Status::fatal;
        }
        }
        const auto alias_name = as_u8string_view(alias_text);
        if (alias_name.empty()) {
            context.try_fatal(
                diagnostic::macro_name_missing, ref.ast_node.get_source_span(),
                u8"The alias name must not be empty."sv
            );
            return Processing_Status::fatal;
        }
        if (!is_directive_name(alias_name)) {
            COWEL_ASSERT(!ref_content->empty());
            context.try_fatal(
                diagnostic::macro_name_invalid, ref_content->get_source_span(),
                joined_char_sequence({
                    u8"The alias name \""sv,
                    alias_name,
                    u8"\" is not a valid directive name."sv,
                })
            );
            return Processing_Status::fatal;
        }
        if (context.find_macro(alias_name) || context.find_alias(alias_name)) {
            context.try_fatal(
                diagnostic::macro_duplicate, ref_content->get_source_span(),
                joined_char_sequence({
                    u8"The alias name \""sv,
                    alias_name,
                    u8"\" is already defined as a macro or alias. "sv,
                    u8"Redefinitions or duplicate definitions are not allowed."sv,
                })
            );
            return Processing_Status::fatal;
        }
        const bool success = context.emplace_macro(
            std::pmr::u8string { alias_name, context.get_transient_memory() },
            call.get_content_span()
        );
        COWEL_ASSERT(success);
        alias_text.clear();
    }

    return Processing_Status::ok;
}

Processing_Status
Put_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context) const
{
    if (call.content_frame == Frame_Index::root) {
        context.try_error(
            diagnostic::put_outside, call.directive.get_source_span(),
            u8"\\cowel_put can only be used when expanded from macros, "
            u8"and this directive appeared at the top-level in the document."sv
        );
        return try_generate_error(out, call, context);
    }

    static constexpr std::u8string_view parameters[] { u8"else" };
    Argument_Matcher matcher { parameters, context.get_transient_memory() };
    matcher.match(call.arguments);

    try_inherit_paragraph(out);

    Call_Stack& stack = context.get_call_stack();
    const Invocation& target_invocation = stack[call.content_frame].invocation;

    if (call.has_empty_content()) {
        return consume_all(
            out, target_invocation.get_content_span(), target_invocation.content_frame, context
        );
    }

    const auto try_else = [&] -> std::optional<Processing_Status> {
        const int else_index = matcher.parameter_indices()[0];
        if (else_index < 0) {
            return {};
        }
        const Argument_Ref else_arg = call.arguments[std::size_t(else_index)];
        return consume_all(out, else_arg.ast_node.get_value(), else_arg.frame_index, context);
    };

    std::pmr::vector<char8_t> target_text { context.get_transient_memory() };
    const Processing_Status target_status
        = to_plaintext(target_text, call.get_content_span(), call.content_frame, context);
    if (target_status != Processing_Status::ok) {
        return target_status;
    }
    const auto target_string = as_u8string_view(target_text);

    // Simple case like \put where we expand the given contents.
    if (target_string.empty()) {
        return consume_all(
            out, target_invocation.get_content_span(), target_invocation.content_frame, context
        );
    }

    // Index case like \put{0} for selecting a given argument,
    // possibly with a fallback like \put[else=abc]{0}.
    const std::optional<std::size_t> arg_index = from_chars<std::size_t>(target_string);
    if (!arg_index) {
        for (const Argument_Ref arg : target_invocation.arguments) {
            if (arg.ast_node.get_name() == target_string) {
                return consume_all(out, arg.ast_node.get_value(), arg.frame_index, context);
            }
        }
        if (const std::optional<Processing_Status> else_status = try_else()) {
            return *else_status;
        }
        context.try_error(
            diagnostic::put_invalid, call.get_content_source_span(),
            joined_char_sequence({
                u8"The target \""sv,
                target_string,
                u8"\" is neither an integer, "sv
                u8"nor does it refer to any named argument of the macro invocation."sv,
            })
        );
        return try_generate_error(out, call, context);
    }

    std::size_t positional_index = 0;
    for (const Argument_Ref arg : target_invocation.arguments) {
        if (arg.ast_node.get_kind() != ast::Member_Kind::positional) {
            continue;
        }
        if (*arg_index == positional_index++) {
            return consume_all(out, arg.ast_node.get_value(), arg.frame_index, context);
        }
    }
    if (const std::optional<Processing_Status> else_status = try_else()) {
        return *else_status;
    }
    const Characters8 limit_chars = to_characters8(positional_index);
    context.try_error(
        diagnostic::put_out_of_range, call.directive.get_source_span(),
        joined_char_sequence({
            u8"This \\put directive is invalid because the positional argument at index ["sv,
            target_string,
            u8"] was requested, but only "sv,
            limit_chars.as_string(),
            u8" were provided. "sv,
        })
    );

    return try_generate_error(out, call, context);
}

Processing_Status
Macro_Definition::operator()(Content_Policy& out, const Invocation& call, Context& context) const
{
    try_inherit_paragraph(out);
    return consume_all(out, m_body, call.call_frame, context);
}

} // namespace cowel
