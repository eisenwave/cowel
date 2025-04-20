#include <vector>

#include "mmml/util/from_chars.hpp"
#include "mmml/util/html_entities.hpp"
#include "mmml/util/strings.hpp"

#include "mmml/builtin_directive_set.hpp"
#include "mmml/directive_processing.hpp"

namespace mmml {
namespace {

[[nodiscard]]
std::u32string_view as_string_view(const std::array<char32_t, 2>& array)
{
    const std::size_t length = array[0] == 0 ? 0 : array[1] == 0 ? 1 : 2;
    return { array.data(), length };
}

void check_arguments(const ast::Directive& d, Context& context)
{
    if (!d.get_arguments().empty()) {
        const Source_Span pos = d.get_arguments().front().get_source_span();
        context.try_warning(
            diagnostic::c_args_ignored, pos, u8"Arguments to this directive are ignored."
        );
    }
}

[[nodiscard]]
std::array<char32_t, 2> get_code_points_from_digits(
    std::u8string_view digits,
    int base,
    const ast::Directive& d,
    Context& context
)
{
    std::optional<std::uint32_t> value = from_chars<std::uint32_t>(digits, base);
    if (!value) {
        const std::u8string_view message = base == 10
            ? u8"Expected a sequence of decimal digits."
            : u8"Expected a sequence of hexadecimal digits.";
        context.try_error(diagnostic::c_digits, d.get_source_span(), message);
        return {};
    }

    const auto code_point = char32_t(*value);
    if (!is_scalar_value(code_point)) {
        context.try_error(
            diagnostic::c_nonscalar, d.get_source_span(),
            u8"The given hex sequence is not a Unicode scalar value. "
            u8"Therefore, it cannot be encoded as UTF-8."
        );
        return {};
    }

    return { code_point };
}

[[nodiscard]]
std::array<char32_t, 2>
get_code_points(std::u8string_view trimmed_text, const ast::Directive& d, Context& context)
{
    if (trimmed_text.empty()) {
        context.try_error(
            diagnostic::c_blank, d.get_source_span(),
            u8"Expected an HTML character reference, but got a blank string."
        );
        return {};
    }
    if (trimmed_text[0] == u8'#') {
        const int base
            = trimmed_text.starts_with(u8"#x") || trimmed_text.starts_with(u8"#X") ? 16 : 10;
        return get_code_points_from_digits(trimmed_text.substr(2), base, d, context);
    }
    const std::array<char32_t, 2> result = code_points_by_character_reference_name(trimmed_text);
    if (result[0] == 0) {
        context.try_error(
            diagnostic::c_name, d.get_source_span(), u8"Invalid named HTML character."
        );
    }
    return result;
}

} // namespace

void HTML_Entity_Behavior::generate_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    Context& context
) const
{
    check_arguments(d, context);
    std::pmr::vector<char8_t> data { context.get_transient_memory() };
    to_plaintext(data, d.get_content(), context);
    const std::u8string_view trimmed_text = trim_ascii_blank({ data.data(), data.size() });
    const std::array<char32_t, 2> code_points = get_code_points(trimmed_text, d, context);
    if (code_points[0] == 0) {
        try_generate_error_plaintext(out, d, context);
        return;
    }
    HTML_Writer out_writer { out };
    out_writer.write_inner_html(as_string_view(code_points));
}

void HTML_Entity_Behavior::generate_html(
    HTML_Writer& out,
    const ast::Directive& d,
    Context& context
) const
{
    check_arguments(d, context);
    std::pmr::vector<char8_t> data { context.get_transient_memory() };
    to_plaintext(data, d.get_content(), context);
    const std::u8string_view trimmed_text = trim_ascii_blank({ data.data(), data.size() });
    if (get_code_points(trimmed_text, d, context)[0] == 0) {
        try_generate_error_html(out, d, context);
        return;
    }
    out.write_inner_html(u8'&');
    out.write_inner_html(trimmed_text);
    out.write_inner_html(u8';');
}

} // namespace mmml
