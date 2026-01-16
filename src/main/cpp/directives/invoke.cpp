#include <span>
#include <string_view>

#include "cowel/parameters.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/context.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"

namespace cowel {

[[nodiscard]]
Processing_Status
Invoke_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    Spliceable_To_String_Matcher directive_name_string { context.get_transient_memory() };
    Group_Member_Matcher string_argument { u8"name", Optionality::mandatory,
                                           directive_name_string };
    Group_Member_Matcher* parameters[] { &string_argument };
    Pack_Usual_Matcher args_matcher { parameters };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(
        call, context, make_fail_callback<Severity::fatal>(), Processing_Status::error
    );
    if (match_status != Processing_Status::ok) {
        return status_is_error(match_status) ? try_generate_error(out, call, context, match_status)
                                             : match_status;
    }

    const std::u8string_view name_string = directive_name_string.get();
    if (!is_identifier(name_string)) {
        context.try_error(
            diagnostic::invoke_name_invalid, directive_name_string.get_location(),
            joined_char_sequence(
                {
                    u8"The name \""sv,
                    name_string,
                    u8"\" is not a valid directive name."sv,
                }
            )
        );
        return try_generate_error(out, call, context);
    }

    const Directive_Behavior* const behavior = context.find_directive(name_string);
    if (!behavior) {
        context.try_error(
            diagnostic::invoke_lookup_failed, directive_name_string.get_location(),
            joined_char_sequence(
                {
                    u8"No directive with the name \""sv,
                    name_string,
                    u8"\" was found."sv,
                }
            )
        );
        return try_generate_error(out, call, context);
    }

    return splice_invocation(
        out, call.directive, name_string, nullptr, call.content, call.content_frame, context
    );
}

} // namespace cowel
