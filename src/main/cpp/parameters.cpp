#include <string_view>

#include "cowel/util/char_sequence_factory.hpp"

#include "cowel/context.hpp"
#include "cowel/parameters.hpp"
#include "cowel/util/small_vector.hpp"

namespace cowel {

Result<Value, Processing_Status> Argument::group_member_evaluate(
    const void* const ast_node, //
    const Frame_Index frame,
    Context& context
)
{
    return evaluate_expression(
        static_cast<const ast::Group_Member*>(ast_node)->get_value(), frame, context
    );
}
Processing_Status Argument::group_member_splice(
    const void* const ast_node, //
    Content_Policy& out,
    const Frame_Index frame,
    Context& context
)
{
    return splice_expression(
        out, static_cast<const ast::Group_Member*>(ast_node)->get_value(), frame, context
    );
}
Processing_Status Argument::group_member_splice_to_plaintext(
    const void* ast_node,
    std::pmr::vector<char8_t>& out,
    Frame_Index frame,
    Context& context
)
{
    return splice_expression_to_plaintext(
        out, static_cast<const ast::Group_Member*>(ast_node)->get_value(), frame, context
    );
}
const File_Source_Span& Argument::group_member_get_source_span(const void* const ast_node) noexcept
{
    return static_cast<const ast::Group_Member*>(ast_node)->get_source_span();
}
const Type&
Argument::group_member_get_static_type(const void* const ast_node, Context& context) noexcept
{
    return cowel::get_static_type(
        static_cast<const ast::Group_Member*>(ast_node)->get_value(), context
    );
}

Result<Value, Processing_Status> Argument::primary_evaluate(
    const void* const ast_node, //
    const Frame_Index frame,
    Context& context
)
{
    return cowel::evaluate(*static_cast<const ast::Primary*>(ast_node), frame, context);
}
Processing_Status Argument::primary_splice(
    const void* const ast_node, //
    Content_Policy& out,
    const Frame_Index frame,
    Context& context
)
{
    return cowel::splice_primary(out, *static_cast<const ast::Primary*>(ast_node), frame, context);
}
Processing_Status Argument::primary_splice_to_plaintext(
    const void* ast_node,
    std::pmr::vector<char8_t>& out,
    Frame_Index frame,
    Context& context
)
{
    return splice_expression_to_plaintext(
        out, *static_cast<const ast::Primary*>(ast_node), frame, context
    );
}
const File_Source_Span& Argument::primary_get_source_span(const void* ast_node) noexcept
{
    return static_cast<const ast::Primary*>(ast_node)->get_source_span();
}
const Type& Argument::primary_get_static_type(const void* const ast_node, Context&) noexcept
{
    return get_type(*static_cast<const ast::Primary*>(ast_node));
}

Processing_Status Lazy_Value_Of_Type_Matcher::match_value(
    const Argument& argument,
    Frame_Index frame,
    Context& context,
    const Match_Fail_Options& on_fail
)
{
    const Type& actual_type = argument.get_static_type(context);
    COWEL_DEBUG_ASSERT(actual_type.is_canonical());

    if (actual_type != Type::any && !actual_type.analytically_convertible_to(get_type())) {
        on_fail.emit(
            argument.get_value_location(),
            joined_char_sequence(
                {
                    u8"Expected a value of type "sv,
                    get_type().get_display_name(),
                    u8", but got "sv,
                    actual_type.get_display_name(),
                    u8".",
                }
            ),
            context
        );
        return on_fail.status;
    }
    m_value = { argument, argument.get_location() };
    m_markup_frame = frame;
    return Processing_Status::ok;
}

Processing_Status Value_Of_Type_Matcher::match_value(
    const Argument& argument,
    Frame_Index frame,
    Context& context,
    const Match_Fail_Options& on_fail
)
{
    Result<Value, Processing_Status> val = argument.evaluate(frame, context);
    if (!val) {
        return status_max(val.error(), on_fail.status);
    }
    if (!val->get_type().analytically_convertible_to(get_type())) {
        on_fail.emit(
            argument.get_value_location(),
            joined_char_sequence(
                {
                    u8"Expected a value of type "sv,
                    get_type().get_display_name(),
                    u8", but got "sv,
                    val->get_type().get_display_name(),
                    u8".",
                }
            ),
            context
        );
        return on_fail.status;
    }
    m_value = { std::move(val.value()), argument.get_value_location() };
    return Processing_Status::ok;
}

Processing_Status Textual_Matcher::match_value(
    const Argument& argument,
    Frame_Index frame,
    Context& context,
    const Match_Fail_Options& on_fail
)
{
    COWEL_DEBUG_ASSERT(status_is_error(on_fail.status));

    std::pmr::vector<char8_t> buffer { context.get_transient_memory() };
    const Processing_Status status = argument.splice_to_plaintext(buffer, frame, context);
    if (status != Processing_Status::ok) {
        return status;
    }
    return match_string(argument, as_u8string_view(buffer), context, on_fail.emit)
        ? Processing_Status::ok
        : on_fail.status;
}

Processing_Status Spliceable_To_String_Matcher::match_value(
    const Argument& argument, //
    Frame_Index frame, //
    Context& context, //
    const Match_Fail_Options& on_fail
)
{
    COWEL_DEBUG_ASSERT(status_is_error(on_fail.status));

    m_data.clear();
    const Processing_Status status = argument.splice_to_plaintext(m_data, frame, context);
    if (status != Processing_Status::ok) {
        m_data.clear();
        return status;
    }
    m_value = { as_u8string_view(m_data), argument.get_value_location() };
    return Processing_Status::ok;
}

Processing_Status String_Matcher::match_value(
    const Argument& argument, //
    Frame_Index frame, //
    Context& context, //
    const Match_Fail_Options& on_fail
)
{
    COWEL_DEBUG_ASSERT(status_is_error(on_fail.status));

    const Result<Value, Processing_Status> val = argument.evaluate(frame, context);
    if (!val) {
        return status_max(val.error(), on_fail.status);
    }
    if (!val->is_str()) {
        // TODO: improve diagnostic
        on_fail.emit(argument.get_value_location(), u8"Expected a string."sv, context);
        return on_fail.status;
    }
    append(m_data, val->as_string());

    m_value = { as_u8string_view(m_data), argument.get_value_location() };
    m_string_kind = val->get_string_kind();
    return Processing_Status::ok;
}

Processing_Status Boolean_Matcher::match_value(
    const Argument& argument,
    Frame_Index frame,
    Context& context,
    const Match_Fail_Options& on_fail
)
{
    const Result<Value, Processing_Status> val = argument.evaluate(frame, context);
    if (!val) {
        return status_max(val.error(), on_fail.status);
    }
    if (!val->is_bool()) {
        // TODO: improve diagnostic
        on_fail.emit(
            argument.get_value_location(), u8"Expected a boolean (true or false)."sv, context
        );
        return on_fail.status;
    }
    m_value = { val.value().as_boolean(), argument.get_value_location() };
    return Processing_Status::ok;
}

Processing_Status Integer_Matcher::match_value(
    const Argument& argument,
    Frame_Index frame,
    Context& context,
    const Match_Fail_Options& on_fail
)
{
    Result<Value, Processing_Status> val = argument.evaluate(frame, context);
    if (!val) {
        return status_max(val.error(), on_fail.status);
    }
    if (!val->is_int()) {
        // TODO: improve diagnostic
        on_fail.emit(argument.get_value_location(), u8"Expected an integer."sv, context);
        return on_fail.status;
    }
    m_value = { std::move(val.value().as_integer()), argument.get_value_location() };
    return Processing_Status::ok;
}

Processing_Status Float_Matcher::match_value(
    const Argument& argument,
    Frame_Index frame,
    Context& context,
    const Match_Fail_Options& on_fail
)
{
    const Result<Value, Processing_Status> val = argument.evaluate(frame, context);
    if (!val) {
        return status_max(val.error(), on_fail.status);
    }
    if (!val->is_float()) {
        // TODO: improve diagnostic
        on_fail.emit(argument.get_value_location(), u8"Expected a float."sv, context);
        return on_fail.status;
    }
    m_value = { val.value().as_float(), argument.get_value_location() };
    return Processing_Status::ok;
}

bool Sorted_Options_Matcher::match_string(
    const Argument& argument,
    std::u8string_view str,
    Context& context,
    Fail_Callback on_fail
)
{
    const auto it = std::ranges::lower_bound(m_options, str);

    if (it == m_options.end() || *it != str) {
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
        on_fail(argument.get_value_location(), std::u8string_view(error), context);
        return false;
    }

    m_value = Value_And_Location<std::size_t> {
        .value = std::size_t(it - m_options.begin()),
        .location = argument.get_value_location(),
    };
    return true;
}

Processing_Status Block_Matcher::match_value(
    const Argument& argument,
    Frame_Index frame,
    Context& context,
    const Match_Fail_Options& on_fail
)
{
    const Result<Value, Processing_Status> val = argument.evaluate(frame, context);
    if (!val) {
        return status_max(val.error(), on_fail.status);
    }
    if (!val->is_block()) {
        // TODO: improve diagnostic
        on_fail.emit(argument.get_value_location(), u8"Expected a block."sv, context);
        return on_fail.status;
    }
    m_value = { val.value(), argument.get_value_location() };
    return Processing_Status::ok;
}

Processing_Status Pack_Of_Type_Matcher::match_value(
    const Argument& argument,
    Frame_Index frame,
    Context& context,
    const Match_Fail_Options& on_fail
)
{
    Result<Value, Processing_Status> val = argument.evaluate(frame, context);
    if (!val) {
        return status_max(val.error(), on_fail.status);
    }
    COWEL_DEBUG_ASSERT(get_type().is_pack());
    const Type& element_type = get_type().get_members().front();
    if (!val->get_type().analytically_convertible_to(element_type)) {
        on_fail.emit(
            argument.get_value_location(),
            joined_char_sequence(
                {
                    u8"Expected an element of type "sv,
                    element_type.get_display_name(),
                    u8", but got "sv,
                    val->get_type().get_display_name(),
                    u8"."sv,
                }
            ),
            context
        );
        return on_fail.status;
    }
    m_values.push_back({ .value = std::move(*val), .location = argument.get_value_location() });
    return Processing_Status::ok;
}

Processing_Status Pack_Named_Of_Type_Matcher::match_value(
    const Argument& argument,
    Frame_Index frame,
    Context& context,
    const Match_Fail_Options& on_fail
)
{
    COWEL_DEBUG_ASSERT(get_type().is_pack());

    if (!argument.is_named()) {
        on_fail.emit(
            argument.get_location(), //
            u8"Expected a named argument, but got a positional one."sv, //
            context
        );
        return on_fail.status;
    }

    Result<Value, Processing_Status> val = argument.evaluate(frame, context);
    if (!val) {
        return status_max(val.error(), on_fail.status);
    }

    if (!val->get_type().analytically_convertible_to(m_element_type)) {
        on_fail.emit(
            argument.get_value_location(),
            joined_char_sequence(
                {
                    u8"Expected a named argument of type "sv,
                    m_element_type.get_display_name(),
                    u8", but got "sv,
                    val->get_type().get_display_name(),
                    u8"."sv,
                }
            ),
            context
        );
        return on_fail.status;
    }
    m_values.push_back(
        {
            .name = argument.get_name(),
            .value = std::move(*val),
        }
    );
    m_locations.push_back(argument.get_location());
    return Processing_Status::ok;
}

Processing_Status Group_Pack_Named_Str_Matcher::match_value(
    const Argument& argument,
    Frame_Index frame,
    Context& context,
    const Match_Fail_Options& on_fail
)
{
    Result<Value, Processing_Status> val = argument.evaluate(frame, context);
    if (!val) {
        return status_max(val.error(), on_fail.status);
    }
    if (!val->is_group()) {
        on_fail.emit(
            argument.get_value_location(),
            joined_char_sequence(
                {
                    u8"Expected a group, but got "sv,
                    val->get_type().get_display_name(),
                    u8"."sv,
                }
            ),
            context
        );
        return on_fail.status;
    }

    for (const auto& [name, value] : val->get_group_members()) {
        if (!name.is_str()) {
            COWEL_ASSERT(name.is_null());
            on_fail.emit(
                argument.get_value_location(),
                u8"Expected a named member, but got a positional one."sv, context
            );
            return on_fail.status;
        }
        if (!value.is_str()) {
            on_fail.emit(
                argument.get_value_location(),
                joined_char_sequence(
                    {
                        u8"Group members must be of type "sv,
                        Type::str.get_display_name(),
                        u8", but got "sv,
                        value.get_type().get_display_name(),
                        u8"."sv,
                    }
                ),
                context
            );
            return on_fail.status;
        }
    }

    m_value = { std::move(*val), argument.get_value_location() };
    return Processing_Status::ok;
}

namespace {

enum struct Argument_Mode : Default_Underlying {
    /// @brief Initial mode.
    normal,
    /// @brief Named-only mode.
    /// After the first named argument, only more named arguments are accepted.
    named_only,
    /// @brief Variadic mode.
    /// After matching an argument to a pack parameter,
    /// all subsequent arguments are treated as part of that pack.
    /// Named arguments are accepted and terminate the pack.
    pack_positional,
    /// @brief Variadic mode.
    /// Pack of named arguments.
    pack_named,
};

struct Match_Call {
    const std::span<Parameter* const> parameters;
    Context& context;
    const Fail_Callback on_fail;
    const Processing_Status on_fail_status;
    const std::span<int> argument_indices_by_parameter;
    Argument_Mode mode;
    std::size_t current_pack_param_index;
    Value_Matcher* current_pack_value_matcher;

