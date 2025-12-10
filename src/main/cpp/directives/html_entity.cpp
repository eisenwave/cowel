#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/char_sequence_ops.hpp"
#include "cowel/util/chars.hpp"
#include "cowel/util/from_chars.hpp"
#include "cowel/util/html_entities.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"
#include "cowel/value.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

[[nodiscard]]
std::u32string_view as_string_view(const std::array<char32_t, 2>& array)
{
    const std::size_t length = array[0] == 0 ? 0 : array[1] == 0 ? 1 : 2;
    return { array.data(), length };
}

[[nodiscard]]
std::array<char32_t, 2> get_code_points_from_digits(
    std::u8string_view digits,
    int base,
    const Invocation& call,
    Context& context
)
{
    const std::optional<std::uint32_t> value = from_characters<std::uint32_t>(digits, base);
    if (!value) {
        const std::u8string_view message = base == 10
            ? u8"Expected a sequence of decimal digits."sv
            : u8"Expected a sequence of hexadecimal digits."sv;
        context.try_error(diagnostic::char_digits, call.directive.get_source_span(), message);
        return {};
    }

    const auto code_point = char32_t(*value);
    if (!is_scalar_value(code_point)) {
        context.try_error(
            diagnostic::char_nonscalar, call.directive.get_source_span(),
            u8"The given hex sequence is not a Unicode scalar value. "
            u8"Therefore, it cannot be encoded as UTF-8."sv
        );
        return {};
    }

    return { code_point };
}

[[nodiscard]]
std::array<char32_t, 2>
get_code_points(std::u8string_view trimmed_text, const Invocation& call, Context& context)
{
    if (trimmed_text.empty()) {
        context.try_error(
            diagnostic::char_blank, call.directive.get_source_span(),
            u8"Expected an HTML character reference, but got a blank string."sv
        );
        return {};
    }
    if (trimmed_text[0] == u8'#') {
        const int base
            = trimmed_text.starts_with(u8"#x") || trimmed_text.starts_with(u8"#X") ? 16 : 10;
        return get_code_points_from_digits(trimmed_text.substr(2), base, call, context);
    }
    const std::array<char32_t, 2> result = code_points_by_character_reference_name(trimmed_text);
    if (result[0] == 0) {
        context.try_error(
            diagnostic::char_name, call.directive.get_source_span(),
            u8"Invalid named HTML character."sv
        );
    }
    return result;
}

} // namespace

Result<Short_String_Value, Processing_Status>
Char_By_Entity_Behavior::do_evaluate(const Invocation& call, Context& context) const
{
    const auto match_status = match_empty_arguments(call, context);
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    std::pmr::vector<char8_t> data { context.get_transient_memory() };
    const auto input_status
        = splice_to_plaintext(data, call.get_content_span(), call.content_frame, context);
    if (input_status != Processing_Status::ok) {
        return input_status;
    }

    const std::u8string_view trimmed_text = trim_ascii_blank(as_u8string_view(data));
    const std::array<char32_t, 2> code_points = get_code_points(trimmed_text, call, context);
    if (code_points[0] == 0) {
        // We don't need to print an error here;
        // get_code_points should have done that.
        return Processing_Status::error;
    }
    return to_static_string<Short_String_Value::max_size_v>(
        make_char_sequence(as_string_view(code_points))
    );
}

} // namespace cowel
