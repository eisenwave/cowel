#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/char_sequence_ops.hpp"
#include "cowel/util/chars.hpp"
#include "cowel/util/from_chars.hpp"
#include "cowel/util/html_entities.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"
#include "cowel/parameters.hpp"
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
get_code_points(const std::u8string_view text, const Invocation& call, Context& context)
{
    if (trim_ascii_blank(text).empty()) {
        context.try_error(
            diagnostic::char_blank, call.directive.get_source_span(),
            u8"Expected an HTML character reference, but got a blank string."sv
        );
        return {};
    }
    if (text[0] == u8'#') {
        const int base = text.starts_with(u8"#x") || text.starts_with(u8"#X") ? 16 : 10;
        return get_code_points_from_digits(text.substr(2), base, call, context);
    }
    const std::array<char32_t, 2> result = code_points_by_character_reference_name(text);
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
    String_Matcher name_matcher { context.get_transient_memory() };
    Group_Member_Matcher name_member { u8"name", Optionality::mandatory, name_matcher };
    Group_Member_Matcher* matchers[] { &name_member };
    Pack_Usual_Matcher args_matcher { matchers };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto args_status = call_matcher.match_call(call, context, make_fail_callback());
    if (args_status != Processing_Status::ok) {
        return args_status;
    }

    const std::u8string_view name = name_matcher.get();
    const std::array<char32_t, 2> code_points = get_code_points(name, call, context);
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
