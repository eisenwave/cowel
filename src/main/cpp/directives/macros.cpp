#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/from_chars.hpp"
#include "cowel/util/function_ref.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/to_chars.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_arguments.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"

using namespace std::string_view_literals;

namespace cowel {

Processing_Status
Macro_Behavior::operator()(Content_Policy&, const Invocation& call, Context& context) const
{
    warn_ignored_argument_subset(call.arguments, context, Argument_Subset::named);

    std::pmr::vector<char8_t> alias_text { context.get_transient_memory() };
    for (const Argument_Ref ref : call.arguments) {
        const Processing_Status name_status
            = to_plaintext(alias_text, ref.ast_node.get_content(), ref.frame_index, context);
        switch (name_status) {
        case Processing_Status::ok: break;
        case Processing_Status::brk:
        case Processing_Status::fatal: return name_status;
        case Processing_Status::error:
        case Processing_Status::error_brk: {
            COWEL_ASSERT(!call.content.empty());
            context.try_fatal(
                diagnostic::alias_name_invalid, ast::get_source_span(call.content.front()),
                u8"Fatal error because generation of an alias failed."sv
            );
        }
        }
        const auto alias_name = as_u8string_view(alias_text);
        if (alias_name.empty()) {
            context.try_fatal(
                diagnostic::macro_name_missing, ref.ast_node.get_source_span(),
                u8"The alias name must not be empty."sv
            );
            return Processing_Status::fatal;
        }
        if (!is_directive_name(alias_name)) {
            COWEL_ASSERT(!ref.ast_node.get_content().empty());
            context.try_fatal(
                diagnostic::macro_name_invalid,
                ast::get_source_span(ref.ast_node.get_content().front()),
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
                diagnostic::macro_duplicate,
                ast::get_source_span(ref.ast_node.get_content().front()),
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
            std::pmr::u8string { alias_name, context.get_transient_memory() }, call.content,
            Macro_Type::cowel
        );
        COWEL_ASSERT(success);
        alias_text.clear();
    }

    return Processing_Status::ok;
}

Processing_Status
Put_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context) const
{
    if (call.content_frame == Frame_Index::root) {
        context.try_error(
            diagnostic::put_outside, call.directive.get_source_span(),
            u8"\\cowel_put can only be used when expanded from macros, "
            u8"and this directive appeared at the top-level in the document."sv
        );
        return try_generate_error(out, call, context);
    }

    static constexpr std::u8string_view parameters[] { u8"else" };
    Argument_Matcher matcher { parameters, context.get_transient_memory() };
    matcher.match(call.arguments);

    try_inherit_paragraph(out);

    Call_Stack& stack = context.get_call_stack();
    const Invocation& target_invocation = stack[call.content_frame].invocation;

    if (call.content.empty()) {
        return consume_all(
            out, target_invocation.content, target_invocation.content_frame, context
        );
    }

    const auto try_else = [&] -> std::optional<Processing_Status> {
        const int else_index = matcher.parameter_indices()[0];
        if (else_index < 0) {
            return {};
        }
        const Argument_Ref else_arg = call.arguments[std::size_t(else_index)];
        return consume_all(out, else_arg.ast_node.get_content(), else_arg.frame_index, context);
    };

    std::pmr::vector<char8_t> target_text { context.get_transient_memory() };
    const Processing_Status target_status
        = to_plaintext(target_text, call.content, call.content_frame, context);
    if (target_status != Processing_Status::ok) {
        return target_status;
    }
    const auto target_string = as_u8string_view(target_text);

    // Simple case like \put where we expand the given contents.
    if (target_string.empty()) {
        return consume_all(
            out, target_invocation.content, target_invocation.content_frame, context
        );
    }

    // Index case like \put{0} for selecting a given argument,
    // possibly with a fallback like \put[else=abc]{0}.
    const std::optional<std::size_t> arg_index = from_chars<std::size_t>(target_string);
    if (!arg_index) {
        for (const Argument_Ref arg : target_invocation.arguments) {
            if (arg.ast_node.get_name() == target_string) {
                return consume_all(out, arg.ast_node.get_content(), arg.frame_index, context);
            }
        }
        if (const std::optional<Processing_Status> else_status = try_else()) {
            return *else_status;
        }
        context.try_error(
            diagnostic::put_invalid, ast::get_source_span(call.directive.get_content().front()),
            joined_char_sequence({
                u8"The target \""sv,
                target_string,
                u8"\" is neither an integer, "sv
                u8"nor does it refer to any named argument of the macro invocation."sv,
            })
        );
        return try_generate_error(out, call, context);
    }

    std::size_t positional_index = 0;
    for (const Argument_Ref arg : target_invocation.arguments) {
        if (arg.ast_node.get_type() != ast::Argument_Type::positional) {
            continue;
        }
        if (*arg_index == positional_index++) {
            return consume_all(out, arg.ast_node.get_content(), arg.frame_index, context);
        }
    }
    if (const std::optional<Processing_Status> else_status = try_else()) {
        return *else_status;
    }
    const Characters8 limit_chars = to_characters8(positional_index);
    context.try_error(
        diagnostic::put_out_of_range, call.directive.get_source_span(),
        joined_char_sequence({
            u8"This \\put directive is invalid because the positional argument at index ["sv,
            target_string,
            u8"] was requested, but only "sv,
            limit_chars.as_string(),
            u8" were provided. "sv,
        })
    );

    return try_generate_error(out, call, context);
}

Processing_Status
Legacy_Macro_Behavior::operator()(Content_Policy&, const Invocation& call, Context& context) const
{
    context.try_warning(
        diagnostic::deprecated, call.directive.get_name_span(),
        u8"\\macro is deprecated. Use \\cowel_macro instead. "
        u8"Note that these are slightly different: \\put pseudo-directives "
        u8"are only supported within legacy \\macro directives."sv
    );

    static const std::u8string_view parameters[] { u8"pattern" };
    Argument_Matcher args { parameters, context.get_transient_memory() };
    args.match(call.arguments);

    const int pattern_index = args.get_argument_index(u8"pattern");
    if (pattern_index < 0) {
        context.try_error(
            diagnostic::macro::no_pattern, call.directive.get_source_span(),
            u8"A directive pattern must be provided when defining a macro."sv
        );
        return Processing_Status::error;
    }
    const Argument_Ref pattern_arg = call.arguments[std::size_t(pattern_index)];
    if (pattern_arg.ast_node.get_content().size() != 1
        || !std::holds_alternative<ast::Directive>(pattern_arg.ast_node.get_content()[0])) {
        context.try_error(
            diagnostic::macro::pattern_no_directive, pattern_arg.ast_node.get_source_span(),
            u8"The pattern in a macro definition has to be a single directive, nothing else."sv
        );
        return Processing_Status::error;
    }

    const auto& pattern_directive = std::get<ast::Directive>(pattern_arg.ast_node.get_content()[0]);
    // The pattern arguments and content currently have no special meaning.
    // They are merely used as documentation by the user, but are never processed.
    // We are only interested in the pattern name at the point of definition.
    const std::u8string_view pattern_name = pattern_directive.get_name();
    std::pmr::u8string owned_name { pattern_name, context.get_transient_memory() };

    const bool success
        = context.emplace_macro(std::move(owned_name), call.content, Macro_Type::legacy);
    if (!success) {
        const std::u8string_view message[] {
            u8"Failed redefinition of macro \"",
            pattern_name,
            u8"\".",
        };
        context.try_warning(
            diagnostic::macro::redefinition, call.directive.get_source_span(),
            joined_char_sequence(message)
        );
    }
    return Processing_Status::ok;
}

namespace {

enum struct Put_Response : bool {
    normal,
    abort,
};

[[nodiscard]]
Processing_Status substitute_in_macro(
    ast::Pmr_Vector<ast::Content>& content,
    const Invocation& call,
    Context& context,
    const Function_Ref<Put_Response(const File_Source_Span&)> on_variadic_put
)
{
    static constexpr std::u8string_view put_parameters[] { u8"else" };

    for (auto it = content.begin(); it != content.end();) {
        auto* const d = std::get_if<ast::Directive>(&*it);
        if (!d) {
            // Anything other than directives (text, etc.) are unaffected by macro substitution.
            ++it;
            continue;
        }

        // Before anything else, we have to replace the contents and the arguments of directives.
        // This comes even before the use evaluation of \put and \arg
        // in order to facilitate nesting, like \arg[\arg[0]].
        ast::Pmr_Vector<ast::Argument>& d_arguments = d->get_arguments();
        for (auto arg_it = d_arguments.begin(); arg_it != d_arguments.end();) {
            ast::Pmr_Vector<ast::Content>& arg_content = arg_it->get_content();
            // Regular case where we just have some content in directive arguments that we
            // run substitution on, recursively.
            if (arg_content.size() != 1
                || !std::holds_alternative<ast::Directive>(arg_content.front())) {
                const auto status
                    = substitute_in_macro(arg_content, call, context, on_variadic_put);
                if (status_is_break(status)) {
                    return status;
                }
                ++arg_it;
                continue;
            }
            // Special case where we have a single directive argument.
            // Within that context, \put{...} is treated specially and can be used as
            // a variadic expansion of the provided arguments.
            bool variadically_expanded = false;
            auto variadic_put_expand = [&](const File_Source_Span&) {
                // This just gets rid of the \put{...} argument,
                // to be replaced with expanded arguments.
                arg_it = d_arguments.erase(arg_it);
                const auto args_ast_nodes = std::views::transform(
                    call.arguments,
                    [](const Argument_Ref a) -> const ast::Argument& { return a.ast_node; }
                );
                arg_it = d_arguments.insert(arg_it, args_ast_nodes.begin(), args_ast_nodes.end());
                arg_it += std::ptrdiff_t(call.arguments.size());
                variadically_expanded = true;
                return Put_Response::abort;
            };
            const auto status
                = substitute_in_macro(arg_content, call, context, variadic_put_expand);
            if (status_is_break(status)) {
                return status;
            }
            if (!variadically_expanded) {
                ++arg_it;
            }
        }

        const auto status0 = substitute_in_macro(d->get_content(), call, context, on_variadic_put);
        if (status_is_break(status0)) {
            return status0;
        }

        if (d->get_name() != u8"put") {
            ++it;
            continue;
        }

        Homogeneous_Call_Arguments put_args_backend { d->get_arguments(), call.call_frame };
        Arguments_View put_args { put_args_backend };
        Argument_Matcher put_arg_matcher { put_parameters, context.get_transient_memory() };
        put_arg_matcher.match(put_args);
        warn_ignored_argument_subset(
            put_args, put_arg_matcher, context, Argument_Subset::unmatched
        );

        std::pmr::vector<char8_t> selection { context.get_transient_memory() };
        const auto status1 = to_plaintext(selection, d->get_content(), call.content_frame, context);
        if (status_is_break(status1)) {
            return status1;
        }

        const auto selection_string = trim_ascii_blank(as_u8string_view(selection));

        const auto substitute_arg = [&](const ast::Argument& arg) {
            const std::span<const ast::Content> arg_content = arg.get_content();
            it = content.insert(it, arg_content.begin(), arg_content.end());
            it += std::ptrdiff_t(arg_content.size());
        };

        // Simple case like \put where we expand the given contents.
        if (selection.empty()) {
            it = content.erase(it);
            it = content.insert(it, call.content.begin(), call.content.end());
            // We must skip over substituted content,
            // otherwise we risk expanding a \put directive that was passed to the macro,
            // rather than being in the macro definition,
            // and \put is only supposed to have special meaning within the macro definition.
            it += std::ptrdiff_t(call.content.size());
            continue;
        }

        // Variadic \put{...} case.
        // Handling depends on the context.
        if (selection_string == u8"...") {
            // important: erasing kills d, so we need to copy its location beforehand
            const File_Source_Span location = d->get_source_span();
            it = content.erase(it);
            if (on_variadic_put(location) == Put_Response::abort) {
                return Processing_Status::ok;
            }
            continue;
        }

        // Index case like \put{0} for selecting a given argument,
        // possibly with a fallback like \put[else=abc]{0}.
        const std::optional<std::size_t> arg_index = from_chars<std::size_t>(selection_string);
        if (!arg_index) {
            context.try_error(
                diagnostic::macro::put_invalid, d->get_source_span(),
                u8"The argument to this \\put pseudo-directive is invalid."sv
            );
            it = content.erase(it);
            continue;
        }
        if (*arg_index >= call.arguments.size()) {
            const int else_index = put_arg_matcher.get_argument_index(u8"else");
            if (else_index < 0) {
                const Characters8 limit_chars = to_characters8(call.arguments.size());
                const std::u8string_view message[] {
                    u8"This \\put directive is invalid because the positional argument at ",
                    u8"index [",
                    selection_string,
                    u8"] was requested, but only ",
                    limit_chars.as_string(),
                    u8" were provided. ",
                    u8"To make this valid, provide an \"else\" argument, like ",
                    u8"\\put[else=xyz]{0}."
                };
                context.try_error(
                    diagnostic::macro::put_out_of_range, d->get_source_span(),
                    joined_char_sequence(message)
                );
                it = content.erase(it);
                continue;
            }
            // It is very important that we do it in this order because erase(it)
            // also kills d.
            const ast::Argument else_arg = std::move(d->get_arguments()[std::size_t(else_index)]);
            it = content.erase(it);
            substitute_arg(else_arg);
            continue;
        }
        it = content.erase(it);
        substitute_arg(call.arguments[*arg_index].ast_node);
    }
    return Processing_Status::ok;
}

[[nodiscard]]
Processing_Status instantiate_legacy_macro(
    std::span<const ast::Content> definition,
    Content_Policy& out,
    const Invocation& call,
    Context& context
)
{
    ast::Pmr_Vector<ast::Content> instance {
        definition.begin(),
        definition.end(),
        context.get_transient_memory(),
    };

    auto on_variadic_put = [&](const File_Source_Span& location) {
        context.try_error(
            diagnostic::macro::put_args_outside_args, location,
            u8"A \\put[...] pseudo-directive can only be used as the sole positional argument "
            u8"in a directive."sv
        );
        return Put_Response::normal;
    };

    const auto instantiate_status = substitute_in_macro(instance, call, context, on_variadic_put);
    if (status_is_break(instantiate_status)) {
        return instantiate_status;
    }

    const auto consume_status = consume_all(out, instance, call.call_frame, context);

    return status_concat(instantiate_status, consume_status);
}

} // namespace

Processing_Status
Macro_Definition::operator()(Content_Policy& out, const Invocation& call, Context& context) const
{
    try_inherit_paragraph(out);

    switch (m_type) {
    case Macro_Type::cowel: return consume_all(out, m_body, call.call_frame, context);
    case Macro_Type::legacy: return instantiate_legacy_macro(m_body, out, call, context);
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid macro type.");
}

} // namespace cowel
