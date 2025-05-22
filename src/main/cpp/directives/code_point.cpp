#include "cowel/util/from_chars.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/directive_processing.hpp"

namespace cowel {
namespace {

constexpr char32_t error_point = char32_t(-1);

[[nodiscard]]
char32_t get_code_point(const ast::Directive& d, Context& context)
{
    if (!d.get_arguments().empty()) {
        const Source_Span pos = d.get_arguments().front().get_source_span();
        context.try_warning(
            diagnostic::ignored_args, pos, u8"Arguments to this directive are ignored."
        );
    }
    std::pmr::vector<char8_t> data { context.get_transient_memory() };
    to_plaintext(data, d.get_content(), context);
    const std::u8string_view digits = trim_ascii_blank({ data.data(), data.size() });
    if (digits.empty()) {
        context.try_error(
            diagnostic::U::blank, d.get_source_span(),
            u8"Expected a sequence of hexadecimal digits, but got a blank string."
        );
        return error_point;
    }

    std::optional<std::uint32_t> value = from_chars<std::uint32_t>(digits, 16);
    if (!value) {
        context.try_error(
            diagnostic::U::digits, d.get_source_span(),
            u8"Expected a sequence of hexadecimal digits."
        );
        return error_point;
    }

    const auto code_point = char32_t(*value);
    if (!is_scalar_value(code_point)) {
        context.try_error(
            diagnostic::U::nonscalar, d.get_source_span(),
            u8"The given hex sequence is not a Unicode scalar value. "
            u8"Therefore, it cannot be encoded as UTF-8."
        );
        return error_point;
    }

    return code_point;
}

} // namespace

void Code_Point_Behavior::generate_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    Context& context
) const
{
    const char32_t code_point = get_code_point(d, context);
    if (code_point == error_point) {
        try_generate_error_plaintext(out, d, context);
        return;
    }
    HTML_Writer out_writer { out };
    out_writer.write_inner_html(code_point);
}

void Code_Point_Behavior::generate_html(HTML_Writer& out, const ast::Directive& d, Context& context)
    const
{
    const char32_t code_point = get_code_point(d, context);
    if (code_point == error_point) {
        try_generate_error_html(out, d, context);
        return;
    }
    out.write_inner_html(code_point);
}

} // namespace cowel
