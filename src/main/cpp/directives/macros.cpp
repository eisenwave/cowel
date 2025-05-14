#include <string_view>
#include <vector>

#include "mmml/ast.hpp"
#include "mmml/builtin_directive_set.hpp"
#include "mmml/context.hpp"
#include "mmml/directive_processing.hpp"

namespace mmml {

void Def_Behavior::evaluate(const ast::Directive& d, Context& context) const
{
    static const std::u8string_view parameters[] { u8"pattern" };
    Argument_Matcher args { parameters, context.get_transient_memory() };
    args.match(d.get_arguments(), context.get_source());

    const int pattern_index = args.get_argument_index(u8"pattern");
    if (pattern_index < 0) {
        context.try_error(
            diagnostic::def_no_pattern, d.get_source_span(),
            u8"A directive pattern must be provided when defining a macro."
        );
        return;
    }
    const ast::Argument& pattern_arg = d.get_arguments()[std::size_t(pattern_index)];
    if (pattern_arg.get_content().size() != 1
        || !std::holds_alternative<ast::Directive>(pattern_arg.get_content()[0])) {
        context.try_error(
            diagnostic::def_pattern_no_directive, pattern_arg.get_source_span(),
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
        if (context.emits(Severity::soft_warning)) {
            Diagnostic diagnostic
                = context.make_soft_warning(diagnostic::def_redefinition, d.get_source_span());
            diagnostic.message += u8"Redefinition of macro \"";
            diagnostic.message += pattern_name;
            diagnostic.message += u8"\".";
            context.emit(std::move(diagnostic));
        }
    }
}

void Macro_Behavior::generate_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    Context& context
) const
{
    const std::u8string_view name = d.get_name(context.get_source());
    const ast::Directive* const definition = context.find_macro(name);
    // There should be no mismatch between the directive behavior lookup and macro lookup.
    MMML_ASSERT(definition);

    std::pmr::vector<ast::Content> instantiation = instantiate_macro(*definition, d, context);
    to_plaintext(out, instantiation, context);
}

void Macro_Behavior::generate_html(HTML_Writer& out, const ast::Directive& d, Context& context)
    const
{
    const std::u8string_view name = d.get_name(context.get_source());
    const ast::Directive* const definition = context.find_macro(name);
    // There should be no mismatch between the directive behavior lookup and macro lookup.
    MMML_ASSERT(definition);

    std::pmr::vector<ast::Content> instantiation = instantiate_macro(*definition, d, context);
    to_html(out, instantiation, context);
}

} // namespace mmml
