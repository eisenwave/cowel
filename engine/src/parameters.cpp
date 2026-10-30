#include <string_view>

#include "cowel/util/char_sequence_factory.hpp"

#include "cowel/context.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/parameters.hpp"
#include "cowel/util/small_vector.hpp"

namespace cowel {

Result<Value, Processing_Status> Argument::evaluate(const Frame_Index frame, Context& context) const
{
    if (m_runtime_value) {
        return *m_runtime_value;
    }
    return m_evaluate(m_ast_node, frame, context);
}

Processing_Status
Argument::splice(Content_Policy& out, const Frame_Index frame, Context& context) const
{
    if (m_runtime_value) {
        return splice_value(out, *m_runtime_value, m_location, context);
    }
    return m_splice(m_ast_node, out, frame, context);
}

Processing_Status Argument::splice_to_plaintext(
    std::pmr::vector<char8_t>& out,
    const Frame_Index frame,
    Context& context
) const
{
    if (m_runtime_value) {
        return splice_value_to_plaintext(out, *m_runtime_value, m_location, context);
    }
    return m_splice_to_plaintext(m_ast_node, out, frame, context);
}

[[nodiscard]]
const File_Source_Span& Argument::get_value_location() const
{
    if (m_runtime_value) {
        return m_location;
    }
    return m_get_value_location(m_ast_node);
}

const Type& Argument::get_static_type(Context& context) const
{
    if (m_runtime_value) {
        return m_runtime_value->get_type();
    }
    return m_get_static_type(m_ast_node, context);
}

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

