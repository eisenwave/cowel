#include "cowel/builtin_directive_set.hpp"
#include "cowel/context.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"
#include "cowel/parameters.hpp"

namespace cowel {

Processing_Status Alias_Behavior::do_evaluate(const Invocation& call, Context& context) const
{
    Pack_Of_Type_Matcher names { Type::pack_of(&Type::str) };
    Parameter names_param { u8"names"sv, Optionality::optional, names };
    Block_Matcher content_matcher;
    Parameter content_param { u8"content"sv, Optionality::mandatory, content_matcher };
    Parameter* const parameters[] { &names_param, &content_param };

    const Processing_Status match_status = match_call_fatal_error(parameters, call, context);
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

    for (const auto& [alias_name, location] : names.get()) {
        if (!alias_name.is_str()) {
            context.try_error(
                diagnostic::type_mismatch, location,
                joined_char_sequence(
                    {
                        u8"Macro names must be of type "sv,
                        Type::str.get_display_name(),
                        u8", but the argument is of type "sv,
                        alias_name.get_type().get_display_name(),
                        u8"."sv,
                    }
                )
            );
            return Processing_Status::error;
        }
    }

    const Value& content = content_matcher.get();
    std::pmr::vector<char8_t> target_text { context.get_transient_memory() };
    const Processing_Status target_status
        = splice_value_to_plaintext(target_text, content, content_matcher.get_location(), context);
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
    if (!is_identifier(target_name)) {
        context.try_fatal(
            diagnostic::alias_name_invalid, call.content->get_source_span(),
            joined_char_sequence(
                {
                    u8"The target name \""sv,
                    target_name,
                    u8"\" is not a valid directive name."sv,
                }
            )
        );
        return Processing_Status::fatal;
    }

    const Directive_Behavior* target_behavior = context.find_directive(target_name);
    if (!target_behavior) {
        context.try_fatal(
            diagnostic::alias_name_invalid, call.content->get_source_span(),
            joined_char_sequence(
                {
                    u8"No existing directive with the name \""sv,
                    target_name,
                    u8"\" was found. "sv,
                    u8"A directive (possibly macro) must be defined before an alias for it can be defined."sv,
                }
            )
        );
        return Processing_Status::fatal;
    }

    for (const auto& [alias_name_value, location] : names.get()) {
        const std::u8string_view alias_name = alias_name_value.as_string();
        if (alias_name.empty()) {
            context.try_fatal(
                diagnostic::alias_name_missing, location, u8"The alias name must not be empty."sv
            );
            return Processing_Status::fatal;
        }
        if (!is_identifier(alias_name)) {
            context.try_fatal(
                diagnostic::alias_name_invalid, location,
                joined_char_sequence(
                    {
                        u8"The alias name \""sv,
                        alias_name,
                        u8"\" is not a valid directive name."sv,
                    }
                )
            );
            return Processing_Status::fatal;
        }
        if (context.find_macro(alias_name) || context.find_alias(alias_name)) {
            context.try_fatal(
                diagnostic::alias_duplicate, location,
                joined_char_sequence(
                    {
                        u8"The alias name \""sv,
                        alias_name,
                        u8"\" is already defined as a macro or alias. "sv,
                        u8"Redefinitions or duplicate definitions are not allowed."sv,
                    }
                )
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
