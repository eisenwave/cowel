#include "cowel/util/strings.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/context.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"

namespace cowel {

[[nodiscard]]
Processing_Status
Invoke_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context) const
{
    warn_ignored_argument_subset(call.arguments, context, Argument_Subset::named);

    const std::optional<Argument_Ref> first_positional
        = get_first_positional_warn_rest(call.arguments, context);
    if (!first_positional) {
        context.try_error(
            diagnostic::invoke_name_missing, call.directive.get_name_span(),
            u8"A directive name name must be provided (in the form of a positional argument)."sv
        );
        return try_generate_error(out, call, context);
    }

    std::pmr::vector<char8_t> name_text { context.get_transient_memory() };
    const Processing_Status name_status = to_plaintext(
        name_text, first_positional->ast_node.get_content(), first_positional->frame_index, context
    );
    if (name_status != Processing_Status::ok) {
        return name_status;
    }
    const auto name_string = as_u8string_view(name_text);
    if (!is_directive_name(name_string)) {
        context.try_error(
            diagnostic::invoke_name_invalid, first_positional->ast_node.get_source_span(),
            joined_char_sequence({
                u8"The name \""sv,
                name_string,
                u8"\" is not a valid directive name."sv,
            })
        );
        return try_generate_error(out, call, context);
    }

    const Directive_Behavior* const behavior = context.find_directive(name_string);
    if (!behavior) {
        context.try_error(
            diagnostic::invoke_lookup_failed, first_positional->ast_node.get_source_span(),
            joined_char_sequence({
                u8"No directive with the name \""sv,
                name_string,
                u8"\" was found."sv,
            })
        );
        return try_generate_error(out, call, context);
    }

    const Invocation indirect_invocation {
        .name = name_string,
        .directive = call.directive,
        .arguments = {},
        .content = call.content,
        .content_frame = call.content_frame,
        .call_frame = call.call_frame + 1,
    };
    return (*behavior)(out, indirect_invocation, context);
}

} // namespace cowel
