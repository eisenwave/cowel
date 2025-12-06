#include "cowel/builtin_directive_set.hpp"
#include "cowel/context.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"
#include "cowel/parameters.hpp"

namespace cowel {

Processing_Status Alias_Behavior::do_evaluate(const Invocation& call, Context& context) const
{
    Group_Pack_Value_Matcher strings { context.get_transient_memory() };
    Call_Matcher call_matcher { strings };

    const Processing_Status match_status
        = call_matcher.match_call(call, context, make_fail_callback(), Processing_Status::fatal);
    switch (match_status) {
    case Processing_Status::ok: break;
    case Processing_Status::brk:
    case Processing_Status::fatal: return match_status;
    case Processing_Status::error:
    case Processing_Status::error_brk: {
        COWEL_ASSERT(call.content);
        context.try_fatal(
            diagnostic::alias_name_invalid, call.content->get_source_span(),
            u8"Fatal error because generation of an alias failed."sv
        );
        return Processing_Status::fatal;
    }
    }

    for (const auto& [alias_name, location] : strings.get_values()) {
        if (!alias_name.is_str()) {
            context.try_error(
                diagnostic::type_mismatch, location,
                joined_char_sequence({
                    u8"Macro names must be of type "sv,
                    Type::str.get_display_name(),
                    u8", but the argument is of type "sv,
                    alias_name.get_type().get_display_name(),
                    u8"."sv,
                })
            );
            return Processing_Status::error;
        }
    }

    std::pmr::vector<char8_t> target_text { context.get_transient_memory() };
    const Processing_Status target_status
        = splice_to_plaintext(target_text, call.get_content_span(), call.content_frame, context);
    switch (target_status) {
    case Processing_Status::ok: break;
    case Processing_Status::brk:
    case Processing_Status::fatal: return target_status;
    case Processing_Status::error:
    case Processing_Status::error_brk: {
        context.try_fatal(
            diagnostic::alias_name_invalid, call.get_arguments_source_span(),
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

    for (const auto& [alias_name_value, location] : strings.get_values()) {
        const std::u8string_view alias_name = alias_name_value.as_string();
        if (alias_name.empty()) {
            context.try_fatal(
                diagnostic::alias_name_missing, location, u8"The alias name must not be empty."sv
            );
            return Processing_Status::fatal;
        }
        if (!is_directive_name(alias_name)) {
            context.try_fatal(
                diagnostic::alias_name_invalid, location,
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
                diagnostic::alias_duplicate, location,
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
    }

    return Processing_Status::ok;
}

} // namespace cowel
