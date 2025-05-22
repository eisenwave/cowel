#include <string_view>
#include <vector>

#include "cowel/util/strings.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/theme_to_css.hpp"

namespace cowel {
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
        const std::u8string_view message[] {
            u8"Unable to apply syntax highlighting because the specified language \"",
            lang,
            u8"\" is not supported.",
        };
        context.emit_warning(diagnostic::highlight_language, d.get_source_span(), message);
        break;
    }
    case Syntax_Highlight_Error::bad_code: {
        const std::u8string_view message[] {
            u8"Unable to apply syntax highlighting because the code is not valid "
            u8"for the specified language \"",
            lang,
            u8"\".",
        };
        context.emit_warning(diagnostic::highlight_malformed, d.get_source_span(), message);
        break;
    }
    case Syntax_Highlight_Error::other: {
        const std::u8string_view message[] {
            u8"Unable to apply syntax highlighting because of an internal error.",
            lang,
            u8"\".",
        };
        context.emit_warning(diagnostic::highlight_error, d.get_source_span(), message);
        break;
    }
    }
}

} // namespace

void Syntax_Highlight_Behavior::generate_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    const Argument_Matcher&,
    Context& context
) const
{
    if (display == Directive_Display::in_line) {
        to_plaintext(out, d, context);
    }
}

namespace {

struct String_Argument {
    std::pmr::vector<char8_t> data;
    std::u8string_view string;
};

String_Argument get_string_argument(
    std::u8string_view name,
    const ast::Directive& d,
    const Argument_Matcher& args,
    Context& context,
    std::u8string_view fallback = u8""
)
{
    String_Argument result { .data = std::pmr::vector<char8_t>(context.get_transient_memory()),
                             .string = {} };
    result.string = argument_to_plaintext_or(result.data, name, fallback, d, args, context);
    return result;
}

[[nodiscard]]
bool get_yes_no_argument(
    std::u8string_view name,
    std::u8string_view diagnostic_id,
    const ast::Directive& d,
    const Argument_Matcher& args,
    Context& context,
    bool fallback
)
{
    const int index = args.get_argument_index(name);
    if (index < 0) {
        return fallback;
    }
    const ast::Argument& arg = d.get_arguments()[std::size_t(index)];
    std::pmr::vector<char8_t> data { context.get_transient_memory() };
    to_plaintext(data, arg.get_content(), context);
    const auto string = as_u8string_view(data);
    if (string == u8"yes") {
        return true;
    }
    if (string == u8"no") {
        return false;
    }
    const std::u8string_view message[] {
        u8"Argument has to be \"yes\" or \"no\", but \"",
        string,
        u8"\" was given.",
    };
    context.try_warning(diagnostic_id, arg.get_source_span(), message);
    return fallback;
}

} // namespace

void Syntax_Highlight_Behavior::generate_html(
    HTML_Writer& out,
    const ast::Directive& d,
    const Argument_Matcher& args,
    Context& context
) const
{
    const String_Argument lang = get_string_argument(lang_parameter, d, args, context);
    const String_Argument prefix = get_string_argument(prefix_parameter, d, args, context);
    const String_Argument suffix = get_string_argument(suffix_parameter, d, args, context);

    const bool has_borders = this->display == Directive_Display::in_line
        || get_yes_no_argument(borders_parameter, diagnostic::codeblock::borders_invalid, d, args,
                               context, true);

    const bool is_nested = this->display == Directive_Display::in_line
        && get_yes_no_argument(nested_parameter, diagnostic::code::nested_invalid, d, args, context,
                               false);

    if (!is_nested) {
        Attribute_Writer attributes = out.open_tag_with_attributes(m_tag_name);
        if (!has_borders) {
            COWEL_ASSERT(display != Directive_Display::in_line);
            attributes.write_class(u8"borderless");
        }
        attributes.end();
    }

    const Result<void, Syntax_Highlight_Error> result = to_html_syntax_highlighted(
        out, d.get_content(), lang.string, context, prefix.string, suffix.string, m_to_html_mode
    );
    if (!result) {
        to_html(out, d.get_content(), context, m_to_html_mode);
        diagnose(result.error(), lang.string, d, context);
    }

    if (!is_nested) {
        out.close_tag(m_tag_name);
    }
}

void Highlight_Behavior::generate_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    const Argument_Matcher&,
    Context& context
) const
{
    to_plaintext(out, d.get_content(), context);
}

void Highlight_Behavior::generate_html(
    HTML_Writer& out,
    const ast::Directive& d,
    const Argument_Matcher& args,
    Context& context
) const
{
    std::pmr::vector<char8_t> name_data { context.get_transient_memory() };
    const bool has_name = argument_to_plaintext(name_data, d, args, name_parameter, context);
    if (!has_name) {
        context.try_error(
            diagnostic::hl::name_missing, d.get_source_span(),
            u8"A name parameter is required to specify the kind of highlight to apply."
        );
        try_generate_error_html(out, d, context);
        return;
    }
    const auto name_string = as_u8string_view(name_data);

    const std::optional<ulight::Highlight_Type> type = highlight_type_by_long_string(name_string);
    if (!type) {
        const std::u8string_view message[] {
            u8"The given highlight name \"",
            name_string,
            u8"\" is not a valid ulight highlight name (long form).",
        };
        context.try_error(diagnostic::hl::name_invalid, d.get_source_span(), message);
        try_generate_error_html(out, d, context);
        return;
    }

    const std::u8string_view short_name = ulight::highlight_type_short_string_u8(*type);
    COWEL_ASSERT(!short_name.empty());

    out.open_tag_with_attributes(u8"h-") //
        .write_attribute(u8"data-h", short_name)
        .end();
    to_html(out, d.get_content(), context);
    out.close_tag(u8"h-");
}

} // namespace cowel
