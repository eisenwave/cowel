#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cowel/autofix.hpp"
#include "cowel/parameters.hpp"
#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/from_chars.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/to_chars.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"

#include "cowel/syntax/ast.hpp"

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
    Result<std::optional<Argument>, Processing_Status> find(
        const std::span<const ast::Group_Member> members,
        Frame_Ref frame // NOLINT(performance-unnecessary-value-param)
    ) const
    {
        for (const ast::Group_Member& arg : members) {
            switch (arg.get_kind()) {
            case ast::Member_Kind::positional: {
                continue;
            }
            case ast::Member_Kind::expansion: {
                Result<Value, Processing_Status> value
                    = evaluate_expression(arg.get_value(), frame, context);
                if (!value) {
                    return value.error();
                }
                if (!value->is_group()) {
                    context.try_error_f(
                        diagnostic::type_mismatch, arg.get_source_span(),
                        u8"Expansion requires a group, but got {}."sv,
                        value->get_type().get_display_name()
                    );
                    return Processing_Status::error;
                }
                for (const Group_Member_Value& expanded : value->get_group_members()) {
                    if (expanded.name.is_str() && expanded.name.as_string() == needle_name) {
                        return std::optional<Argument>(
                            Argument::named(expanded.name, expanded.value, arg.get_source_span())
                        );
                    }
                }
                continue;
            }
            case ast::Member_Kind::ellipsis: {
                const Invocation* const ellipsis_invocation
                    = resolve_ellipsis_invocation(arg, frame.get(), context);
                if (!ellipsis_invocation) {
                    return Processing_Status::error;
                }
                auto maybe_result = find(
                    ellipsis_invocation->get_arguments_span(), //
                    ellipsis_invocation->content_frame
                );
                if (!maybe_result || *maybe_result) {
                    return maybe_result;
                }
                continue;
            }
            case ast::Member_Kind::named: {
                // TODO: This implies that each use of cowel_put evaluates every member name,
                //       which is probably not desirable.
                //       However, cowel_macro and cowel_put are kinda on the chopping block,
                //       so it's not worth solving this right now.
                Result<Value, Processing_Status> name_value
                    = evaluate(arg.get_name(), frame, context);
                if (!name_value) {
                    return name_value.error();
                }
                if (name_value->as_string() == needle_name) {
                    return std::optional<Argument>(Argument::named(std::move(*name_value), arg));
                }
                continue;
            }
            }
        }
        return std::optional<Argument> {};
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
    Result<std::optional<Argument>, Processing_Status> find(
        std::span<const ast::Group_Member> members,
        Frame_Ref frame // NOLINT(performance-unnecessary-value-param)
    )
    {
        for (const ast::Group_Member& arg : members) {
            switch (arg.get_kind()) {
            case ast::Member_Kind::named: {
                continue;
            }
            case ast::Member_Kind::ellipsis: {
                const Invocation* const ellipsis_invocation
                    = resolve_ellipsis_invocation(arg, frame.get(), context);
                if (!ellipsis_invocation) {
                    return Processing_Status::error;
                }
                auto maybe_result = find(
                    ellipsis_invocation->get_arguments_span(), //
                    ellipsis_invocation->content_frame
                );
                if (!maybe_result || *maybe_result) {
                    return maybe_result;
                }
                continue;
            }
            case ast::Member_Kind::expansion: {
                Result<Value, Processing_Status> value
                    = evaluate_expression(arg.get_value(), frame, context);
                if (!value) {
                    return value.error();
                }
                if (!value->is_group()) {
                    context.try_error(
                        diagnostic::type_mismatch, arg.get_source_span(),
                        joined_char_sequence(
                            {
                                u8"Expansion requires a group, but got "sv,
                                value->get_type().get_display_name(),
                                u8"."sv,
                            }
                        )
                    );
                    return Processing_Status::error;
                }
                for (const Group_Member_Value& expanded : value->get_group_members()) {
                    if (expanded.name.is_null() && needle_index == index++) {
                        return std::optional<Argument>(
                            Argument::positional(expanded.value, arg.get_source_span())
                        );
                    }
                }
                continue;
            }
            case ast::Member_Kind::positional: {
                if (needle_index == index++) {
                    return std::optional<Argument>(Argument::positional(arg));
                }
                continue;
            }
            }
        }
        return std::optional<Argument> {};
    }
};

