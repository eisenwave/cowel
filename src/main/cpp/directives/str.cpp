#include <string_view>

#include "cowel/util/case_transform.hpp"
#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_ops.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/unicode.hpp"

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

/// @brief Replaces all occurrences of `needle` within `str` with `with`.
/// The resulting string is appended to `out`.
/// @param str The string in which contents are to be replaced.
/// @param needle The string which is searched for within `str`.
/// @param with The replacement string.
/// @param max_replacements The (inclusive) maximum amount of replacements.
/// @returns The amount of replacements that have taken place,
/// also including identity replacements (where `needle` equals `with`).
/// The result is less than or equal to `max_replacements`.
std::size_t replace_all(
    std::pmr::vector<char8_t>& out,
    const std::u8string_view str,
    const std::u8string_view needle,
    const std::u8string_view with,
    const std::size_t max_replacements = std::size_t(-1)
)
{
    if (max_replacements == 0) [[unlikely]] {
        out.insert(out.end(), str.begin(), str.end());
        return 0;
    }

    std::size_t replacements = 0;
    const auto append_replacement = [&] {
        out.insert(out.end(), with.data(), with.data() + with.size());
        ++replacements;
    };

    // In the unlikely event that the given needle is empty,
    // there are theoretically infinitely many matches inside `str`,
    // even if `str` is empty.
    // We resolve this problem like JavaScript;
    // that is, each replacement results in at least one character of progress.
    //
    // Unfortunately, this also forces Unicode decoding because advancing
    // by a single code unit may slice code points in half.
    if (needle.empty()) [[unlikely]] {
        append_replacement();
        std::u8string_view remainder = str;
        while (!remainder.empty() && replacements < max_replacements) {
            const auto [code_point, length] = utf8::decode_and_length_or_replacement(remainder);
            const std::u8string_view character = remainder.substr(0, std::size_t(length));
            out.insert(out.end(), character.data(), character.data() + character.size());
            append_replacement();
            remainder.remove_prefix(std::size_t(length));
        }
        out.insert(out.end(), remainder.data(), remainder.data() + remainder.length());
        return replacements;
    }

    for (std::size_t pos = 0; pos < str.size();) {
        const std::size_t next = str.find(needle, pos);
        if (replacements >= max_replacements || next == std::u8string_view::npos) {
            out.insert(out.end(), str.data() + pos, str.data() + str.size());
            break;
        }
        out.insert(out.end(), str.data() + pos, str.data() + next);
        append_replacement();
        pos = next + needle.size();
    }

    COWEL_ASSERT(replacements <= max_replacements);
    return replacements;
}

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
    const Reg_Exp_Status status = regex.match(text);
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

