#include <string_view>

#include "cowel/util/case_transform.hpp"
#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_ops.hpp"
#include "cowel/util/result.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/fwd.hpp"
#include "cowel/parameters.hpp"

namespace cowel {
namespace {

struct Sink_For_Evaluation final : String_Sink {
    std::pmr::vector<char8_t> m_text;

    void reserve(std::size_t amount) override
    {
        m_text.reserve(amount);
    }
    void consume(std::pmr::vector<char8_t>&& text) override
    {
        if (m_text.empty()) {
            m_text = std::move(text);
        }
        else {
            m_text.insert(m_text.end(), text.begin(), text.end());
        }
    }
    void consume(Char_Sequence8 chars) override
    {
        append(m_text, chars);
    }
};

struct Sink_For_Splicing final : String_Sink {
    Content_Policy& out;
    explicit Sink_For_Splicing(Content_Policy& out)
        : out { out }
    {
    }
    // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
    void consume(std::pmr::vector<char8_t>&& text) override
    {
        out.write(as_u8string_view(text), Output_Language::text);
    }
    void consume(Char_Sequence8 chars) override
    {
        out.write(chars, Output_Language::text);
    }
};

} // namespace

Result<Value, Processing_Status>
String_Sink_Behavior::evaluate(const Invocation& call, Context& context) const
{
    Sink_For_Evaluation sink;
    const Processing_Status result = do_evaluate(sink, call, context);
    if (result != Processing_Status::ok) {
        return result;
    }
    return Value::dynamic_string(std::move(sink.m_text));
}

Processing_Status
String_Sink_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    Sink_For_Splicing sink { out };
    return do_evaluate(sink, call, context);
}

Processing_Status Str_Transform_Behavior::do_evaluate(
    String_Sink& out,
    const Invocation& call,
    Context& context
) const
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

    if (!result.empty()) {
        out.consume(std::move(result));
    }
    return Processing_Status::ok;
}

} // namespace cowel