/// @brief The argument of a macro invocation which is expanded by a `\cowel_put` call.
struct Put_Target {
    enum struct Kind {
        /// The block argument at index 0, which is the contents of the invocation.
        contents,
        /// The positional argument at index @ref index.
        positional,
        /// The named argument called @ref name.
        named,
    };

    Kind kind = Kind::contents;
    std::size_t index = 0;
    std::u8string_view name {};
};

/// @brief The syntactic position in which a `\cowel_put` call occurs,
/// which determines how it is replaced with a reference to a parameter.
enum struct Put_Position {
    /// The call is spliced,
    /// so it is replaced with `\(parameter)`,
    /// which splices the value of the parameter into the surrounding markup or string.
    splice,
    /// The call occurs in an expression,
    /// so it is replaced with the name of the parameter,
    /// which refers to its value.
    expression,
};

/// @brief The argument which a `\cowel_put` call expands,
/// and the value used when that argument is absent.
struct Put_Resolution {
    Put_Target target;
    /// @brief The source code of the `else` argument,
    /// or empty when the call has no `else` argument.
    std::u8string_view default_source {};
};

/// @brief Produces the function-expression
/// which replaces a deprecated `\cowel_macro` definition.
///
/// A macro definition such as `\cowel_macro("hl"){BODY}`
/// is roughly equivalent to the function `\(fun hl(...) = {BODY'})`,
/// where `BODY'` is the body of the macro,
/// except that macros access their arguments through `\cowel_put` calls.
///
/// This class rewrites such a body so that
/// - every `\cowel_put` call becomes a reference to a parameter, and
/// - every ellipsis argument becomes a reference to a pack parameter,
///   which forwards the arguments of the invocation.
///
/// The parameters of the function are derived from the `\cowel_put` calls:
/// - `content` is the contents of the macro, which are its block argument,
/// - `argN` is the positional argument at index `N`,
/// - for any other argument, the parameter has the same name as the argument.
///
/// Not every definition can be converted.
/// For example, a function-expression cannot expand the second block argument of an invocation,
/// so a definition which does is left alone.
/// In that case @ref build returns an empty optional, and no fix is produced.
struct Macro_Fixer {
    Context& context;
    /// @brief The `\cowel_macro` invocation which defines the macro.
    const Invocation& definition;

    /// @brief Edits which rewrite the body of the definition into the body of a function.
    /// The spans of the edits are in the coordinate system of the enclosing file.
    std::pmr::vector<Source_Edit> body_edits { context.get_persistent_memory() };

    /// @brief A parameter which captures the positional argument at one index.
    struct Positional_Parameter {
        std::pmr::u8string name {};
        /// @brief Whether the argument is used by the body at all.
        bool used = false;
        /// @brief The default argument, or empty when there is no default argument.
        std::u8string_view default_argument {};
    };
    /// @brief The generated parameter for the positional argument at each index.
    std::pmr::vector<Positional_Parameter> positional_parameters { context.get_persistent_memory(
    ) };

    /// @brief A parameter which captures a named argument of the macro.
    struct Named_Parameter {
        std::u8string_view name {};
        /// @brief The default argument, or empty when there is no default argument.
        std::u8string_view default_argument {};
    };
    std::pmr::vector<Named_Parameter> named_parameters { context.get_persistent_memory() };

    /// @brief Whether the body expands the contents of the macro through `\cowel_put`.
    bool uses_contents = false;
    /// @brief The default argument for the contents, which is empty when there is none.
    std::u8string_view contents_default {};

    /// @brief Whether the body expands arguments through an ellipsis argument,
    /// which is replaced with a single pack parameter.
    bool uses_pack = false;

    /// @brief Whether the definition cannot be converted, so no fix should be produced.
    bool bailout = false;

