#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "cowel/parameters.hpp"
#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/chars.hpp"
#include "cowel/util/code_point_names.hpp"
#include "cowel/util/from_chars.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/to_chars.hpp"
#include "cowel/util/unicode.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"
#include "cowel/output_language.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

[[nodiscard]]
Result<char32_t, Processing_Status> code_point_by_generated_digits(
    std::span<const ast::Markup_Element> content,
    Frame_Index content_frame,
    const File_Source_Span& source_span,
    Context& context
)
{
    std::pmr::vector<char8_t> data { context.get_transient_memory() };
    const auto status = splice_to_plaintext(data, content, content_frame, context);
    if (status != Processing_Status::ok) {
        return status;
    }

    const std::u8string_view digits = trim_ascii_blank(as_u8string_view(data));
    if (digits.empty()) {
        context.try_error(
            diagnostic::char_blank, source_span,
            u8"Expected a sequence of hexadecimal digits, but got a blank string."sv
        );
        return Processing_Status::error;
    }

    const std::optional<std::uint32_t> value = from_chars<std::uint32_t>(digits, 16);
    if (!value) {
        const std::u8string_view message[] {
            u8"Expected a sequence of hexadecimal digits, but got \""sv,
            digits,
            u8"\"."sv,
        };
        context.try_error(diagnostic::char_digits, source_span, joined_char_sequence(message));
        return Processing_Status::error;
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
        context.try_error(diagnostic::char_nonscalar, source_span, joined_char_sequence(message));
        return Processing_Status::error;
    }

    return code_point;
}

[[nodiscard]]
Result<char32_t, Processing_Status> code_point_by_generated_name(
    std::span<const ast::Markup_Element> content,
    Frame_Index content_frame,
    const File_Source_Span& source_span,
    Context& context
)
{
    constexpr auto error_point = char32_t(-1);

    std::pmr::vector<char8_t> name { context.get_transient_memory() };
    const auto status = splice_to_plaintext(name, content, content_frame, context);
    if (status != Processing_Status::ok) {
        return status;
    }
    const auto name_string = as_u8string_view(name);

    const std::u8string_view digits = trim_ascii_blank(name_string);
    if (digits.empty()) {
        context.try_error(
            diagnostic::char_blank, source_span,
            u8"Expected the name of a Unicode code point, but got a blank string."sv
        );
        return Processing_Status::error;
    }

    const char32_t code_point = code_point_by_name(name_string);
    if (code_point == error_point) {
        const std::u8string_view message[] {
            u8"Expected an (all caps) name of a Unicode code point, but got \"",
            name_string,
            u8"\".",
        };
        context.try_error(diagnostic::char_name, source_span, joined_char_sequence(message));
        return Processing_Status::error;
    }

    return code_point;
}

} // namespace

[[nodiscard]]
Result<Short_String_Value, Processing_Status>
Code_Point_Behavior::do_evaluate(const Invocation& call, Context& context) const
{
    const auto match_status = match_empty_arguments(call, context);
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    const Result<char32_t, Processing_Status> code_point = get_code_point(call, context);
    if (!code_point) {
        COWEL_ASSERT(code_point.error() != Processing_Status::ok);
        return code_point.error();
    }

    return to_static_string<Short_String_Value::max_size_v>(make_char_sequence(*code_point));
}

[[nodiscard]]
Result<char32_t, Processing_Status>
Char_By_Num_Behavior::get_code_point(const Invocation& call, Context& context) const
{
    return code_point_by_generated_digits(
        call.get_content_span(), call.content_frame, call.directive.get_source_span(), context
    );
}

[[nodiscard]]
Result<char32_t, Processing_Status>
Char_By_Name_Behavior::get_code_point(const Invocation& call, Context& context) const
{
    return code_point_by_generated_name(
        call.get_content_span(), call.content_frame, call.directive.get_source_span(), context
    );
}

Processing_Status
Char_Get_Num_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    constexpr Integer default_zfill = 0;
    constexpr Integer min_zfill = 0;
    constexpr Integer max_zfill = 1024;

    constexpr Integer default_base = 16;
    constexpr Integer min_base = 2;
    constexpr Integer max_base = 16;

    Integer_Matcher zfill_integer;
    Group_Member_Matcher zfill_member { u8"zfill", Optionality::optional, zfill_integer };
    Integer_Matcher base_integer;
    Group_Member_Matcher base_member { u8"base", Optionality::optional, base_integer };
    Boolean_Matcher lower_boolean;
    Group_Member_Matcher lower_member { u8"lower", Optionality::optional, lower_boolean };

    Group_Member_Matcher* matchers[] { &zfill_member, &base_member, &lower_member };
    Pack_Usual_Matcher args_matcher { matchers };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto args_status
        = call_matcher.match_call(call, context, make_fail_callback(), Processing_Status::error);

    const Integer zfill = zfill_integer.get_or_default(default_zfill);
    if (zfill < min_zfill || zfill > max_zfill) {
        context.try_error(
            diagnostic::char_zfill_range, zfill_integer.get_location(),
            u8"Value for zfill is out of range."sv
        );
        return try_generate_error(out, call, context);
    }

    const Integer base = base_integer.get_or_default(default_base);
    if (base < min_base || base > max_base) {
        context.try_error(
            diagnostic::char_base_range, zfill_integer.get_location(),
            u8"Value for base is out of range."sv
        );
        return try_generate_error(out, call, context);
    }

    std::pmr::vector<char8_t> input { context.get_transient_memory() };
    const auto input_status
        = splice_to_plaintext(input, call.get_content_span(), call.content_frame, context);
    if (input_status != Processing_Status::ok) {
        return status_concat(args_status, input_status);
    }

    const auto input_text = as_u8string_view(input);

    if (input_text.empty()) {
        context.try_error(
            diagnostic::char_blank, call.directive.get_source_span(),
            u8"Nothing can be generated because the input is empty."sv
        );
        return try_generate_error(out, call, context);
    }

    const std::expected<utf8::Code_Point_And_Length, utf8::Unicode_Error> decode_result
        = utf8::decode_and_length(input_text);
    if (!decode_result) {
        context.try_error(
            diagnostic::char_corrupted, call.directive.get_source_span(),
            u8"The input could not be interpreted as a code point because it is malformed UTF-8."sv
        );
        return try_generate_error(out, call, context);
    }

    const auto [code_point, length] = *decode_result;
    if (std::size_t(length) != input.size()) {
        COWEL_ASSERT(std::size_t(length) <= input.size());
        context.try_warning(
            diagnostic::ignored_content, call.directive.get_source_span(),
            u8"Some of the code units inside were ignored because only the first given code point "
            u8"is converted. "
            u8"This can happen if you type a character inside \\Udigits that consist of multiple "
            u8"code points, like a country flag."sv
        );
    }

    const bool convert_to_upper = !lower_boolean.get_or_default(false);
    const Characters8 chars
        = to_characters8(std::uint32_t(code_point), int(base), convert_to_upper);
    if (const auto pad_length = std::min(std::size_t(zfill), chars.length())) {
        out.write(repeated_char_sequence(pad_length, u8'0'), Output_Language::text);
    }
    out.write(chars.as_string(), Output_Language::text);

    return status_concat(args_status, input_status);
}

} // namespace cowel
