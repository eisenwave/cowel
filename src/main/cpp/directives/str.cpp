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

[[nodiscard]]
Result<Big_Int, Processing_Status>
Str_Length_Behavior::do_evaluate(const Invocation& call, Context& context) const
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

    const std::u8string_view x = x_matcher.get();
    if (m_kind == Str_Length_Kind::utf8 || x_matcher.get_string_kind() == String_Kind::ascii) {
        return Big_Int { Int128 { x.length() } };
    }

    COWEL_DEBUG_ASSERT(m_kind == Str_Length_Kind::code_point);
    return Big_Int { Int128 { utf8::count_code_points_or_replacement(x) } };
}

Result<Value, Processing_Status>
String_Sink_Behavior::evaluate(const Invocation& call, Context& context) const
{
    Sink_For_Evaluation sink;
    const Processing_Status result = do_evaluate(sink, call, context);
    if (result != Processing_Status::ok) {
        return result;
    }
    // TODO: String_Kind::unicode is needlessly pessimistic here.
    //       We could alter String_Sink so it receives the String_Kind and keeps track of it.
    return Value::string(as_u8string_view(sink.m_text), String_Kind::unknown);
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

Result<bool, Processing_Status>
Str_Match_Behavior::do_evaluate(const Invocation& call, Context& context) const
{
    String_Matcher text_matcher { context.get_transient_memory() };
    Group_Member_Matcher text_member { u8"text", Optionality::mandatory, text_matcher };
    Value_Of_Type_Matcher regex_matcher { &Type::regex };
    Group_Member_Matcher regex_member { u8"regex", Optionality::mandatory, regex_matcher };
    Group_Member_Matcher* const matchers[] { &text_member, &regex_member };
    Pack_Usual_Matcher args_matcher { matchers };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto args_status = call_matcher.match_call(call, context, make_fail_callback());
    if (args_status != Processing_Status::ok) {
        return args_status;
    }

    const std::u8string_view text = text_matcher.get();
    const Reg_Exp& regex = regex_matcher.get().as_regex();
    const Reg_Exp_Status status = regex.test(text);
    switch (status) {
    case Reg_Exp_Status::unmatched: return false;
    case Reg_Exp_Status::matched: return true;
    case Reg_Exp_Status::invalid: COWEL_ASSERT_UNREACHABLE(u8"Developer error.");
    case Reg_Exp_Status::execution_error: {
        context.try_error(
            diagnostic::regex_execution, regex_matcher.get_location(),
            u8"The given regular expression is valid, "
            u8"but its execution failed (too expensive, or due to an internal error)."sv
        );
        return Processing_Status::error;
    }
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid status.");
}

Result<Value, Processing_Status>
Regex_Make_Behavior::evaluate(const Invocation& call, Context& context) const
{
    String_Matcher pattern_matcher { context.get_transient_memory() };
    Group_Member_Matcher pattern_member { u8"pattern", Optionality::mandatory, pattern_matcher };
    Group_Member_Matcher* const matchers[] { &pattern_member };
    Pack_Usual_Matcher args_matcher { matchers };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto args_status = call_matcher.match_call(call, context, make_fail_callback());
    if (args_status != Processing_Status::ok) {
        return args_status;
    }

    const std::u8string_view pattern = pattern_matcher.get();
    Result<Reg_Exp, Reg_Exp_Error_Code> regex = Reg_Exp::make(pattern);
    if (!regex) {
        context.try_error(
            diagnostic::regex_pattern, pattern_matcher.get_location(),
            joined_char_sequence(
                {
                    u8"The provided pattern \""sv,
                    pattern,
                    u8"\" is not a valid regular expression."sv,
                }
            )
        );
        return Processing_Status::error;
    }

    return Value::regex(std::move(*regex));
}

} // namespace cowel
