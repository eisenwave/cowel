#include <optional>
#include <string_view>
#include <vector>

#include "cowel/util/strings.hpp"

#include "cowel/policy/syntax_highlight.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/directive_arguments.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/theme_to_css.hpp"

using namespace std::string_view_literals;

namespace cowel {

Processing_Status
Code_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context) const
{
    Argument_Matcher args { parameters, context.get_transient_memory() };
    args.match(d.get_arguments());

    const Greedy_Result<String_Argument> lang
        = get_string_argument(lang_parameter, d, args, context);
    if (status_is_break(lang.status())) {
        return lang.status();
    }

    const Greedy_Result<String_Argument> prefix
        = get_string_argument(prefix_parameter, d, args, context);
    if (status_is_break(prefix.status())) {
        return prefix.status();
    }

    const Greedy_Result<String_Argument> suffix
        = get_string_argument(suffix_parameter, d, args, context);
    if (status_is_break(suffix.status())) {
        return suffix.status();
    }

    const auto borders = m_display == Directive_Display::in_line
        ? Greedy_Result<bool> { true }
        : get_yes_no_argument(
              borders_parameter, diagnostic::codeblock::borders_invalid, d, args, context, true
          );
    if (status_is_break(borders.status())) {
        return borders.status();
    }

    const auto nested = m_display == Directive_Display::block
        ? Greedy_Result<bool> { false }
        : get_yes_no_argument(
              nested_parameter, diagnostic::code::nested_invalid, d, args, context, false
          );
    if (status_is_break(nested.status())) {
        return nested.status();
    }
    const Processing_Status args_status = status_concat(
        lang.status(), prefix.status(), suffix.status(), borders.status(), nested.status()
    );

    // While syntax highlighting generally outputs HTML,
    // when plaintext content is needed (e.g. for ID synthesis),
    // we still want \code to be "transparent" by simply outputting plaintext.
    // Note that for consistent side effects,
    // we still process all the arguments above.
    if (out.get_language() == Output_Language::text) {
        const Processing_Status text_status = consume_all(out, d.get_content(), context);
        return status_concat(args_status, text_status);
    }

    ensure_paragraph_matches_display(out, m_display);

    // All content written to out is HTML anyway,
    // so we don't need an extra HTML_Content_Policy here.
    HTML_Writer writer { out };
    const bool has_tags = !*nested;

    if (has_tags) {
        Attribute_Writer attributes = writer.open_tag_with_attributes(m_tag_name);
        if (!*borders) {
            COWEL_ASSERT(m_display != Directive_Display::in_line);
            attributes.write_class(u8"borderless"sv);
        }
        attributes.end();
    }

    const bool should_trim = m_pre_compat_trim == Pre_Trimming::yes;

    Vector_Text_Sink buffer_sink { Output_Language::html, context.get_transient_memory() };
    auto& chosen_sink = should_trim ? buffer_sink : static_cast<Text_Sink&>(out);

    Syntax_Highlight_Policy highlight_policy //
        { context.get_transient_memory() };
    highlight_policy.write_phantom(prefix->string);
    const auto highlight_status = consume_all(highlight_policy, d.get_content(), context);
    highlight_policy.write_phantom(suffix->string);

    const Result<void, Syntax_Highlight_Error> result
        = highlight_policy.dump_to(chosen_sink, context, lang->string);
    if (!result) {
        diagnose(result.error(), lang->string, d, context);
    }
    if (should_trim) {
        // https://html.spec.whatwg.org/dev/grouping-content.html#the-pre-element
        // Leading newlines immediately following <pre> are stripped anyway.
        // The same applies to any elements styled "white-space: pre".
        // In general, it is best to remove these.
        // To ensure portability, we need to trim away newlines (if any).
        auto inner_html = as_u8string_view(*buffer_sink);
        while (inner_html.starts_with(u8'\n')) {
            inner_html.remove_prefix(1);
        }
        while (inner_html.ends_with(u8'\n')) {
            inner_html.remove_suffix(1);
        }
        out.write(inner_html, Output_Language::html);
    }

    if (has_tags) {
        writer.close_tag(m_tag_name);
    }

    return status_concat(args_status, highlight_status);
}

Processing_Status
Highlight_As_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context)
    const
{
    Argument_Matcher args { parameters, context.get_transient_memory() };

    std::pmr::vector<char8_t> name_data { context.get_transient_memory() };
    const Result<bool, Processing_Status> has_name_result
        = argument_to_plaintext(name_data, d, args, name_parameter, context);
    if (!has_name_result) {
        return has_name_result.error();
    }
    if (!*has_name_result) {
        context.try_error(
            diagnostic::highlight_name_missing, d.get_source_span(),
            u8"A name parameter is required to specify the kind of highlight to apply."sv
        );
        return try_generate_error(out, d, context);
    }
    const auto name_string = as_u8string_view(name_data);

    const std::optional<ulight::Highlight_Type> type = highlight_type_by_long_string(name_string);
    if (!type) {
        const std::u8string_view message[] {
            u8"The given highlight name \"",
            name_string,
            u8"\" is not a valid ulight highlight name (long form).",
        };
        context.try_error(
            diagnostic::highlight_name_invalid, d.get_source_span(), joined_char_sequence(message)
        );
        return try_generate_error(out, d, context);
    }

    const std::u8string_view short_name = ulight::highlight_type_short_string_u8(*type);
    COWEL_ASSERT(!short_name.empty());

    // We do the same special casing as for \code (see above for explanation).
    if (out.get_language() == Output_Language::text) {
        return consume_all(out, d.get_content(), context);
    }

    HTML_Content_Policy policy { out };
    HTML_Writer writer { policy };
    writer
        .open_tag_with_attributes(u8"h-") //
        .write_attribute(u8"data-h", short_name)
        .end();
    const Processing_Status result = consume_all(policy, d.get_content(), context);
    if (status_is_break(result)) {
        return result;
    }
    writer.close_tag(u8"h-");
    return result;
}

} // namespace cowel
