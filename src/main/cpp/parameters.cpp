#include <algorithm>
#include <string_view>
#include <variant>

#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/from_chars.hpp"

#include "cowel/context.hpp"
#include "cowel/parameters.hpp"

namespace cowel {

Processing_Status Content_Value_Matcher::match_value(
    const ast::Value& argument,
    Frame_Index frame,
    Context& context,
    const Match_Fail_Options& on_fail
)
{
    COWEL_DEBUG_ASSERT(status_is_error(on_fail.status));

    const auto* const content = std::get_if<ast::Content_Sequence>(&argument);
    if (!content) {
        COWEL_ASSERT(std::holds_alternative<ast::Group>(argument));
        on_fail.emit(
            argument.get_source_span(),
            u8"Expected a sequence of markup content, but got a group."sv, context
        );
        return on_fail.status;
    }
    return match_markup_value(*content, frame, context, on_fail);
}

Processing_Status Textual_Matcher::match_markup_value(
    const ast::Content_Sequence& argument,
    Frame_Index frame,
    Context& context,
    const Match_Fail_Options& on_fail
)
{
    COWEL_DEBUG_ASSERT(status_is_error(on_fail.status));

    std::pmr::vector<char8_t> buffer { context.get_transient_memory() };
    const auto [status, string]
        = to_plaintext_optimistic(buffer, argument.get_elements(), frame, context);
    if (status != Processing_Status::ok) {
        return status;
    }
    return match_string(argument, string, context, on_fail.emit) ? Processing_Status::ok
                                                                 : on_fail.status;
}

Processing_Status String_Matcher::match_markup_value(
    const ast::Content_Sequence& argument, //
    Frame_Index frame, //
    Context& context, //
    const Match_Fail_Options& on_fail
)
{
    COWEL_DEBUG_ASSERT(status_is_error(on_fail.status));

    std::pmr::vector<char8_t> buffer { context.get_transient_memory() };
    const auto [status, string]
        = to_plaintext_optimistic(buffer, argument.get_elements(), frame, context);
    if (status != Processing_Status::ok) {
        return status;
    }
    if (buffer.empty()) {
        m_value = { string, argument.get_source_span() };
        return Processing_Status::ok;
    }
    m_data = std::move(buffer);
    m_value = { as_u8string_view(m_data), argument.get_source_span() };
    return Processing_Status::ok;
}

bool Boolean_Matcher::match_string(
    const ast::Content_Sequence& argument,
    std::u8string_view str,
    Context& context,
    Fail_Callback on_fail
)
{
    if (str == u8"true"sv) {
        m_value = { true, argument.get_source_span() };
        return true;
    }
    if (str == u8"false"sv) {
        m_value = { false, argument.get_source_span() };
        return true;
    }
    on_fail(
        argument.get_source_span(),
        joined_char_sequence({
            u8"Expected the given argument to be \"true\" or \"false\", but got \"",
            str,
            u8"\".",
        }),
        context
    );
    return false;
}

bool Integer_Matcher::match_string(
    const ast::Content_Sequence& argument,
    std::u8string_view str,
    Context& context,
    Fail_Callback on_fail
)
{
    const std::optional<long long> parsed = from_chars<long long>(str);
    if (!parsed) {
        on_fail(
            argument.get_source_span(),
            joined_char_sequence({ u8"\"", str, u8"\" is not a valid integer." }), context
        );
        return false;
    }
    m_value = { *parsed, argument.get_source_span() };
    return true;
}

bool Sorted_Options_Matcher::match_string(
    const ast::Content_Sequence& argument,
    std::u8string_view str,
    Context& context,
    Fail_Callback on_fail
)
{
    const auto it = std::ranges::lower_bound(m_options, str);

    if (it == m_options.end() || *it != str) {
        m_index = -1;
        std::pmr::u8string error { context.get_transient_memory() };
        error += u8'"';
        error += str;
        error += u8"\" does not match any of the valid options ("sv;
        bool first = true;
        for (const std::u8string_view o : m_options) {
            if (!first) {
                error += u8", "sv;
            }
            first = false;

            error += u8'"';
            error += o;
            error += u8'"';
        }
        error += u8").";
        on_fail(argument.get_source_span(), std::u8string_view(error), context);
        return false;
    }

    m_index = it - m_options.begin();
    return true;
}

Processing_Status Pack_Usual_Matcher::match_pack(
    std::span<const ast::Group_Member> members,
    Frame_Index frame,
    Context& context,
    const Match_Fail_Options& on_fail
)
{
    COWEL_DEBUG_ASSERT(status_is_error(on_fail.status));

    std::pmr::vector<int> indices(m_member_matchers.size(), -1, context.get_transient_memory());
    return do_match(members, frame, context, on_fail, indices, 0);
}

Processing_Status Empty_Pack_Matcher::match_pack(
    std::span<ast::Group_Member const> members,
    Frame_Index frame,
    Context& context,
    const Match_Fail_Options& on_fail
)
{
    COWEL_DEBUG_ASSERT(status_is_error(on_fail.status));

    Processing_Status result_status = Processing_Status::ok;
    for (const ast::Group_Member& arg : members) {
        if (arg.get_kind() == ast::Member_Kind::ellipsis) {
            const Stack_Frame& ellipsis_frame = context.get_call_stack()[frame];
            const auto recursive_status = match_pack(
                ellipsis_frame.invocation.get_arguments_span(),
                ellipsis_frame.invocation.content_frame, context, on_fail
            );
            if (recursive_status != Processing_Status::ok) {
                return recursive_status;
            }
        }
        else {
            on_fail.emit(
                arg.get_source_span(),
                u8"This argument does not match any parameter (no parameters are accepted)."sv,
                context
            );
            if (on_fail.status == Processing_Status::fatal) {
                return on_fail.status;
            }
            result_status = on_fail.status;
        }
    }
    return result_status;
}

Processing_Status Group_Matcher::match_value(
    const ast::Value& argument,
    Frame_Index frame,
    Context& context,
    const Match_Fail_Options& on_fail
)
{
    COWEL_DEBUG_ASSERT(status_is_error(on_fail.status));

    const auto* const group = std::get_if<ast::Group>(&argument);
    if (!group) {
        COWEL_ASSERT(std::holds_alternative<ast::Content_Sequence>(argument));
        on_fail.emit(argument.get_source_span(), u8"Expected a group, but got markup."sv, context);
        return on_fail.status;
    }
    return match_group(group, frame, context, on_fail);
}

Processing_Status Pack_Usual_Matcher::do_match(
    std::span<const ast::Group_Member> members,
    Frame_Index frame,
    Context& context,
    const Match_Fail_Options& on_fail,
    std::span<int> argument_indices_by_parameter,
    std::size_t cumulative_arg_index
)
{
    COWEL_DEBUG_ASSERT(status_is_error(on_fail.status));

    bool encountered_named = false;

    for (std::size_t arg_index = 0; arg_index < members.size(); ++arg_index) {
        const ast::Group_Member& member = members[arg_index];
        switch (member.get_kind()) {

        case ast::Member_Kind::positional: {
            if (encountered_named) {
                // TODO: more detailed feedback
                on_fail.emit(
                    member.get_source_span(),
                    u8"Providing a positional argument after a named argument is not valid."sv,
                    context
                );
                return on_fail.status;
            }
            argument_indices_by_parameter[arg_index + cumulative_arg_index] = int(arg_index);
            const ast::Value& value = member.get_value();
            const auto member_status = m_member_matchers[arg_index + cumulative_arg_index]
                                           ->get_value_matcher()
                                           .match_value(value, frame, context, on_fail);
            if (member_status != Processing_Status::ok) {
                return member_status;
            }
            continue;
        }

        case ast::Member_Kind::ellipsis: {
            const Stack_Frame& ellipsis_frame = context.get_call_stack()[frame];
            const auto recursive_status = do_match(
                ellipsis_frame.invocation.get_arguments_span(),
                ellipsis_frame.invocation.content_frame, context, on_fail,
                argument_indices_by_parameter, cumulative_arg_index + arg_index
            );
            if (recursive_status != Processing_Status::ok) {
                return recursive_status;
            }
            continue;
        }

        case ast::Member_Kind::named: {
            encountered_named = true;
            const std::u8string_view arg_name = member.get_name();
            for (std::size_t i = 0; i < m_member_matchers.size(); ++i) {
                auto* const member_matcher = m_member_matchers[i];
                if (arg_name != member_matcher->get_name()) {
                    continue;
                }
                if (argument_indices_by_parameter[i] != -1) {
                    on_fail.emit(
                        member.get_name_span(),
                        joined_char_sequence({
                            u8"The named argument \""sv,
                            arg_name,
                            u8"\" cannot be provided more than once.",
                        }),
                        context
                    );
                    return on_fail.status;
                }
                argument_indices_by_parameter[i] = int(arg_index);
                const auto member_status = member_matcher->get_value_matcher().match_value(
                    member.get_value(), frame, context, on_fail
                );
                if (member_status != Processing_Status::ok) {
                    return member_status;
                }
                goto named_parameter_matched;
            }
            on_fail.emit(
                member.get_name_span(),
                joined_char_sequence({
                    u8"The named argument \""sv,
                    arg_name,
                    u8"\" does not correspond to any parameter"sv,
                }),
                context
            );
            return on_fail.status;
        named_parameter_matched:
            continue;
        }
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid member kind.");
    }

    for (Group_Member_Matcher* const member_matcher : m_member_matchers) {
        if (member_matcher->is_mandatory() && !member_matcher->get_value_matcher().was_matched()) {
            on_fail.emit(
                on_fail.location,
                joined_char_sequence({
                    u8"No argument for parameter \"",
                    member_matcher->get_name(),
                    u8"\" was provided.",
                }),
                context
            );
            return on_fail.status;
        }
    }

    return Processing_Status::ok;
}

Processing_Status Group_Pack_Named_Lazy_Any_Matcher::match_pack(
    std::span<const ast::Group_Member> members,
    Frame_Index frame,
    Context& context,
    const Match_Fail_Options& on_fail
)
{
    for (const ast::Group_Member& member : members) {
        switch (member.get_kind()) {
        case ast::Member_Kind::named: {
            continue;
        }
        case ast::Member_Kind::ellipsis: {
            const Stack_Frame& ellipsis_frame = context.get_call_stack()[frame];
            const auto recursive_status = match_pack(
                ellipsis_frame.invocation.get_arguments_span(),
                ellipsis_frame.invocation.content_frame, context, on_fail
            );
            if (recursive_status != Processing_Status::ok) {
                return recursive_status;
            }
            continue;
        }
        case ast::Member_Kind::positional: {
            on_fail.emit(
                member.get_source_span(),
                u8"Only named arguments can be provided here, not positional arguments."sv, context
            );
            return on_fail.status;
        }
        }
    }
    return Processing_Status::ok;
}

[[nodiscard]]
Processing_Status detail::Group_Pack_Value_Matcher_Base::match_pack(
    std::span<const ast::Group_Member> members,
    Frame_Index frame,
    Context& context,
    const Match_Fail_Options& on_fail
)
{
    COWEL_DEBUG_ASSERT(status_is_error(on_fail.status));

    for (const ast::Group_Member& member : members) {
        switch (member.get_kind()) {

        case ast::Member_Kind::positional: {
            const auto member_status
                = match_value_in_pack(member.get_value(), frame, context, on_fail);
            if (member_status != Processing_Status::ok) {
                return member_status;
            }
            continue;
        }

        case ast::Member_Kind::named: {
            on_fail.emit(
                member.get_name_span(),
                u8"A pack of values was expected here. Named arguments cannot be provided."sv,
                context
            );
            return on_fail.status;
        }

        case ast::Member_Kind::ellipsis: {
            const Stack_Frame& ellipsis_frame = context.get_call_stack()[frame];
            const auto recursive_status = match_pack(
                ellipsis_frame.invocation.get_arguments_span(),
                ellipsis_frame.invocation.content_frame, context, on_fail
            );
            if (recursive_status != Processing_Status::ok) {
                return recursive_status;
            }
            continue;
        }
        }
    }
    return Processing_Status::ok;
}

template <std::derived_from<Value_Matcher> VM>
[[nodiscard]]
Processing_Status Group_Pack_Value_Matcher<VM>::match_value_in_pack(
    const ast::Value& value,
    Frame_Index frame,
    Context& context,
    const Match_Fail_Options& on_fail
)
{
    COWEL_DEBUG_ASSERT(status_is_error(on_fail.status));

    auto value_matcher = [&] -> VM {
        if constexpr (std::is_default_constructible_v<VM>) {
            return VM {};
        }
        else {
            return VM { m_values.get_allocator().resource() };
        }
    }();
    const auto result = value_matcher.match_value(value, frame, context, on_fail);
    if (result == Processing_Status::ok) {
        m_values.emplace_back(std::move(value_matcher.get()), value.get_source_span());
    }
    return result;
}

template struct Group_Pack_Value_Matcher<String_Matcher>;
template struct Group_Pack_Value_Matcher<Boolean_Matcher>;
template struct Group_Pack_Value_Matcher<Integer_Matcher>;

} // namespace cowel