    /// @brief Builds the source code which replaces the macro definition.
    /// @param name The name which the macro is defined with.
    /// @return The replacement source, or an empty optional if the definition cannot be converted.
    [[nodiscard]]
    std::optional<std::pmr::u8string> build(const std::u8string_view name)
    {
        COWEL_ASSERT(!definition.block_arguments.empty());
        const ast::Primary& body = definition.block_arguments.front();
        walk_elements(body.get_elements());
        if (bailout) {
            return std::optional<std::pmr::u8string> {};
        }

        std::pmr::memory_resource* const memory = context.get_persistent_memory();
        // The parenthesized form of the function expression
        // is used because it can be followed by a line splice,
        // unlike the `\ fun ...` form,
        // which consumes the rest of the line as its expression.
        std::pmr::u8string result { u8"\\(fun ", memory };
        result.append(name);
        result.append(u8"(");
        for (const Positional_Parameter& parameter : positional_parameters) {
            // A positional parameter which is not used in the body is optional,
            // so that the function can be called with fewer arguments.
            const std::u8string_view default_argument
                = parameter.used ? parameter.default_argument : std::u8string_view { u8"{}" };
            declare_parameter(result, parameter.name, default_argument);
        }
        for (const Named_Parameter& parameter : named_parameters) {
            declare_parameter(result, parameter.name, parameter.default_argument);
        }
        if (uses_pack) {
            declare_parameter(result, u8"...args", {});
        }
        if (uses_contents) {
            declare_parameter(result, u8"content", contents_default);
        }
        result.append(u8") = ");
        result.append(apply_source_edits(
            body.get_source(), body.get_source_span().begin, body.get_source_span().file,
            body_edits, memory
        ));
        result.append(u8")");
        return std::optional<std::pmr::u8string> { std::move(result) };
    }

private:
    /// @brief Appends the declaration of a parameter to @p out,
    /// including the separating comma unless @p out ends with an opening parenthesis.
    static void declare_parameter(
        std::pmr::u8string& out,
        const std::u8string_view name,
        const std::u8string_view default_argument
    )
    {
        if (!out.ends_with(u8'(')) {
            out.append(u8", ");
        }
        out.append(name);
        if (!default_argument.empty()) {
            out.append(u8" = ");
            out.append(default_argument);
        }
    }

    /// @brief Adds an edit which replaces @p span with @p replacement.
    void add_edit(const File_Source_Span& span, const std::u8string_view replacement)
    {
        body_edits.push_back(
            Source_Edit { span,
                          std::pmr::u8string { replacement, context.get_persistent_memory() } }
        );
    }

    /// @brief Returns the name of the parameter for the positional argument at @p index,
    /// generating the name if this index has not been used so far.
    [[nodiscard]]
    std::u8string_view positional_name(const std::size_t index)
    {
        while (positional_parameters.size() <= index) {
            const auto digits = to_characters8(positional_parameters.size());
            std::pmr::u8string name { u8"arg", context.get_persistent_memory() };
            name.append(digits.as_string());
            positional_parameters.push_back(Positional_Parameter { std::move(name), false, {} });
        }
        return positional_parameters[index].name;
    }

