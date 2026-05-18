#include <cstddef>
#include <expected>
#include <span>
#include <string_view>

#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/chars.hpp"
#include "cowel/util/code_point_names.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/unicode.hpp"

#include "cowel/big_int.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"
#include "cowel/parameters.hpp"

#include "cowel/syntax/ast.hpp"

using namespace std::string_view_literals;

namespace cowel {

[[nodiscard]]
Result<Short_String_Value, Processing_Status>
Code_Point_Behavior::do_evaluate(const Invocation& call, Context& context) const
{
    const Result<char32_t, Processing_Status> code_point = get_code_point(call, context);
    if (!code_point) {
        COWEL_ASSERT(code_point.error() != Processing_Status::ok);
        return code_point.error();
    }

    if (context.collects_hovers()) {
        const char32_t cp = *code_point;
        // Format as U+XXXX (minimum 4 uppercase hex digits).
        const auto hex_chars = to_characters8(static_cast<std::uint32_t>(cp), 16, true);
        const std::size_t hex_len = hex_chars.size();
        const std::size_t min_len = 4;
        const std::size_t padded_len = hex_len < min_len ? min_len : hex_len;
        const std::size_t total = 2u + padded_len;
        std::pmr::u8string article { context.get_persistent_memory() };
        article.reserve(total);
        article += u8'U';
        article += u8'+';
        // Left-pad with zeros if needed
        for (std::size_t i = hex_len; i < min_len; ++i) {
            article += u8'0';
        }
        article += std::u8string_view(hex_chars);
        COWEL_ASSERT(!article.empty());
        context.push_hover(call.directive.get_source_span(), article);
    }

    return to_static_string<Short_String_Value::max_size_v>(make_char_sequence(*code_point));
}

[[nodiscard]]
Result<char32_t, Processing_Status>
Char_By_Num_Behavior::get_code_point(const Invocation& call, Context& context) const
{
    Integer_Matcher num_matcher;
    Parameter num_param { u8"num", Optionality::mandatory, num_matcher };
    Parameter* const parameters[] { &num_param };

    const auto args_status = match_call(parameters, call, context);
    if (args_status != Processing_Status::ok) {
        return args_status;
    }

    const auto [i32, i32_lossy] = num_matcher.get().as_i32();
    const auto code_point = char32_t(i32);
    if (i32_lossy || i32 < 0 || !is_scalar_value(code_point)) {
        constexpr bool to_upper = true;
        constexpr int base = 16;
        const auto num_string = to_u8string(num_matcher.get(), base, to_upper);
        context.try_error(
            diagnostic::char_nonscalar, num_matcher.get_location(),
            joined_char_sequence(
                {
                    u8"The computed code point U"sv,
                    num_string.starts_with(u8'-') ? u8""sv : u8"+"sv,
                    num_string,
                    u8" is not a Unicode scalar value. "sv,
                    u8"Therefore, it cannot be encoded as UTF-8."sv,
                }
            )
        );
        return Processing_Status::error;
    }

    return code_point;
}

[[nodiscard]]
Result<char32_t, Processing_Status>
Char_By_Name_Behavior::get_code_point(const Invocation& call, Context& context) const
{
    constexpr auto error_point = char32_t(-1);

    String_Matcher name_matcher { context.get_transient_memory() };
    Parameter name_param { u8"name", Optionality::mandatory, name_matcher };
    Parameter* const parameters[] { &name_param };

    const auto args_status = match_call(parameters, call, context);
    if (args_status != Processing_Status::ok) {
        return args_status;
    }

    const char32_t code_point = code_point_by_name(name_matcher.get());
    if (code_point == error_point) {
        const std::u8string_view message[] {
            u8"Expected an (all caps) name of a Unicode code point, but got \"",
            name_matcher.get(),
            u8"\".",
        };
        context.try_error(
            diagnostic::char_name, name_matcher.get_location(), joined_char_sequence(message)
        );
        return Processing_Status::error;
    }

    return code_point;
}

Result<Big_Int, Processing_Status>
Char_Get_Num_Behavior::do_evaluate(const Invocation& call, Context& context) const
{
    String_Matcher x_matcher { context.get_transient_memory() };
    Parameter x_parameter { u8"x", Optionality::mandatory, x_matcher };
    Parameter* const parameters[] { &x_parameter };

    const auto args_status = match_call(parameters, call, context);
    if (args_status != Processing_Status::ok) {
        return args_status;
    }

    const std::u8string_view input_text = x_matcher.get();

    if (input_text.empty()) {
        context.try_error(
            diagnostic::char_blank, call.directive.get_source_span(),
            u8"Nothing can be generated because the input is empty."sv
        );
        return Processing_Status::error;
    }

    const std::expected<utf8::Code_Point_And_Length, utf8::Unicode_Error> decode_result
        = utf8::decode_and_length(input_text);
    if (!decode_result) {
        context.try_error(
            diagnostic::char_corrupted, call.directive.get_source_span(),
            u8"The input could not be interpreted as a code point because it is malformed UTF-8."sv
        );
        return Processing_Status::error;
    }

    const auto [code_point, length] = *decode_result;
    if (std::size_t(length) != input_text.size()) {
        COWEL_ASSERT(std::size_t(length) <= input_text.size());
        context.try_warning(
            diagnostic::ignored_input, call.directive.get_source_span(),
            u8"Some of the code units inside were ignored because only the first given code point "
            u8"is converted. "
            u8"This can happen if you type a glyph that consist of multiple "
            u8"code points, like a country flag."sv
        );
    }

    return Big_Int { Int128 { code_point } };
}

} // namespace cowel