    if (actual_type != Type::any && !actual_type.instance_of(get_matchable_type())) {
        on_fail.emit(
            argument.get_value_location(),
            joined_char_sequence(
                {
                    u8"Expected a value of type "sv,
                    get_matchable_type().get_display_name(),
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
    if (!val->get_type().instance_of(get_matchable_type())) {
        on_fail.emit(
            argument.get_value_location(),
            joined_char_sequence(
                {
                    u8"Expected a value of type "sv,
                    get_matchable_type().get_display_name(),
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

Processing_Status Declared_Matcher::match_value(
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
    if (m_is_pack) {
        m_values.push_back({ .value = std::move(*val), .location = argument.get_value_location() });
    }
    else {
        m_value = { .value = std::move(*val), .location = argument.get_value_location() };
    }
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
    COWEL_DEBUG_ASSERT(get_matchable_type().is_pack());
    const Type& element_type = get_matchable_type().get_members().front();
    if (!val->get_type().instance_of(element_type)) {
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
    COWEL_DEBUG_ASSERT(get_matchable_type().is_pack());

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

    if (!val->get_type().instance_of(m_element_type)) {
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

enum Parameter_Argument_Index : int {
    parameter_not_provided = -1,
    parameter_provided_as_block = std::numeric_limits<int>::max(),
};

struct Expand_Call_Arguments {
    Small_Vector<Argument, 32> out_arguments {};
    Small_Vector<Frame_Index, 32> out_frames {};
    const Fail_Callback on_fail;
    const Processing_Status on_fail_status;
    Context& context;

    [[nodiscard]]
    Processing_Status operator()(std::span<const ast::Group_Member> args, Frame_Index frame);
};

[[nodiscard]]
Processing_Status Expand_Call_Arguments::operator()(
    const std::span<const ast::Group_Member> args,
    const Frame_Index frame
)
{
    return process_greedy(args, [&](const ast::Group_Member& argument) -> Processing_Status {
        switch (argument.get_kind()) {
        case ast::Member_Kind::positional: {
            out_arguments.push_back(Argument::positional(argument));
            out_frames.push_back(frame);
            return Processing_Status::ok;
        }

        case ast::Member_Kind::named: {
            Result<Value, Processing_Status> name_result
                = evaluate(argument.get_name(), frame, context);
            if (!name_result) {
                return status_max(on_fail_status, name_result.error());
            }
            COWEL_ASSERT(name_result->is_str());
            out_arguments.push_back(Argument::named(std::move(*name_result), argument));
            out_frames.push_back(frame);
            return Processing_Status::ok;
        }

        case ast::Member_Kind::expansion: {
            const Result<Value, Processing_Status> expanded_group_value
                = evaluate_expression(argument.get_value(), frame, context);
            if (!expanded_group_value) {
                return status_max(expanded_group_value.error(), on_fail_status);
            }
            if (!expanded_group_value->is_group()) {
                on_fail(
                    argument.get_source_span(),
                    joined_char_sequence(
                        {
                            u8"Expansion requires a group, but got "sv,
                            expanded_group_value->get_type().get_display_name(),
                            u8"."sv,
                        }
                    ),
                    context
                );
                return on_fail_status;
            }
            for (const Group_Member_Value& expanded : expanded_group_value->get_group_members()) {
                out_arguments.push_back(
                    expanded.name.is_null()
                        ? Argument::positional(expanded.value, argument.get_source_span())
                        : Argument::named(expanded.name, expanded.value, argument.get_source_span())
                );
                out_frames.push_back(frame);
            }
            return Processing_Status::ok;
        }

        case ast::Member_Kind::ellipsis: {
            const Stack_Frame& ellipsis_frame = context.get_call_stack()[frame];
            return (*this)(
                ellipsis_frame.invocation.get_arguments_span(),
                ellipsis_frame.invocation.content_frame
            );
        }
        }

        COWEL_ASSERT_UNREACHABLE(u8"Invalid member kind.");
    });
}

struct Match_Arguments_Options {
    std::span<Parameter* const> parameters;
    std::span<const Argument> arguments;
    std::span<const Frame_Index> argument_frames;
    Context& context;
    Fail_Callback on_fail;
    Processing_Status on_fail_status;
    std::span<int> argument_indices_by_parameter;
};

Processing_Status match_argument(
    const Match_Arguments_Options& options,
    const Argument& argument,
    const Frame_Index frame,
    const std::size_t argument_index,
    Argument_Mode& mode,
    std::size_t& current_pack_param_index,
    Value_Matcher*& current_pack_value_matcher
)
{
    const Match_Fail_Options on_member_fail {
        .emit = options.on_fail,
        .status = options.on_fail_status,
        .location = argument.get_location(),
    };
    if (argument.is_positional()) {
        switch (mode) {
        case Argument_Mode::named_only:
        case Argument_Mode::pack_named: {
            options.on_fail(
                argument.get_location(),
                u8"Providing a positional argument after a named argument is not valid."sv,
                options.context
            );
            return options.on_fail_status;
        }

        case Argument_Mode::pack_positional: {
            COWEL_ASSERT(current_pack_value_matcher);
            COWEL_ASSERT(current_pack_param_index <= options.argument_indices_by_parameter.size());
            COWEL_ASSERT(
                options.argument_indices_by_parameter[current_pack_param_index]
                != parameter_not_provided
            );

            return current_pack_value_matcher->match_value(
                argument, frame, options.context, on_member_fail
            );
        }

        case Argument_Mode::normal: {
            const std::size_t parameter_index = argument_index;
            if (parameter_index >= options.argument_indices_by_parameter.size()) {
                options.on_fail(
                    argument.get_location(), u8"Too many arguments."sv, options.context
                );
                return options.on_fail_status;
            }
            if (options.argument_indices_by_parameter[parameter_index]
                == parameter_provided_as_block) {
                options.on_fail(
                    argument.get_location(),
                    joined_char_sequence(
                        {
                            u8"This argument is invalid because the parameter \""sv,
                            options.parameters[parameter_index]->get_name(),
                            u8"\" has already been provided as a block argument.",
                        }
                    ),
                    options.context
                );
                return options.on_fail_status;
            }
            COWEL_ASSERT(
                options.argument_indices_by_parameter[parameter_index] == parameter_not_provided
            );

            options.argument_indices_by_parameter[parameter_index] = int(argument_index);
            Value_Matcher& value_matcher = options.parameters[parameter_index]->get_value_matcher();
            const auto arg_status
                = value_matcher.match_value(argument, frame, options.context, on_member_fail);
            if (arg_status != Processing_Status::ok) {
                return arg_status;
            }
            if (value_matcher.get_matchable_type().is_pack()) {
                mode = Argument_Mode::pack_positional;
                current_pack_param_index = parameter_index;
                current_pack_value_matcher = &value_matcher;
            }
            return Processing_Status::ok;
        }
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid mode.");
    }

    COWEL_DEBUG_ASSERT(argument.is_named());
    const std::u8string_view arg_name = argument.get_name().as_string();
    if (mode == Argument_Mode::pack_named) {
        COWEL_ASSERT(current_pack_value_matcher);
        COWEL_ASSERT(current_pack_param_index <= options.argument_indices_by_parameter.size());
        COWEL_ASSERT(
            options.argument_indices_by_parameter[current_pack_param_index]
            != parameter_not_provided
        );

        return current_pack_value_matcher->match_value(
            argument, frame, options.context, on_member_fail
        );
    }

    if (mode == Argument_Mode::normal) {
        const std::size_t parameter_index = argument_index;
        if (parameter_index < options.argument_indices_by_parameter.size()) {
            auto* const parameter = options.parameters[parameter_index];
            const bool is_pack_named = parameter->get_type().is_pack()
                && parameter->get_type().get_members().front().is_named();
            if (is_pack_named) {
                options.argument_indices_by_parameter[parameter_index] = int(argument_index);
                current_pack_value_matcher = &parameter->get_value_matcher();
                current_pack_param_index = parameter_index;
                mode = Argument_Mode::pack_named;
                const auto arg_status = parameter->get_value_matcher().match_value(
                    argument, frame, options.context, on_member_fail
                );
                if (arg_status != Processing_Status::ok) {
                    return arg_status;
                }
                return Processing_Status::ok;
            }
        }
    }

    COWEL_DEBUG_ASSERT(mode != Argument_Mode::pack_named);
    mode = Argument_Mode::named_only;
    for (std::size_t i = 0; i < options.parameters.size(); ++i) {
        auto* const member_matcher = options.parameters[i];
        if (arg_name != member_matcher->get_name()) {
            continue;
        }
        if (options.argument_indices_by_parameter[i] != parameter_not_provided) {
            options.on_fail(
                argument.get_location(),
                joined_char_sequence(
                    {
                        u8"The named argument \""sv,
                        arg_name,
                        u8"\" cannot be provided more than once.",
                    }
                ),
                options.context
            );
            return options.on_fail_status;
        }
        options.argument_indices_by_parameter[i] = int(argument_index);
        const auto arg_status = member_matcher->get_value_matcher().match_value(
            argument, frame, options.context, on_member_fail
        );
        if (arg_status != Processing_Status::ok) {
            return arg_status;
        }
        return Processing_Status::ok;
    }
    options.on_fail(
        argument.get_location(),
        joined_char_sequence(
            {
                u8"The named argument \""sv,
                arg_name,
                u8"\" does not correspond to any parameter"sv,
            }
        ),
        options.context
    );
    return options.on_fail_status;
}

[[nodiscard]]
Processing_Status match_arguments(const Match_Arguments_Options& options)
{
    Argument_Mode mode = Argument_Mode::normal;
    std::size_t current_pack_param_index = -1uz;
    Value_Matcher* current_pack_value_matcher = nullptr;
    std::size_t arg_index = 0;
    return process_greedy(options.arguments, [&](const Argument& arg) {
        const auto status = match_argument(
            options, arg, options.argument_frames[arg_index], arg_index, //
            mode, current_pack_param_index, current_pack_value_matcher
        );
        ++arg_index;
        return status;
    });
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
    COWEL_DEBUG_ASSERT(!std::ranges::contains(parameters, nullptr));

    Expand_Call_Arguments expand_args {
        .on_fail = on_fail,
        .on_fail_status = on_fail_status,
        .context = context,
    };
    const auto expansion_status = expand_args(call.get_arguments_span(), call.content_frame);
    if (expansion_status != Processing_Status::ok) {
        return status_max(expansion_status, on_fail_status);
    }

    const bool last_parameter_is_pack
        = !parameters.empty() && parameters.back()->get_type().is_pack();
    if (!last_parameter_is_pack && call.block_arguments.size() > parameters.size()) {
        on_fail(
            call.block_arguments.front().get_source_span(),
            u8"Block argument does not match any parameter."sv, context
        );
        return on_fail_status;
    }

    Small_Vector<int, 32> argument_indices_by_parameter(parameters.size(), parameter_not_provided);
    const std::size_t block_parameter_offset = parameters.size() - call.block_arguments.size();
    for (std::size_t block_index = 0; block_index < call.block_arguments.size(); ++block_index) {
        const std::size_t parameter_index
            = last_parameter_is_pack ? parameters.size() - 1 : block_parameter_offset + block_index;
        const ast::Primary& block = call.block_arguments[block_index];
        if (parameters.empty()) {
            on_fail(
                block.get_source_span(), //
                u8"Block argument does not match any parameter."sv, //
                context
            );
            return on_fail_status;
        }
        const auto arg = Argument::block(block);
        const Match_Fail_Options fail_options {
            .emit = on_fail,
            .status = on_fail_status,
            .location = block.get_source_span(),
        };
        const Processing_Status block_status
            = parameters[parameter_index]->get_value_matcher().match_value(
                arg, call.content_frame, context, fail_options
            );
        if (block_status != Processing_Status::ok) {
            return block_status;
        }
        argument_indices_by_parameter[parameter_index] = parameter_provided_as_block;
    }

    const auto matching_status = match_arguments(
        {
            .parameters = parameters,
            .arguments = expand_args.out_arguments,
            .argument_frames = expand_args.out_frames,
            .context = context,
            .on_fail = on_fail,
            .on_fail_status = on_fail_status,
            .argument_indices_by_parameter = argument_indices_by_parameter,
        }
    );
    // The following check for non-provided parameters will produce noise
    // when matching parameters failed due to prior errors
    // (because lots will be non-provided due to the errors),
    // so we should abort here instead of greedily trying to produce more errors.
    if (matching_status != Processing_Status::ok) {
        return status_concat(expansion_status, matching_status);
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