    /// @brief Registers the parameter which captures @p target, and returns its name.
    ///
    /// Sets @ref bailout when the parameter cannot be declared,
    /// e.g. because the same parameter is used with two different default arguments.
    [[nodiscard]]
    std::u8string_view
    declare_target(const Put_Target& target, const std::u8string_view default_source)
    {
        switch (target.kind) {
        case Put_Target::Kind::contents: {
            if (uses_contents && !contents_default.empty() && contents_default != default_source) {
                bailout = true;
                return {};
            }
            uses_contents = true;
            if (!default_source.empty()) {
                contents_default = default_source;
            }
            return u8"content";
        }
        case Put_Target::Kind::positional: {
            const std::u8string_view name = positional_name(target.index);
            Positional_Parameter& parameter = positional_parameters[target.index];
            if (!parameter.default_argument.empty()
                && parameter.default_argument != default_source) {
                bailout = true;
                return {};
            }
            if (!default_source.empty()) {
                parameter.default_argument = default_source;
            }
            parameter.used = true;
            return name;
        }
        case Put_Target::Kind::named: {
            // The parameter must have a name which can be spelled in the definition,
            // which is generally not the case for a name which is not an identifier.
            if (!is_identifier(target.name)) {
                bailout = true;
                return {};
            }
            for (Named_Parameter& parameter : named_parameters) {
                if (parameter.name == target.name) {
                    if (!parameter.default_argument.empty()
                        && parameter.default_argument != default_source) {
                        bailout = true;
                        return {};
                    }
                    if (!default_source.empty()) {
                        parameter.default_argument = default_source;
                    }
                    return parameter.name;
                }
            }
            named_parameters.push_back(Named_Parameter { target.name, default_source });
            return named_parameters.back().name;
        }
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid kind of put target.");
    }

    /// @brief Walks the body of the macro, or the contents of a string literal,
    /// adding an edit for every `\cowel_put` call and ellipsis argument.
    void walk_elements(const std::span<const ast::Markup_Element> elements)
    {
        for (const ast::Markup_Element& element : elements) {
            if (bailout) {
                return;
            }
            if (const ast::Directive* const directive = element.try_as_directive()) {
                walk_directive(*directive, Put_Position::splice);
            }
            else if (const ast::Primary* const primary = element.try_as_primary()) {
                walk_primary(*primary);
            }
            else {
                walk_expression(element.as_expression(), Put_Position::splice);
            }
        }
    }

    /// @brief Walks a primary, recursing into blocks, string literals, and groups.
    void walk_primary(const ast::Primary& primary)
    {
        switch (primary.get_kind()) {
        case ast::Primary_Kind::block:
        case ast::Primary_Kind::quoted_string: walk_elements(primary.get_elements()); return;
        case ast::Primary_Kind::group: walk_members(primary.get_members()); return;
        default: return;
        }
    }

    /// @brief Walks the arguments of a directive or the members of a group,
    /// replacing every ellipsis argument with the pack parameter which forwards it.
    void walk_members(const std::span<const ast::Group_Member> members)
    {
        for (const ast::Group_Member& member : members) {
            if (bailout) {
                return;
            }
            if (member.get_kind() == ast::Member_Kind::ellipsis) {
                // An ellipsis argument expands all arguments of the invocation,
                // which the pack parameter does as well.
                add_edit(member.get_source_span(), u8"...args");
                uses_pack = true;
                continue;
            }
            if (member.has_value()) {
                walk_expression(member.get_value(), Put_Position::expression);
            }
        }
    }

    /// @brief Walks an expression, adding an edit for every `\cowel_put` call.
    void walk_expression(const ast::Expression& expression, const Put_Position position)
    {
        if (const ast::Directive* const directive = expression.try_as_directive()) {
            walk_directive(*directive, position);
        }
        else if (const ast::Primary* const primary = expression.try_as_primary()) {
            walk_primary(*primary);
        }
        else if (const ast::Unary_Expression* const unary = expression.try_as_unary()) {
            walk_expression(unary->get_operand(), position);
        }
        else if (const ast::Binary_Expression* const binary = expression.try_as_binary()) {
            walk_expression(binary->get_lhs(), position);
            walk_expression(binary->get_rhs(), position);
        }
        else if (const ast::Let_Expression* const let = expression.try_as_let()) {
            walk_expression(let->get_value(), position);
        }
        else if (expression.is_fun()) {
            // Nested function-expressions are not supported by the AST,
            // and the body of the function would be attributed to the outer definition.
            bailout = true;
        }
    }

    /// @brief Walks a directive, adding an edit for every `\cowel_put` call in it.
    void walk_directive(const ast::Directive& directive, const Put_Position position)
    {
        if (directive.get_name() == u8"cowel_put") {
            fix_put(directive, position);
            return;
        }
        if (directive.get_name() == u8"cowel_macro") {
            // Macros cannot be nested,
            // and the arguments of an inner definition would be attributed to the outer one.
            bailout = true;
            return;
        }
        walk_members(directive.get_argument_span());
        for (const ast::Primary& block : directive.get_block_arguments()) {
            walk_primary(block);
        }
    }

    /// @brief Replaces a `\cowel_put` call with a reference to the parameter
    /// which captures the argument that the call expands.
    void fix_put(const ast::Directive& put, const Put_Position position)
    {
        const std::optional<Put_Resolution> resolution = resolve_put(put);
        if (!resolution) {
            bailout = true;
            return;
        }
        const std::u8string_view name
            = declare_target(resolution->target, resolution->default_source);
        if (bailout) {
            return;
        }
        if (position == Put_Position::expression) {
            add_edit(put.get_source_span(), name);
            return;
        }
        std::pmr::u8string splice { u8"\\(", context.get_persistent_memory() };
        splice.append(name);
        splice.append(u8")");
        add_edit(put.get_source_span(), splice);
    }

    /// @brief Determines the argument which a `\cowel_put` call expands.
    /// @return An empty optional if this cannot be determined,
    /// e.g. because the call uses an argument which is only known at run time.
    [[nodiscard]]
    std::optional<Put_Resolution> resolve_put(const ast::Directive& put) const
    {
        if (put.has_ellipsis()) {
            // The call is resolved through an ellipsis argument,
            // which would have to be resolved against the arguments of the invocation.
            return std::optional<Put_Resolution> {};
        }

        std::optional<std::size_t> block_index;
        std::optional<std::size_t> positional_index;
        std::optional<std::u8string_view> named_argument;
        std::u8string_view default_source {};

        for (const ast::Group_Member& member : put.get_argument_span()) {
            if (member.get_kind() == ast::Member_Kind::named) {
                const std::u8string_view name = member.get_name().get_source();
                if (name == u8"else") {
                    if (!default_source.empty()) {
                        return std::optional<Put_Resolution> {};
                    }
                    default_source = member.get_value().get_source();
                }
                else if (name == u8"block") {
                    const std::optional<std::size_t> index = integer_argument(member.get_value());
                    if (!index || block_index) {
                        return std::optional<Put_Resolution> {};
                    }
                    block_index = index;
                }
                else if (name == u8"pos") {
                    const std::optional<std::size_t> index = integer_argument(member.get_value());
                    if (!index || positional_index) {
                        return std::optional<Put_Resolution> {};
                    }
                    positional_index = index;
                }
                else if (name == u8"name") {
                    const std::optional<std::u8string_view> value
                        = string_argument(member.get_value());
                    if (!value || named_argument) {
                        return std::optional<Put_Resolution> {};
                    }
                    named_argument = value;
                }
                else {
                    return std::optional<Put_Resolution> {};
                }
                continue;
            }
            if (member.get_kind() == ast::Member_Kind::positional) {
                // A positional integer argument is the 'block' argument,
                // and a positional argument of another type is the 'else' argument.
                if (const std::optional<std::size_t> index = integer_argument(member.get_value())) {
                    if (block_index) {
                        return std::optional<Put_Resolution> {};
                    }
                    block_index = index;
                }
                else {
                    if (!default_source.empty() || !is_else_argument(member.get_value())) {
                        return std::optional<Put_Resolution> {};
                    }
                    default_source = member.get_value().get_source();
                }
                continue;
            }
            // The value of an expansion or ellipsis argument is only known at run time.
            return std::optional<Put_Resolution> {};
        }

        std::optional<std::u8string_view> target_text;
        if (!put.get_block_arguments().empty()) {
            // The value of the block argument is spliced to plain text before it is interpreted.
            if (put.get_block_arguments().size() > 1) {
                return std::optional<Put_Resolution> {};
            }
            target_text = literal_text(put.get_block_arguments().front());
            if (!target_text) {
                return std::optional<Put_Resolution> {};
            }
        }

        const int selector_count = int(block_index.has_value()) + int(positional_index.has_value())
            + int(named_argument.has_value());
        if (selector_count > 1 || (selector_count != 0 && target_text.has_value())) {
            return std::optional<Put_Resolution> {};
        }
        if (target_text.has_value()) {
            if (target_text->empty()) {
                // A call like `\cowel_put{}` expands the contents of the macro.
                return Put_Resolution { Put_Target {}, default_source };
            }
            if (const std::optional<std::size_t> index = parse_index(*target_text)) {
                return Put_Resolution { Put_Target { Put_Target::Kind::positional, *index, {} },
                                        default_source };
            }
            // A call like `\cowel_put{name}` expands the argument of that name.
            return Put_Resolution { Put_Target { Put_Target::Kind::named, 0, *target_text },
                                    default_source };
        }
        if (block_index) {
            // The first block argument of the invocation is its contents.
            if (*block_index != 0) {
                return std::optional<Put_Resolution> {};
            }
            return Put_Resolution { Put_Target {}, default_source };
        }
        if (positional_index) {
            return Put_Resolution {
                Put_Target { Put_Target::Kind::positional, *positional_index, {} }, default_source
            };
        }
        if (named_argument) {
            return Put_Resolution { Put_Target { Put_Target::Kind::named, 0, *named_argument },
                                    default_source };
        }
        // A call with no selector and no block argument expands the contents of the macro.
        return Put_Resolution { Put_Target {}, default_source };
    }

    /// @brief Returns `true` if @p expression can be the `else` argument of a `\cowel_put` call.
    [[nodiscard]]
    static bool is_else_argument(const ast::Expression& expression)
    {
        const ast::Primary* const primary = expression.try_as_primary();
        return primary != nullptr
            && (primary->get_kind() == ast::Primary_Kind::quoted_string
                || primary->get_kind() == ast::Primary_Kind::block);
    }

    /// @brief Returns the value of @p expression if it is a non-empty string literal.
    [[nodiscard]]
    static std::optional<std::u8string_view> string_argument(const ast::Expression& expression)
    {
        const ast::Primary* const primary = expression.try_as_primary();
        if (primary == nullptr || primary->get_kind() != ast::Primary_Kind::quoted_string) {
            return std::optional<std::u8string_view> {};
        }
        const std::span<const ast::Markup_Element> elements = primary->get_elements();
        if (elements.size() != 1) {
            return std::optional<std::u8string_view> {};
        }
        const ast::Primary* const text = elements.front().try_as_primary();
        if (text == nullptr || text->get_kind() != ast::Primary_Kind::text) {
            return std::optional<std::u8string_view> {};
        }
        return text->get_source();
    }

    /// @brief Returns the value of @p expression if it is a non-negative integer literal.
    [[nodiscard]]
    static std::optional<std::size_t> integer_argument(const ast::Expression& expression)
    {
        const ast::Primary* const primary = expression.try_as_primary();
        if (primary == nullptr || primary->get_kind() != ast::Primary_Kind::int_literal) {
            return std::optional<std::size_t> {};
        }
        // A negative index is invalid.
        const std::u8string_view source = primary->get_source();
        if (!source.empty() && source.front() == u8'-') {
            return std::optional<std::size_t> {};
        }
        return parse_index(source);
    }

    /// @brief Returns the text of @p block,
    /// or a default-constructed view if the block is empty.
    /// @return An empty optional if the block is anything but a single text element.
    [[nodiscard]]
    static std::optional<std::u8string_view> literal_text(const ast::Primary& block)
    {
        const std::span<const ast::Markup_Element> elements = block.get_elements();
        if (elements.empty()) {
            return std::u8string_view {};
        }
        if (elements.size() != 1) {
            return std::optional<std::u8string_view> {};
        }
        const ast::Primary* const text = elements.front().try_as_primary();
        if (text == nullptr || text->get_kind() != ast::Primary_Kind::text) {
            return std::optional<std::u8string_view> {};
        }
        return text->get_source();
    }

    /// @brief Parses @p text as a non-negative decimal integer,
    /// which is how a `\cowel_put` call interprets an index.
    [[nodiscard]]
    static std::optional<std::size_t> parse_index(const std::u8string_view text)
    {
        if (text.empty()) {
            return std::optional<std::size_t> {};
        }
        return from_characters<std::size_t>(text);
    }
};

/// @brief Emits the deprecation warning for a `\cowel_macro` definition,
/// along with an auto-fix which rewrites it into a function-expression if possible.
void warn_macro_deprecated(
    const Invocation& call,
    Context& context,
    const std::span<const Value_And_Location<Value>> names
)
{
    if (!context.emits(Severity::warning) && !context.collects_fixes()) {
        return;
    }

    std::pmr::vector<Source_Edit> fixes { context.get_persistent_memory() };
    if (names.size() == 1) {
        // The definition of a macro with several names
        // cannot be replaced by a single function-expression.
        Macro_Fixer fixer { .context = context, .definition = call };
        if (std::optional<std::pmr::u8string> replacement
            = fixer.build(names.front().value.as_string())) {
            fixes.push_back(
                Source_Edit { call.directive.get_source_span(), std::move(*replacement) }
            );
        }
    }
    for (const Source_Edit& fix : fixes) {
        context.push_fix(fix);
    }

    const std::u8string_view message = fixes.empty()
        ? u8"\\cowel_macro is deprecated; use a function-expression instead; this definition could not be converted automatically."sv
        : u8"\\cowel_macro is deprecated; use a function-expression instead."sv;
    context.try_emit(
        Diagnostic {
            Severity::warning,
            diagnostic::deprecated,
            call.directive.get_name_span(),
            message,
            {},
            fixes,
        }
    );
}

}; // namespace

