#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cowel/parameters.hpp"
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
#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

struct Put_Named {
    Context& context;

    const std::u8string_view needle_name;

    /// @brief Finds argument with name `needle_name`,
    /// recursively traversing any ellipses.
    /// @param members The group members, usually call arguments.
    /// @param frame The content frame of the call.
    [[nodiscard]]
    const ast::Group_Member*
    find(std::span<const ast::Group_Member> members, Frame_Index frame) const
    {
        for (const ast::Group_Member& arg : members) {
            switch (arg.get_kind()) {
            case ast::Member_Kind::positional: {
                continue;
            }
            case ast::Member_Kind::ellipsis: {
                const Stack_Frame& ellipsis_frame = context.get_call_stack()[frame];
                const auto* maybe_result = find(
                    ellipsis_frame.invocation.get_arguments_span(),
                    ellipsis_frame.invocation.content_frame
                );
                if (maybe_result) {
                    return maybe_result;
                }
                continue;
            }
            case ast::Member_Kind::named: {
                if (arg.get_name() == needle_name) {
                    return &arg;
                }
                continue;
            }
            }
        }
        return {};
    }
};

struct Put_Positional {
    Context& context;

    const std::size_t needle_index;
    std::size_t index = 0;

    /// @brief Finds argument with index `needle_index`,
    /// recursively traversing any ellipses.
    /// @param members The group members, usually call arguments.
    /// @param frame The content frame of the call.
    [[nodiscard]]
    const ast::Group_Member* find(std::span<const ast::Group_Member> members, Frame_Index frame)
    {
        for (const ast::Group_Member& arg : members) {
            switch (arg.get_kind()) {
            case ast::Member_Kind::named: {
                continue;
            }
            case ast::Member_Kind::ellipsis: {
                const Stack_Frame& ellipsis_frame = context.get_call_stack()[frame];
                const auto* maybe_result = find(
                    ellipsis_frame.invocation.get_arguments_span(),
                    ellipsis_frame.invocation.content_frame
                );
                if (maybe_result) {
                    return maybe_result;
                }
                continue;
            }
            case ast::Member_Kind::positional: {
                if (needle_index == index++) {
                    return &arg;
                }
                continue;
            }
            }
        }
        return {};
    }
};

}; // namespace