    [[nodiscard]]
    Processing_Status operator()(
        std::span<const ast::Group_Member> args,
        Frame_Index frame,
        std::size_t cumulative_arg_index
    );
};

Processing_Status Match_Call::operator()(
    const std::span<const ast::Group_Member> args,
    const Frame_Index frame,
    const std::size_t cumulative_arg_index
)
{
    for (std::size_t arg_index = 0; arg_index < args.size(); ++arg_index) {
        const ast::Group_Member& member = args[arg_index];
        const Match_Fail_Options on_member_fail {
            .emit = on_fail,
            .status = on_fail_status,
            .location = member.get_source_span(),
        };

        switch (member.get_kind()) {
        case ast::Member_Kind::positional: {
            const auto arg = Argument::positional(member);

            switch (mode) {
            case Argument_Mode::named_only:
            case Argument_Mode::pack_named: {
                // TODO: more detailed feedback
                on_fail(
                    member.get_source_span(),
                    u8"Providing a positional argument after a named argument is not valid."sv,
                    context
                );
                return on_fail_status;
            }

            case Argument_Mode::pack_positional: {
                // We shouldn't be in pack mode
                // unless we have matched a pack parameter once in normal mode.
                COWEL_ASSERT(current_pack_value_matcher);
                COWEL_ASSERT(current_pack_param_index <= argument_indices_by_parameter.size());
                COWEL_ASSERT(argument_indices_by_parameter[current_pack_param_index] != -1);

                const auto arg_status
                    = current_pack_value_matcher->match_value(arg, frame, context, on_member_fail);
                if (arg_status != Processing_Status::ok) {
                    return arg_status;
                }
                continue;
            }

            case Argument_Mode::normal: {
                const std::size_t parameter_index = arg_index + cumulative_arg_index;
                if (parameter_index >= argument_indices_by_parameter.size()) {
                    on_fail(member.get_source_span(), u8"Too many arguments."sv, context);
                    return on_fail_status;
                }
                if (argument_indices_by_parameter[parameter_index]
                    == std::numeric_limits<int>::max()) {
                    on_fail(
                        member.get_source_span(),
                        joined_char_sequence(
                            {
                                u8"This argument is invalid because the parameter \""sv,
                                parameters[parameter_index]->get_name(),
                                u8"\" has already been provided as a block argument.",
                            }
                        ),
                        context
                    );
                    return on_fail_status;
                }
                // No other failure is possible:
                // named arguments can only be provided after positional arguments,
                // and positional arguments wouldn't match a parameter more than once.
                COWEL_ASSERT(argument_indices_by_parameter[parameter_index] == -1);

                // FIXME: pretty sure this should be = parameter_index
                argument_indices_by_parameter[parameter_index] = int(arg_index);
                Value_Matcher& value_matcher = parameters[parameter_index]->get_value_matcher();
                const auto arg_status
                    = value_matcher.match_value(arg, frame, context, on_member_fail);
                if (arg_status != Processing_Status::ok) {
                    return arg_status;
                }
                if (value_matcher.get_type().is_pack()) {
                    mode = Argument_Mode::pack_positional;
                    current_pack_param_index = parameter_index;
                    current_pack_value_matcher = &value_matcher;
                }
                continue;
            }
            }
            COWEL_ASSERT_UNREACHABLE(u8"Invalid mode.");
        }

        case ast::Member_Kind::ellipsis: {
            const Stack_Frame& ellipsis_frame = context.get_call_stack()[frame];
            const auto recursive_status = (*this)(
                ellipsis_frame.invocation.get_arguments_span(),
                ellipsis_frame.invocation.content_frame, cumulative_arg_index + arg_index
            );
            if (recursive_status != Processing_Status::ok) {
                return recursive_status;
            }
            continue;
        }

        case ast::Member_Kind::named: {
            Result<Value, Processing_Status> name_result
                = evaluate(member.get_name(), frame, context);
            if (!name_result) {
                return status_max(on_fail_status, name_result.error());
            }
            COWEL_ASSERT(name_result->is_str());

            const auto arg = Argument::named(std::move(*name_result), member);
            const std::u8string_view arg_name = arg.get_name().as_string();

            // Handle pack of named arguments, i.e. parameters of type "pack named any".
            // This should only be possible in normal mode because otherwise,
            // there is an ambiguity between providing a named argument that goes into the pack
            // and one that applies to a prior positional parameter.
            if (mode == Argument_Mode::normal) {
                const std::size_t parameter_index = arg_index + cumulative_arg_index;
                if (parameter_index < argument_indices_by_parameter.size()) {
                    auto* const parameter = parameters[parameter_index];
                    const bool is_pack_named = parameter->get_type().is_pack()
                        && parameter->get_type().get_members().front().is_named();
                    if (is_pack_named) {
                        argument_indices_by_parameter[parameter_index] = int(parameter_index);
                        current_pack_value_matcher = &parameter->get_value_matcher();
                        current_pack_param_index = parameter_index;
                        mode = Argument_Mode::pack_named;
                        const auto arg_status = parameter->get_value_matcher().match_value(
                            arg, frame, context, on_member_fail
                        );
                        if (arg_status != Processing_Status::ok) {
                            return arg_status;
                        }
                        goto named_parameter_matched;
                    }
                }
            }
            // If we are already in a pack of named arguments and provide another named argument,
            // we simply append it to the pack.
            else if (mode == Argument_Mode::pack_named) {
                COWEL_ASSERT(current_pack_value_matcher);
                COWEL_ASSERT(current_pack_param_index <= argument_indices_by_parameter.size());
                COWEL_ASSERT(argument_indices_by_parameter[current_pack_param_index] != -1);

                const auto arg_status
                    = current_pack_value_matcher->match_value(arg, frame, context, on_member_fail);
                if (arg_status != Processing_Status::ok) {
                    return arg_status;
                }
                goto named_parameter_matched;
            }

            COWEL_DEBUG_ASSERT(mode != Argument_Mode::pack_named);
            mode = Argument_Mode::named_only;
            // Otherwise, in the normal case, for the named argument,
            // we need to find a parameter with the same name.
            // Providing the same named argument twice or providing a named argument
            // without a corresponding parameter need to be diagnosed.
            for (std::size_t i = 0; i < parameters.size(); ++i) {
                auto* const member_matcher = parameters[i];
                if (arg_name != member_matcher->get_name()) {
                    continue;
                }
                if (argument_indices_by_parameter[i] != -1) {
                    on_fail(
                        member.get_name_span(),
                        joined_char_sequence(
                            {
                                u8"The named argument \""sv,
                                arg_name,
                                u8"\" cannot be provided more than once.",
                            }
                        ),
                        context
                    );
                    return on_fail_status;
                }
                // FIXME: Pretty sure this should be = parameter_index
                argument_indices_by_parameter[i] = int(arg_index);
                const auto arg_status = member_matcher->get_value_matcher().match_value(
                    arg, frame, context, on_member_fail
                );
                if (arg_status != Processing_Status::ok) {
                    return arg_status;
                }
                goto named_parameter_matched;
            }
            on_fail(
                member.get_name_span(),
                joined_char_sequence(
                    {
                        u8"The named argument \""sv,
                        arg_name,
                        u8"\" does not correspond to any parameter"sv,
                    }
                ),
                context
            );
            return on_fail_status;

        named_parameter_matched:
            continue;
        }
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid member kind.");
    }

    return Processing_Status::ok;
}

} // namespace

Processing_Status match_call(
    const std::span<Parameter* const> parameters,
    const Invocation& call,
    Context& context,
    Fail_Callback on_fail,
    Processing_Status on_fail_status
)
{
    COWEL_ASSERT(on_fail);
    COWEL_ASSERT(status_is_error(on_fail_status));
    if constexpr (is_debug_build) {
        for (const auto* const matcher : parameters) {
            COWEL_ASSERT(matcher != nullptr);
        }
    }

    Small_Vector<int, 32> argument_indices_by_parameter(parameters.size(), -1);

    if (call.content) {
        if (parameters.empty()) {
            on_fail(
                call.content->get_source_span(), //
                u8"Block argument does not match any parameter."sv, //
                context
            );
            return Processing_Status::error;
        }
        const auto arg = Argument::block(*call.content);
        const Match_Fail_Options fail_options {
            .emit = on_fail,
            .status = on_fail_status,
            .location = call.content->get_source_span(),
        };
        const Processing_Status block_status = parameters.back()->get_value_matcher().match_value(
            arg, call.content_frame, context, fail_options
        );
        if (block_status != Processing_Status::ok) {
            return block_status;
        }
        argument_indices_by_parameter.back() = std::numeric_limits<int>::max();
    }

    Match_Call match_call {
        .parameters = parameters,
        .context = context,
        .on_fail = on_fail,
        .on_fail_status = on_fail_status,
        .argument_indices_by_parameter = argument_indices_by_parameter,
        .mode = Argument_Mode::normal,
        .current_pack_param_index = -1uz,
        .current_pack_value_matcher = nullptr,
    };
    const Processing_Status status = match_call(
        call.get_arguments_span(), //
        call.content_frame, //
        /* cumulative_arg_index= */ 0
    );
    if (status != Processing_Status::ok) {
        return status;
    }

    for (Parameter* const p : parameters) {
        if (p->is_mandatory() && !p->get_value_matcher().was_matched()) {
            on_fail(
                call.directive.get_name_span(),
                joined_char_sequence(
                    {
                        u8"No argument for parameter \"",
                        p->get_name(),
                        u8"\" was provided.",
                    }
                ),
                context
            );
            return on_fail_status;
        }
    }

    return Processing_Status::ok;
}

} // namespace cowel
