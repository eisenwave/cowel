#include "mmml/builtin_directive_set.hpp"
#include "mmml/directive_processing.hpp"

namespace mmml {
namespace {

[[nodiscard]]
std::u8string_view argument_to_plaintext_or(
    std::pmr::vector<char8_t>& out,
    std::u8string_view parameter_name,
    std::u8string_view fallback,
    const ast::Directive& directive,
    const Argument_Matcher& args,
    Context& context
)
{
    const int index = args.get_argument_index(parameter_name);
    if (index < 0) {
        return fallback;
    }
    to_plaintext(out, directive.get_arguments()[std::size_t(index)].get_content(), context);
    return { out.data(), out.size() };
}

void diagnose(
    Syntax_Highlight_Error error,
    std::u8string_view lang,
    const ast::Directive& d,
    Context& context
)
{
    if (!context.emits(Severity::warning)) {
        return;
    }
    switch (error) {
    case Syntax_Highlight_Error::unsupported_language: {
        if (lang.empty()) {
            context.try_warning(
                diagnostic::highlight_language, d.get_source_span(),
                u8"Syntax highlighting was not possible because no language was given, "
                u8"and automatic language detection was not possible. "
                u8"Please use \\tt{...} or \\pre{...} if you want a code (block) "
                u8"without any syntax highlighting."
            );
            break;
        }
        Diagnostic warning
            = context.make_warning(diagnostic::highlight_language, d.get_source_span());
        warning.message
            += u8"Unable to apply syntax highlighting because the specified language \"";
        warning.message += lang;
        warning.message += u8"\" is not supported.";
        context.emit(std::move(warning));
        break;
    }
    case Syntax_Highlight_Error::bad_code: {
        Diagnostic warning
            = context.make_warning(diagnostic::highlight_malformed, d.get_source_span());
        warning.message += u8"Unable to apply syntax highlighting because the code is not valid "
                           u8"for the specified language \"";
        warning.message += lang;
        warning.message += u8"\".";
        context.emit(std::move(warning));
        break;
    }
    case Syntax_Highlight_Error::other: {
        Diagnostic warning = context.make_warning(diagnostic::highlight_error, d.get_source_span());
        warning.message += u8"Unable to apply syntax highlighting because of an internal error.";
        warning.message += lang;
        warning.message += u8"\".";
        context.emit(std::move(warning));
        break;
    }
    }
}

} // namespace

void Syntax_Highlight_Behavior::
    generate_plaintext(std::pmr::vector<char8_t>&, const ast::Directive&, const Argument_Matcher&, Context&)
        const
{
}

void Syntax_Highlight_Behavior::generate_html(
    HTML_Writer& out,
    const ast::Directive& d,
    const Argument_Matcher& args,
    Context& context
) const
{
    std::pmr::vector<char8_t> lang_data { context.get_transient_memory() };
    const std::u8string_view lang
        = argument_to_plaintext_or(lang_data, lang_parameter, u8"", d, args, context);

    std::pmr::vector<char8_t> borders_data { context.get_transient_memory() };
    const bool has_borders = this->display == Directive_Display::block
        && argument_to_plaintext_or(borders_data, borders_parameter, u8"yes", d, args, context)
            != u8"no";

    Attribute_Writer attributes = out.open_tag_with_attributes(m_tag_name);
    if (this->display == Directive_Display::block && !has_borders) {
        attributes.write_class(u8"borderless");
    }
    attributes.end();

    const Result<void, Syntax_Highlight_Error> result
        = to_html_syntax_highlighted(out, d.get_content(), lang, context, m_to_html_mode);
    if (!result) {
        to_html(out, d.get_content(), context, m_to_html_mode);
        diagnose(result.error(), lang, d, context);
    }
    out.close_tag(m_tag_name);
}

} // namespace mmml
