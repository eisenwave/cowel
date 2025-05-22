#include <string_view>
#include <vector>

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/context.hpp"
#include "cowel/directive_processing.hpp"

namespace cowel {

void Macro_Define_Behavior::evaluate(const ast::Directive& d, Context& context) const
{
    static const std::u8string_view parameters[] { u8"pattern" };
    Argument_Matcher args { parameters, context.get_transient_memory() };
    args.match(d.get_arguments(), context.get_source());

    const int pattern_index = args.get_argument_index(u8"pattern");
    if (pattern_index < 0) {
        context.try_error(
            diagnostic::macro::no_pattern, d.get_source_span(),
            u8"A directive pattern must be provided when defining a macro."
        );
        return;
    }
    const ast::Argument& pattern_arg = d.get_arguments()[std::size_t(pattern_index)];
    if (pattern_arg.get_content().size() != 1
        || !std::holds_alternative<ast::Directive>(pattern_arg.get_content()[0])) {
        context.try_error(
            diagnostic::macro::pattern_no_directive, pattern_arg.get_source_span(),
            u8"The pattern in a macro definition has to be a single directive, nothing else."
        );
        return;
    }

    const auto& pattern_directive = std::get<ast::Directive>(pattern_arg.get_content()[0]);
    // The pattern arguments and content currently have no special meaning.
    // They are merely used as documentation by the user, but are never processed.
    // We are only interested in the pattern name at the point of definition.
    const std::u8string_view pattern_name = pattern_directive.get_name(context.get_source());
    std::pmr::u8string owned_name { pattern_name, context.get_transient_memory() };

    const bool success = context.emplace_macro(std::move(owned_name), &d);
    if (!success) {
        const std::u8string_view message[] {
            u8"Redefinition of macro \"",
            pattern_name,
            u8"\".",
        };
        context.try_soft_warning(diagnostic::macro::redefinition, d.get_source_span(), message);
    }
}

namespace {

void substitute_in_macro(
    std::pmr::vector<ast::Content>& content,
    std::span<const ast::Argument> put_arguments,
    std::span<const ast::Content> put_content,
    Context& context
)
{
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
        for (auto& arg : d->get_arguments()) {
            substitute_in_macro(arg.get_content(), put_arguments, put_content, context);
        }
        substitute_in_macro(d->get_content(), put_arguments, put_content, context);

        const std::u8string_view name = d->get_name(context.get_source());
        if (name == u8"put") {
            // TODO: there's probably a way to do this faster, in a single step,
            //       but I couldn't find anything obvious in std::vector's interface.
            it = content.erase(it);
            it = content.insert(it, put_content.begin(), put_content.end());
            // We must skip over substituted content,
            // otherwise we risk expanding a \put directive that was passed to the macro,
            // rather than being in the macro definition,
            // and \put is only supposed to have special meaning within the macro definition.
            it += std::ptrdiff_t(put_content.size());
        }
        else {
            ++it;
        }
    }
}

void instantiate_macro(
    std::pmr::vector<ast::Content>& out,
    const ast::Directive& definition,
    std::span<const ast::Argument> put_arguments,
    std::span<const ast::Content> put_content,
    Context& context
)
{
    const std::span<const ast::Content> content = definition.get_content();
    out.insert(out.end(), content.begin(), content.end());
    substitute_in_macro(out, put_arguments, put_content, context);
}

} // namespace

void Macro_Instantiate_Behavior::generate_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    Context& context
) const
{
    std::pmr::vector<ast::Content> instantiation { context.get_transient_memory() };
    instantiate(instantiation, d, context);
    to_plaintext(out, instantiation, context);
}

void Macro_Instantiate_Behavior::generate_html(
    HTML_Writer& out,
    const ast::Directive& d,
    Context& context
) const
{
    std::pmr::vector<ast::Content> instantiation { context.get_transient_memory() };
    instantiate(instantiation, d, context);
    to_html(out, instantiation, context);
}

void Macro_Instantiate_Behavior::instantiate(
    std::pmr::vector<ast::Content>& out,
    const ast::Directive& d,
    Context& context
) const
{
    const std::u8string_view name = d.get_name(context.get_source());
    const ast::Directive* const definition = context.find_macro(name);
    // We always find a macro
    // because the name lookup for this directive utilizes `find_macro`,
    // so we're effectively calling it twice with the same input.
    COWEL_ASSERT(definition);

    instantiate_macro(out, *definition, d.get_arguments(), d.get_content(), context);
}

} // namespace cowel