Processing_Status Macro_Behavior::do_evaluate(const Invocation& call, Context& context) const
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
        COWEL_ASSERT(call.has_block_arguments());
        context.try_fatal(
            diagnostic::alias_name_invalid, call.get_block_arguments_source_span(),
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

    for (const auto& [alias_name_value, location] : names.get()) {
        const std::u8string_view alias_name = alias_name_value.as_string();
        if (alias_name.empty()) {
            context.try_fatal(
                diagnostic::macro_name_missing, location, u8"The alias name must not be empty."sv
            );
            return Processing_Status::fatal;
        }
        if (!is_identifier(alias_name)) {
            context.try_fatal(
                diagnostic::macro_name_invalid, location,
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
                diagnostic::macro_duplicate, location,
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
        const bool success = context.emplace_macro(
            std::pmr::u8string { alias_name, context.get_transient_memory() },
            call.block_arguments.front().get_elements(), call.directive.get_source()
        );
        COWEL_ASSERT(success);
    }

    // Macros are deprecated in favor of function-expressions,
    // and a definition can usually be converted automatically.
    warn_macro_deprecated(call, context, names.get());

    return Processing_Status::ok;
}

Result<Argument, Processing_Status>
Put_Behavior::resolve(const Invocation& call, Context& context) const
{
    if (!call.content_frame) {
        context.try_error(
            diagnostic::put_outside, call.directive.get_source_span(),
            u8"\\cowel_put can only be used when expanded from macros, "
            u8"and this directive appeared at the top-level in the document."sv
        );
        return Processing_Status::error;
    }

    static constexpr Type else_alternatives[] { Type::str, Type::block };
    static constexpr auto else_type = Type::union_of(else_alternatives);
    static_assert(else_type.is_canonical());

    Lazy_Value_Of_Type_Matcher else_matcher { else_type };
    Parameter else_param { u8"else"sv, Optionality::optional, else_matcher };
    Integer_Matcher block_index_matcher;
    Parameter block_index_param { u8"block"sv, Optionality::optional, block_index_matcher };
    Integer_Matcher positional_index_matcher;
    Parameter positional_index_param { u8"pos"sv, Optionality::optional, positional_index_matcher };
    String_Matcher name_matcher { context.get_transient_memory() };
    Parameter name_param { u8"name"sv, Optionality::optional, name_matcher };
    Block_Matcher content_matcher;
    Parameter content_param { u8"content"sv, Optionality::optional, content_matcher };
    Parameter* const parameters[] {
        &else_param, &block_index_param, &positional_index_param, &name_param, &content_param,
    };

    const auto match_status = match_call(parameters, call, context);
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    const int selector_count = //
        int(block_index_matcher.was_matched()) + //
        int(positional_index_matcher.was_matched()) + //
        int(name_matcher.was_matched());
    if (selector_count > 1) {
        context.try_error(
            diagnostic::put_invalid, call.get_arguments_source_span(),
            u8"Only one of the 'block', 'pos', or 'name' arguments may be provided to \\cowel_put."sv
        );
        return Processing_Status::error;
    }
    if (selector_count != 0 && content_matcher.was_matched()) {
        context.try_error(
            diagnostic::put_invalid, call.get_arguments_source_span(),
            u8"A 'block', 'pos', or 'name' argument cannot be combined with a block argument."sv
        );
        return Processing_Status::error;
    }

    const Invocation& target_invocation = call.content_frame->invocation;

    const bool has_else = else_matcher.was_matched();
    const auto get_block_argument
        = [&](const std::size_t index) -> Result<std::optional<Argument>, Processing_Status> {
        if (index < target_invocation.block_arguments.size()) {
            return std::optional<Argument> {
                Argument::block(target_invocation.block_arguments[index])
            };
        }
        if (!has_else) {
            context.try_error(
                diagnostic::put_out_of_range, call.get_arguments_source_span(),
                u8"The requested block argument index is out of range."sv
            );
        }
        return std::optional<Argument> {};
    };

    // Simple case like \cowel_put where we expand the given contents.
    if (selector_count == 0 && !content_matcher.was_matched()) {
        if (target_invocation.block_arguments.empty()) {
            context.try_error(
                diagnostic::put_invalid, call.directive.get_source_span(),
                u8"\\cowel_put attempted to expand a block argument, but none was provided. "
                u8"Use \\cowel_put(else={}) to silence such cases."sv
            );
            return Processing_Status::error;
        }
        return Argument::block(target_invocation.block_arguments.front());
    }

    if (block_index_matcher.was_matched()) {
        const std::optional index = block_index_matcher.get().checked_as<std::size_t>();
        if (!index) {
            context.try_error(
                diagnostic::put_invalid, block_index_matcher.get_location(),
                u8"The 'block' argument must be a non-negative integer."sv
            );
            return Processing_Status::error;
        }
        const Result<std::optional<Argument>, Processing_Status> result
            = get_block_argument(*index);
        if (!result) {
            return result.error();
        }
        if (!*result) {
            if (has_else) {
                return else_matcher.get();
            }
            return Processing_Status::error;
        }
        return **result;
    }

    std::pmr::vector<char8_t> target_text { context.get_transient_memory() };
    std::u8string_view target_string;
    std::optional<std::size_t> needle_index;

    if (positional_index_matcher.was_matched()) {
        const std::optional index = positional_index_matcher.get().checked_as<std::size_t>();
        if (!index) {
            context.try_error(
                diagnostic::put_invalid, positional_index_matcher.get_location(),
                u8"The 'pos' argument must be a non-negative integer."sv
            );
            return Processing_Status::error;
        }
        needle_index = index;
    }
    else if (name_matcher.was_matched()) {
        target_string = name_matcher.get();
    }
    else {
        const Processing_Status target_status = splice_to_plaintext(
            target_text, call.block_arguments.front().get_elements(), call.content_frame, context
        );
        if (target_status != Processing_Status::ok) {
            return target_status;
        }
        target_string = as_u8string_view(target_text);

        // Simple case like \put where we expand the given contents.
        if (target_string.empty()) {
            // FIXME: error if need be
            COWEL_ASSERT(target_invocation.has_block_arguments());
            return Argument::block(target_invocation.block_arguments.front());
        }

        needle_index = from_characters<std::size_t>(target_string);
    }

    const auto arg_result = [&] -> Result<std::optional<Argument>, Processing_Status> {
        if (needle_index) {
            Put_Positional expand_positional {
                .context = context,
                .needle_index = *needle_index,
            };
            Result<std::optional<Argument>, Processing_Status> maybe_result
                = expand_positional.find(
                    target_invocation.get_arguments_span(), target_invocation.content_frame
                );
            if (!maybe_result) {
                return maybe_result.error();
            }
            if (!*maybe_result && !has_else) {
                if (positional_index_matcher.was_matched()) {
                    context.try_error(
                        diagnostic::put_out_of_range, call.get_arguments_source_span(),
                        u8"The requested positional argument index is out of range."sv
                    );
                }
                else {
                    const Characters8 limit_chars = to_characters8(expand_positional.index);
                    context.try_error(
                        diagnostic::put_out_of_range, call.directive.get_source_span(),
                        joined_char_sequence(
                            {
                                u8"This \\put directive is invalid "
                                u8"because the positional argument at index ["sv,
                                target_string,
                                u8"] was requested, but only "sv,
                                limit_chars.as_string(),
                                u8" were provided. "sv,
                            }
                        )
                    );
                }
            }
            return *maybe_result;
        }
        // NOLINTNEXTLINE(readability-else-after-return)
        else {
            const Put_Named expand_named {
                .context = context,
                .needle_name = target_string,
            };
            Result<std::optional<Argument>, Processing_Status> maybe_result = expand_named.find(
                target_invocation.get_arguments_span(), target_invocation.content_frame
            );
            if (!maybe_result) {
                return maybe_result.error();
            }
            if (!*maybe_result && !has_else) {
                context.try_error(
                    diagnostic::put_invalid, call.get_arguments_source_span(),
                    joined_char_sequence(
                        {
                            u8"The target \""sv,
                            target_string,
                            u8"\" is neither an integer, "sv
                            u8"nor does it refer to any named argument of the macro invocation."sv,
                        }
                    )
                );
            }
            return maybe_result;
        }
    }();
    if (!arg_result) {
        return arg_result.error();
    }
    const std::optional<Argument>& arg = *arg_result;
    if (!arg) {
        if (!has_else) {
            // Error has already been printed above.
            return Processing_Status::error;
        }
        return else_matcher.get();
    }

    return *arg;
}

// FIXME: Something is seriously inconsistent here.
//        `splice` should usually be a shortcut for evaluation
//        followed by splicing the result,
//        but this can't be reconciled with the fact
//        that `cowel_put` also inherits the paragraph when splicing.
Result<Value, Processing_Status>
Put_Behavior::evaluate(const Invocation& call, Context& context) const
{
    const Result<Argument, Processing_Status> result = resolve(call, context);
    if (!result) {
        return result.error();
    }

    const Invocation& target_invocation = call.content_frame->invocation;

    return result->evaluate(target_invocation.content_frame, context);
}

Processing_Status
Put_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    // FIXME: Maybe this function should just go away
    const Result<Argument, Processing_Status> result = resolve(call, context);
    if (!result) {
        return try_generate_error(out, call, context, result.error());
    }

    const Invocation& target_invocation = call.content_frame->invocation;

    try_inherit_paragraph(out);
    return result->splice(out, target_invocation.content_frame, context);
}

Processing_Status
Macro_Definition::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    try_inherit_paragraph(out);
    return splice_all(out, m_body, call.call_frame, context);
}

} // namespace cowel
