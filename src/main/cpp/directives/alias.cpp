#include "cowel/builtin_directive_set.hpp"
#include "cowel/context.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"

namespace cowel {

Processing_Status
Alias_Behavior::operator()(Content_Policy&, const Invocation& call, Context& context) const
{
    warn_ignored_argument_subset(call.arguments, context, Argument_Subset::named);

    std::pmr::vector<char8_t> target_text { context.get_transient_memory() };
    const Processing_Status target_status
        = to_plaintext(target_text, call.get_content_span(), call.content_frame, context);
    switch (target_status) {
    case Processing_Status::ok: break;
    case Processing_Status::brk:
    case Processing_Status::fatal: return target_status;
    case Processing_Status::error:
    case Processing_Status::error_brk: {
        context.try_fatal(
            diagnostic::alias_name_invalid, call.get_content_source_span(),
            u8"Fatal error because generation of the target name failed."sv
        );
        return Processing_Status::fatal;
    }
    }
    const auto target_name = as_u8string_view(target_text);

    if (target_name.empty()) {
        context.try_fatal(
            diagnostic::alias_name_missing, call.directive.get_source_span(),
            u8"The target name must not be empty."sv
        );
        return Processing_Status::fatal;
    }
    COWEL_ASSERT(call.content);
    if (!is_directive_name(target_name)) {
        context.try_fatal(
            diagnostic::alias_name_invalid, call.content->get_source_span(),
            joined_char_sequence({
                u8"The target name \""sv,
                target_name,
                u8"\" is not a valid directive name."sv,
            })
        );
        return Processing_Status::fatal;
    }

    const Directive_Behavior* target_behavior = context.find_directive(target_name);
    if (!target_behavior) {
        context.try_fatal(
            diagnostic::alias_name_invalid, call.content->get_source_span(),
            joined_char_sequence({
                u8"No existing directive with the name \""sv,
                target_name,
                u8"\" was found. "sv,
                u8"A directive (possibly macro) must be defined before an alias for it can be defined."sv,
            })
        );
        return Processing_Status::fatal;
    }

    std::pmr::vector<char8_t> alias_text { context.get_transient_memory() };
    for (const Argument_Ref ref : call.arguments) {
        const ast::Content_Sequence* arg_content
            = as_content_or_error(ref.ast_node.get_value(), context);
        if (!arg_content) {
            return Processing_Status::fatal;
        }

        const Processing_Status name_status
            = to_plaintext(alias_text, arg_content->get_elements(), ref.frame_index, context);
        switch (name_status) {
        case Processing_Status::ok: break;
        case Processing_Status::brk:
        case Processing_Status::fatal: return name_status;
        case Processing_Status::error:
        case Processing_Status::error_brk: {
            COWEL_ASSERT(call.content);
            context.try_fatal(
                diagnostic::alias_name_invalid, call.content->get_source_span(),
                u8"Fatal error because generation of an alias failed."sv
            );
        }
        }
        const auto alias_name = as_u8string_view(alias_text);
        if (alias_name.empty()) {
            context.try_fatal(
                diagnostic::alias_name_missing, ref.ast_node.get_source_span(),
                u8"The alias name must not be empty."sv
            );
            return Processing_Status::fatal;
        }
        if (!is_directive_name(alias_name)) {
            COWEL_ASSERT(!arg_content->empty());
            context.try_fatal(
                diagnostic::alias_name_invalid, arg_content->get_source_span(),
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
                diagnostic::alias_duplicate, arg_content->get_source_span(),
                joined_char_sequence({
                    u8"The alias name \""sv,
                    alias_name,
                    u8"\" is already defined as a macro or alias. "sv,
                    u8"Redefinitions or duplicate definitions are not allowed."sv,
                })
            );
            return Processing_Status::fatal;
        }
        const bool success = context.emplace_alias(
            std::pmr::u8string { alias_name, context.get_transient_memory() }, target_behavior
        );
        COWEL_ASSERT(success);
        alias_text.clear();
    }

    return Processing_Status::ok;
}

} // namespace cowel
