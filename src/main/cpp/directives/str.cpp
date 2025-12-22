#include <string_view>

#include "cowel/util/case_transform.hpp"
#include "cowel/util/result.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/fwd.hpp"
#include "cowel/parameters.hpp"

namespace cowel {

Result<Value, Processing_Status>
Str_Transform_Behavior::evaluate(const Invocation& call, Context& context) const
{
    String_Matcher x_matcher { context.get_transient_memory() };
    Group_Member_Matcher x_member { u8"x", Optionality::mandatory, x_matcher };
    Group_Member_Matcher* matchers[] { &x_member };
    Pack_Usual_Matcher args_matcher { matchers };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto args_status = call_matcher.match_call(call, context, make_fail_callback());
    if (args_status != Processing_Status::ok) {
        return args_status;
    }

    const std::u8string_view input = x_matcher.get();
    std::pmr::vector<char8_t> result { context.get_transient_memory() };
    result.reserve(input.length() * 3 / 2);

    for (std::size_t i = 0; i < input.length();) {
        const auto [input_point, input_length]
            = utf8::decode_and_length_or_replacement(input.substr(i));
        COWEL_ASSERT(input_length != 0);

        const std::u32string_view transformed = m_transform == Text_Transformation::uppercase
            ? unconditional_to_upper(input_point)
            : unconditional_to_lower(input_point);
        if (transformed.empty()) {
            append(result, input.substr(i, std::size_t(input_length)));
        }
        else {
            for (const char32_t t : transformed) {
                const utf8::Code_Units_And_Length output = utf8::encode8_unchecked(t);
                append(result, output.as_string());
            }
        }

        i += std::size_t(input_length);
    }

    return Value::dynamic_string(std::move(result));
}

} // namespace cowel
