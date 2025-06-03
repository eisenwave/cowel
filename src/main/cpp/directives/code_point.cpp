#include <expected>
#include <string_view>
#include <vector>

#include "cowel/util/code_point_names.hpp"
#include "cowel/util/from_chars.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/to_chars.hpp"
#include "cowel/util/unicode.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/directive_processing.hpp"

namespace cowel {
namespace {

constexpr char32_t error_point = char32_t(-1);

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

[[nodiscard]]
char32_t
Code_Point_By_Digits_Behavior::get_code_point(const ast::Directive& d, Context& context) const
{
    if (!d.get_arguments().empty()) {
        const File_Source_Span8 pos = d.get_arguments().front().get_source_span();
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
        const std::u8string_view message[] {
            u8"Expected a sequence of hexadecimal digits, but got \"",
            digits,
            u8"\".",
        };
        context.try_error(diagnostic::U::digits, d.get_source_span(), message);
        return error_point;
    }

    const auto code_point = char32_t(*value);
    if (!is_scalar_value(code_point)) {
        const Characters8 chars = to_characters8(code_point);
        const std::u8string_view message[] {
            u8"The computed code point U+",
            chars.as_string(),
            u8" is not a Unicode scalar value. ",
            u8"Therefore, it cannot be encoded as UTF-8.",
        };
        context.try_error(diagnostic::U::nonscalar, d.get_source_span(), message);
        return error_point;
    }

    return code_point;
}

[[nodiscard]]
char32_t
Code_Point_By_Name_Behavior::get_code_point(const ast::Directive& d, Context& context) const
{
    if (!d.get_arguments().empty()) {
        const File_Source_Span8 pos = d.get_arguments().front().get_source_span();
        context.try_warning(
            diagnostic::ignored_args, pos, u8"Arguments to this directive are ignored."
        );
    }
    std::pmr::vector<char8_t> name { context.get_transient_memory() };
    to_plaintext(name, d.get_content(), context);
    const std::u8string_view digits = trim_ascii_blank({ name.data(), name.size() });
    if (digits.empty()) {
        context.try_error(
            diagnostic::N::blank, d.get_source_span(),
            u8"Expected the name of a Unicode code point, but got a blank string."
        );
        return error_point;
    }
    const auto name_string = as_u8string_view(name);

    const char32_t code_point = code_point_by_name(name_string);
    if (code_point == error_point) {
        const std::u8string_view message[] {
            u8"Expected an (all caps) name of a Unicode code point, but got \"",
            name_string,
            u8"\".",
        };
        context.try_error(diagnostic::N::invalid, d.get_source_span(), message);
        return error_point;
    }

    return code_point;
}

void Code_Point_Digits_Behavior::generate_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    Context& context
) const
{
    static constexpr std::u8string_view parameters[] { u8"zfill", u8"base", u8"lower" };

    constexpr std::size_t default_zfill = 0;
    constexpr std::size_t min_zfill = 0;
    constexpr std::size_t max_zfill = 1024;

    constexpr std::size_t default_base = 16;
    constexpr std::size_t min_base = 2;
    constexpr std::size_t max_base = 16;

    Argument_Matcher args { parameters, context.get_transient_memory() };
    args.match(d.get_arguments());

    const std::size_t zfill = get_integer_argument(
        u8"zfill", diagnostic::Udigits::zfill_not_an_integer, diagnostic::Udigits::zfill_range, //
        args, d, context, default_zfill, min_zfill, max_zfill
    );
    const std::size_t base = get_integer_argument(
        u8"base", diagnostic::Udigits::base_not_an_integer, diagnostic::Udigits::base_range, //
        args, d, context, default_base, min_base, max_base
    );
    const bool is_lower = get_yes_no_argument(
        u8"lower", diagnostic::Udigits::lower_invalid, d, args, context, false
    );

    std::pmr::vector<char8_t> input { context.get_transient_memory() };
    to_plaintext(input, d.get_content(), context);
    const auto input_text = as_u8string_view(input);

    if (input_text.empty()) {
        context.try_error(
            diagnostic::Udigits::blank, d.get_source_span(),
            u8"Nothing can be generated because the input is empty."
        );
        try_generate_error_plaintext(out, d, context);
        return;
    }

    const std::expected<utf8::Code_Point_And_Length, utf8::Unicode_Error> decode_result
        = utf8::decode_and_length(input_text);
    if (!decode_result) {
        context.try_error(
            diagnostic::Udigits::malformed, d.get_source_span(),
            u8"The input could not be interpreted as a code point because it is malformed UTF-8."
        );
        try_generate_error_plaintext(out, d, context);
        return;
    }

    const auto [code_point, length] = *decode_result;
    if (std::size_t(length) != input.size()) {
        COWEL_ASSERT(std::size_t(length) <= input.size());
        context.try_warning(
            diagnostic::Udigits::ignored, d.get_source_span(),
            u8"Some of the code units inside were ignored because only the first given code point "
            u8"is converted. "
            u8"This can happen if you type a character inside \\Udigits that consist of multiple "
            u8"code points, like a country flag."
        );
    }

    const bool convert_to_upper = !is_lower;
    const Characters8 chars
        = to_characters8(std::uint32_t(code_point), int(base), convert_to_upper);
    const std::size_t pad_length = std::min(zfill, chars.length);
    out.resize(out.size() + pad_length, u8'0');
    append(out, chars.as_string());
}

} // namespace cowel