Result<bool, Processing_Status>
Str_Contains_Behavior::do_evaluate(const Invocation& call, Context& context) const
{
    static const auto needle_type = Type::canonical_union_of({ Type::str, Type::regex });
    String_Matcher text_matcher { context.get_transient_memory() };
    Group_Member_Matcher text_member { u8"text", Optionality::mandatory, text_matcher };
    Value_Of_Type_Matcher needle_matcher { &needle_type };
    Group_Member_Matcher needle_member { u8"needle", Optionality::mandatory, needle_matcher };
    Group_Member_Matcher* const matchers[] { &text_member, &needle_member };
    Pack_Usual_Matcher args_matcher { matchers };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto args_status = call_matcher.match_call(call, context, make_fail_callback());
    if (args_status != Processing_Status::ok) {
        return args_status;
    }

    const std::u8string_view text = text_matcher.get();
    const Value& needle = needle_matcher.get();
    if (needle.is_str()) {
        return text.contains(needle.as_string());
    }
    COWEL_ASSERT(needle.is_regex());

    const Reg_Exp& regex = needle.as_regex();
    const Reg_Exp_Status status = regex.search(text).status;
    switch (status) {
    case Reg_Exp_Status::unmatched: return false;
    case Reg_Exp_Status::matched: return true;
    case Reg_Exp_Status::invalid: COWEL_ASSERT_UNREACHABLE(u8"Developer error.");
    case Reg_Exp_Status::execution_error: {
        context.try_error(
            diagnostic::regex_execution, needle_matcher.get_location(),
            u8"The given regular expression is valid, "
            u8"but its execution failed (too expensive, or due to an internal error)."sv
        );
        return Processing_Status::error;
    }
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid status.");
}

Result<Value, Processing_Status>
Str_Replace_Behavior::evaluate(const Invocation& call, Context& context) const
{
    static const auto needle_type = Type::canonical_union_of({ Type::str, Type::regex });
    String_Matcher text_matcher { context.get_transient_memory() };
    Group_Member_Matcher text_member { u8"text", Optionality::mandatory, text_matcher };
    Value_Of_Type_Matcher needle_matcher { &needle_type };
    Group_Member_Matcher needle_member { u8"needle", Optionality::mandatory, needle_matcher };
    String_Matcher with_matcher { context.get_transient_memory() };
    Group_Member_Matcher with_member { u8"with", Optionality::mandatory, with_matcher };
    Group_Member_Matcher* const matchers[] { &text_member, &needle_member, &with_member };
    Pack_Usual_Matcher args_matcher { matchers };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto args_status = call_matcher.match_call(call, context, make_fail_callback());
    if (args_status != Processing_Status::ok) {
        return args_status;
    }

    const std::u8string_view text_string = text_matcher.get();
    const std::u8string_view with_string = with_matcher.get();
    const String_Kind text_string_kind = text_matcher.get_string_kind();

    std::pmr::vector<char8_t> out { context.get_transient_memory() };
    const bool is_result_ascii = text_string_kind == String_Kind::ascii
        && with_matcher.get_string_kind() == String_Kind::ascii;
    const auto out_string_kind = is_result_ascii ? String_Kind::ascii : String_Kind::unknown;

    const Value& needle = needle_matcher.get();
    out.reserve(text_string.size() * 2);
    if (needle.is_str()) {
        const std::u8string_view needle_string = needle.as_string();
        const auto max_replacements = m_kind == Str_Replacement_Kind::first ? 1uz : std::size_t(-1);
        const std::size_t replacement_count
            = replace_all(out, text_string, needle_string, with_string, max_replacements);
        if (replacement_count == 0) {
            return Value::string(text_string, text_string_kind);
        }
    }
    else {
        COWEL_ASSERT(needle.is_regex());
        const Reg_Exp& regex = needle.as_regex();
        const auto status = [&] -> Reg_Exp_Status { //
            switch (m_kind) {
            case Str_Replacement_Kind::first: {
                const auto [s, match] = regex.search(text_string);
                if (s == Reg_Exp_Status::matched) {
                    out.insert(out.end(), text_string.data(), text_string.data() + match.index);
                    out.insert(
                        out.end(), with_string.data(), with_string.data() + with_string.size()
                    );
                    out.insert(
                        out.end(), text_string.data() + match.index + match.length,
                        text_string.data() + text_string.size()
                    );
                }
                return s;
            }
            case Str_Replacement_Kind::all: {
                return regex.replace_all(out, text_string, with_string);
            }
            }
            COWEL_ASSERT_UNREACHABLE(u8"Invalid kind.");
        }();

        switch (status) {
        case Reg_Exp_Status::unmatched: return Value::string(text_string, text_string_kind);
        case Reg_Exp_Status::matched: break;
        case Reg_Exp_Status::invalid: COWEL_ASSERT_UNREACHABLE(u8"Developer error.");
        case Reg_Exp_Status::execution_error: {
            context.try_error(
                diagnostic::regex_execution, needle_matcher.get_location(),
                u8"The given regular expression is valid, "
                u8"but its execution failed (too expensive, or due to an internal error)."sv
            );
            return Processing_Status::error;
        }
        }
    }

    const auto out_string = as_u8string_view(out);
    return Value::string(out_string, out_string_kind);
}

Result<Value, Processing_Status>
Regex_Make_Behavior::evaluate(const Invocation& call, Context& context) const
{
    String_Matcher pattern_matcher { context.get_transient_memory() };
    Group_Member_Matcher pattern_member { u8"pattern", Optionality::mandatory, pattern_matcher };
    String_Matcher flags_matcher { context.get_transient_memory() };
    Group_Member_Matcher flags_member { u8"flags", Optionality::optional, flags_matcher };
    Group_Member_Matcher* const matchers[] { &pattern_member, &flags_member };
    Pack_Usual_Matcher args_matcher { matchers };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto args_status = call_matcher.match_call(call, context, make_fail_callback());
    if (args_status != Processing_Status::ok) {
        return args_status;
    }

    const std::u8string_view pattern = pattern_matcher.get();
    const std::u8string_view flags = flags_matcher.get_or_default(u8""sv);

    const Result<Reg_Exp_Flags, Reg_Exp_Flags_Error> parsed_flags = reg_exp_flags_parse(flags);
    if (!parsed_flags) {
        const auto error = parsed_flags.error();
        const std::u8string_view bad_flag = flags.substr(error.index, error.length);
        context.try_error(
            diagnostic::regex_flags, flags_matcher.get_location(),
            error.kind == Reg_Exp_Flags_Error_Kind::invalid
                ? joined_char_sequence({ u8"The flag \""sv, bad_flag, u8"\" is not valid."sv })
                : joined_char_sequence({ u8"Duplicate flag \""sv, bad_flag, u8"\"."sv })
        );
        return Processing_Status::error;
    }

    Result<Reg_Exp, Reg_Exp_Error_Code> regex = Reg_Exp::make(pattern, *parsed_flags);
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