Processing_Status Macro_Behavior::do_evaluate(const Invocation& call, Context& context) const
{
    Group_Pack_String_Matcher strings { context.get_transient_memory() };
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
        if (alias_name.empty()) {
            context.try_fatal(
                diagnostic::macro_name_missing, location, u8"The alias name must not be empty."sv
            );
            return Processing_Status::fatal;
        }
        if (!is_directive_name(alias_name)) {
            context.try_fatal(
                diagnostic::macro_name_invalid, location,
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
                diagnostic::macro_duplicate, location,
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
    }

    return Processing_Status::ok;
}

Result<const ast::Member_Value*, Processing_Status>
Put_Behavior::resolve(const Invocation& call, Context& context) const
{
    constexpr const ast::Member_Value* found_content_result {};

    if (call.content_frame == Frame_Index::root) {
        context.try_error(
            diagnostic::put_outside, call.directive.get_source_span(),
            u8"\\cowel_put can only be used when expanded from macros, "
            u8"and this directive appeared at the top-level in the document."sv
        );
        return Processing_Status::error;
    }

    static const auto else_type = Type::canonical_union_of({ Type::block, Type::str });
    Lazy_Value_Of_Type_Matcher else_matcher { &else_type };
    Group_Member_Matcher else_member { u8"else"sv, Optionality::optional, else_matcher };
    Group_Member_Matcher* const parameters[] { &else_member };
    Pack_Usual_Matcher args_matcher { parameters };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    Call_Stack& stack = context.get_call_stack();
    const Invocation& target_invocation = stack[call.content_frame].invocation;

    // Simple case like \put where we expand the given contents.
    if (call.has_empty_content()) {
        return found_content_result;
    }

    const bool has_else = else_matcher.was_matched();

    std::pmr::vector<char8_t> target_text { context.get_transient_memory() };
    const Processing_Status target_status
        = splice_to_plaintext(target_text, call.get_content_span(), call.content_frame, context);
    if (target_status != Processing_Status::ok) {
        return target_status;
    }
    const auto target_string = as_u8string_view(target_text);

    // Simple case like \put where we expand the given contents.
    if (target_string.empty()) {
        return found_content_result;
    }

    const std::optional<std::size_t> needle_index = from_chars<std::size_t>(target_string);
    const ast::Group_Member* const arg = [&] -> const ast::Group_Member* {
        if (needle_index) {
            Put_Positional expand_positional {
                .context = context,
                .needle_index = *needle_index,
            };
            const ast::Group_Member* const maybe_result = expand_positional.find(
                target_invocation.get_arguments_span(), target_invocation.content_frame
            );
            if (!maybe_result && !has_else) {
                const Characters8 limit_chars = to_characters8(expand_positional.index);
                context.try_error(
                    diagnostic::put_out_of_range, call.directive.get_source_span(),
                    joined_char_sequence({
                        u8"This \\put directive is invalid "
                        u8"because the positional argument at index ["sv,
                        target_string,
                        u8"] was requested, but only "sv,
                        limit_chars.as_string(),
                        u8" were provided. "sv,
                    })
                );
            }
            return maybe_result;
        }
        // NOLINTNEXTLINE(readability-else-after-return)
        else {
            const Put_Named expand_named {
                .context = context,
                .needle_name = target_string,
            };
            const ast::Group_Member* const maybe_result = expand_named.find(
                target_invocation.get_arguments_span(), target_invocation.content_frame
            );
            if (!maybe_result && !has_else) {
                context.try_error(
                    diagnostic::put_invalid, call.get_arguments_source_span(),
                    joined_char_sequence({
                        u8"The target \""sv,
                        target_string,
                        u8"\" is neither an integer, "sv
                        u8"nor does it refer to any named argument of the macro invocation."sv,
                    })
                );
            }
            return maybe_result;
        }
    }();
    if (!arg) {
        if (!has_else) {
            // Error has already been printed above.
            return Processing_Status::error;
        }
        return &else_matcher.get();
    }

    return &arg->get_value();
}

// FIXME: Something is seriously inconsistent here.
//        `splice` should usually be a shortcut for evaluation
//        followed by splicing the result,
//        but this can't be reconciled with the fact
//        that `cowel_put` also inherits the paragraph when splicing.
Result<Value, Processing_Status>
Put_Behavior::evaluate(const Invocation& call, Context& context) const
{
    const Result<const ast::Member_Value*, Processing_Status> result = resolve(call, context);
    if (!result) {
        return result.error();
    }

    Call_Stack& stack = context.get_call_stack();
    const Invocation& target_invocation = stack[call.content_frame].invocation;

    if (*result == nullptr) {
        // FIXME: I'm pretty sure nothing so far has guaranteed that the block exists.
        //        We don't need to worry about that for splicing,
        //        but when evaluating, we need to obtain some Value.
        COWEL_ASSERT(target_invocation.content);
        return Value::block(*target_invocation.content, target_invocation.content_frame);
    }
    return evaluate_member_value(**result, target_invocation.content_frame, context);
}

Processing_Status
Put_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    // FIXME: Maybe this function should just go away
    const Result<const ast::Member_Value*, Processing_Status> result = resolve(call, context);
    if (!result) {
        return try_generate_error(out, call, context, result.error());
    }

    Call_Stack& stack = context.get_call_stack();
    const Invocation& target_invocation = stack[call.content_frame].invocation;

    try_inherit_paragraph(out);
    if (*result == nullptr) {
        return splice_all(
            out, target_invocation.get_content_span(), target_invocation.content_frame, context
        );
    }
    // FIXME: This may not be a spliceable value,
    //        such as when a group was found.
    return splice_value(out, **result, target_invocation.content_frame, context);
}

Processing_Status
Macro_Definition::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    try_inherit_paragraph(out);
    return splice_all(out, m_body, call.call_frame, context);
}

} // namespace cowel
