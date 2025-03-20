#include <algorithm>
#include <bitset>
#include <cstddef>
#include <memory_resource>
#include <optional>
#include <string_view>
#include <vector>

#include "mmml/util/annotation_span.hpp"
#include "mmml/util/assert.hpp"
#include "mmml/util/chars.hpp"
#include "mmml/util/unicode.hpp"

#include "mmml/fwd.hpp"
#include "mmml/parse_utils.hpp"

#include "mmml/highlight/cpp.hpp"
#include "mmml/highlight/highlight.hpp"

namespace mmml {

namespace cpp {

std::size_t match_whitespace(std::u8string_view str)
{
    constexpr auto predicate = [](char8_t c) { return is_cpp_whitespace(c); };
    return std::size_t(std::ranges::find_if_not(str, predicate) - str.begin());
}

std::size_t match_non_whitespace(std::u8string_view str)
{
    constexpr auto predicate = [](char8_t c) { return is_cpp_whitespace(c); };
    return std::size_t(std::ranges::find_if_not(str, predicate) - str.begin());
}

namespace {

[[nodiscard]]
std::size_t match_newline_escape(std::u8string_view str)
{
    // https://eel.is/c++draft/lex.phases#1.2
    // > Each sequence of a backslash character (\)
    // > immediately followed by zero or more whitespace characters other than new-line
    // > followed by a new-line character is deleted,
    // > splicing physical source lines to form logical source lines.

    if (!str.starts_with(u8'\\')) {
        return 0;
    }
    std::size_t length = 1;
    for (; length < str.length(); ++length) {
        if (str[length] == u8'\n') {
            return length + 1;
        }
        if (!is_cpp_whitespace(str[length])) {
            return 0;
        }
    }
    return 0;
}

enum struct Special_Line_Type : Default_Underlying {
    comment,
    preprocessing,
};

Comment_Result match_special_line(std::u8string_view s, Special_Line_Type type) noexcept
{
    switch (type) {
    case Special_Line_Type::comment: {
        if (!s.starts_with(u8"//")) {
            return {};
        }
        break;
    }
    case Special_Line_Type::preprocessing: {
        const std::optional<Cpp_Token_Type> first_token = match_preprocessing_op_or_punc(s);
        if (first_token != Cpp_Token_Type::pound && first_token != Cpp_Token_Type::pound_alt) {
            return {};
        }
        break;
    }
    }

    std::size_t length = 2;

    while (length < s.length()) {
        if (s[length] == u8'\n') {
            return Comment_Result { .length = length + 1, .is_terminated = true };
        }
        if (const std::size_t escape = match_newline_escape(s.substr(length))) {
            length += escape;
        }
        else {
            ++length;
        }
    }
    return Comment_Result { .length = length, .is_terminated = false };
}

} // namespace

Comment_Result match_line_comment(std::u8string_view s) noexcept
{
    return match_special_line(s, Special_Line_Type::comment);
}

Comment_Result match_block_comment(std::u8string_view s) noexcept
{
    if (!s.starts_with(u8"/*")) {
        return {};
    }
    // naive: nesting disallowed, but line comments can be nested in block comments
    const std::size_t end = s.find(u8"*/", 2);
    if (end == std::string_view::npos) {
        return Comment_Result { .length = s.length(), .is_terminated = false };
    }
    return Comment_Result { .length = end + 2, .is_terminated = true };
}

Comment_Result match_preprocessing_line(std::u8string_view s) noexcept
{
    return match_special_line(s, Special_Line_Type::preprocessing);
}

Literal_Match_Result match_integer_literal // NOLINT(bugprone-exception-escape)
    (std::u8string_view s) noexcept
{
    if (s.empty() || !is_ascii_digit(s[0])) {
        return { Literal_Match_Status::no_digits, 0, {} };
    }
    if (s.starts_with(u8"0b")) {
        const std::size_t digits = match_digits(s.substr(2), 2);
        if (digits == 0) {
            return { Literal_Match_Status::no_digits_following_prefix, 2,
                     Integer_Literal_Type::binary };
        }
        return { Literal_Match_Status::ok, digits + 2, Integer_Literal_Type::binary };
    }
    if (s.starts_with(u8"0x")) {
        const std::size_t digits = match_digits(s.substr(2), 16);
        if (digits == 0) {
            return { Literal_Match_Status::no_digits_following_prefix, 2,
                     Integer_Literal_Type::hexadecimal };
        }
        return { Literal_Match_Status::ok, digits + 2, Integer_Literal_Type::hexadecimal };
    }
    if (s[0] == '0') {
        const std::size_t digits = match_digits(s, 8);
        return { Literal_Match_Status::ok, digits,
                 digits == 1 ? Integer_Literal_Type::decimal : Integer_Literal_Type::octal };
    }
    const std::size_t digits = match_digits(s, 10);

    return { Literal_Match_Status::ok, digits, Integer_Literal_Type::decimal };
}

namespace {

[[nodiscard]]
bool is_identifier_start_likely_ascii(char32_t c)
{
    if (is_ascii(c)) [[likely]] {
        return is_cpp_ascii_identifier_start(c);
    }
    return is_cpp_identifier_start(c);
}

[[nodiscard]]
bool is_identifier_continue_likely_ascii(char32_t c)
{
    if (is_ascii(c)) [[likely]] {
        return is_cpp_ascii_identifier_continue(c);
    }
    return is_cpp_identifier_continue(c);
}

} // namespace

std::size_t match_pp_number(const std::u8string_view str)
{
    std::size_t length = 0;
    // "." digit
    if (str.length() >= 2 && str[0] == u8'.' && is_ascii_digit(str[1])) {
        length += 2;
    }
    // digit
    else if (!str.empty() && is_ascii_digit(str[0])) {
        length += 1;
    }
    else {
        return length;
    }

    while (length < str.size()) {
        switch (str[length]) {
        // pp-number "'" digit
        // pp-number "'" nondigit
        case u8'\'': {
            if (length + 1 < str.size() && is_cpp_ascii_identifier_continue(str[length + 1])) {
                length += 2;
            }
            break;
        }
        // pp-number "e" sign
        case u8'e':
        // pp-number "E" sign
        case u8'E':
        // pp-number "p" sign
        case u8'p':
        // pp-number "P" sign
        case u8'P': {
            if (length + 1 < str.size() && (str[length + 1] == u8'-' || str[length + 1] == u8'+')) {
                length += 2;
            }
            break;
        }
        // pp-number "."
        case u8'.': {
            ++length;
            break;
        }
        // pp-number identifier-continue
        default: {
            const std::u8string_view remainder = str.substr(length);
            const auto [code_point, units] = utf8::decode_and_length_or_throw(remainder);
            if (is_identifier_continue_likely_ascii(code_point)) {
                length += std::size_t(units);
                break;
            }
            return length;
        }
        }
    }

    return length;
}

std::size_t match_identifier(std::u8string_view str)
{
    std::size_t length = 0;

    if (!str.empty()) {
        const auto [code_point, units] = utf8::decode_and_length_or_throw(str);
        if (!is_identifier_start_likely_ascii(code_point)) {
            return length;
        }
        str.remove_prefix(std::size_t(units));
        length += std::size_t(units);
    }

    while (!str.empty()) {
        const auto [code_point, units] = utf8::decode_and_length_or_throw(str);
        if (!is_identifier_continue_likely_ascii(code_point)) {
            return length;
        }
        str.remove_prefix(std::size_t(units));
        length += std::size_t(units);
    }

    return length;
}

Character_Literal_Result match_character_literal(std::u8string_view str)
{
    std::size_t length = 0;
    if (str.starts_with(u8"u8")) {
        length += 2;
    }
    else if (str.starts_with(u8'u') || str.starts_with(u8'U') || str.starts_with(u8'L')) {
        length += 1;
    }
    const std::size_t encoding_prefix_length = length;

    if (length >= str.size() || str[length] != u8'\'') {
        return {};
    }
    ++length;
    while (length < str.size()) {
        const std::u8string_view remainder = str.substr(length);
        const auto [code_point, units] = utf8::decode_and_length_or_throw(remainder);
        switch (code_point) {
        case '\'': {
            return { .length = length + 1,
                     .encoding_prefix_length = encoding_prefix_length,
                     .terminated = true };
        }
        case U'\\': {
            length += std::size_t(units) + 1;
            break;
        }
        case U'\n':
            return { .length = length,
                     .encoding_prefix_length = encoding_prefix_length,
                     .terminated = false };
        default: {
            length += std::size_t(units);
            break;
        }
        }
    }

    return { .length = length,
             .encoding_prefix_length = encoding_prefix_length,
             .terminated = false };
}

namespace {

[[nodiscard]]
constexpr bool is_d_char(char8_t c)
{
    return is_ascii(c) && !is_ascii_blank(c) && c != u8'(' && c != u8')' && c != '\\';
}

[[nodiscard]]
std::size_t match_d_char_sequence(std::u8string_view str)
{
    constexpr auto predicate = [](char8_t c) { return is_d_char(c); };
    return std::size_t(std::ranges::find_if_not(str, predicate) - str.begin());
}

} // namespace

String_Literal_Result match_string_literal(std::u8string_view str)
{
    std::size_t length = 0;
    const auto expect = [&](char8_t c) {
        if (length < str.length() && str[length] == c) {
            ++length;
            return true;
        }
        return false;
    };

    if (str.starts_with(u8"u8")) {
        length += 2;
    }
    else {
        expect(u8'u') || expect(u8'U') || expect(u8'L');
    }
    const std::size_t encoding_prefix_length = length;

    if (length >= str.size()) {
        return {};
    }
    const bool raw = expect(u8'R');
    if (!expect(u8'"')) {
        return {};
    }
    if (raw) {
        const std::size_t d_char_sequence_length = match_d_char_sequence(str.substr(length));
        const std::u8string_view d_char_sequence = str.substr(length, d_char_sequence_length);
        length += d_char_sequence_length;

        if (!expect(u8'(')) {
            return {};
        }
        for (; length < str.size(); ++length) {
            if (str[length] == u8')' //
                && str.substr(1).starts_with(d_char_sequence) //
                && str.substr(1 + d_char_sequence_length).starts_with(u8'"')) {
                return { .length = length + d_char_sequence_length + 2,
                         .encoding_prefix_length = encoding_prefix_length,
                         .raw = true,
                         .terminated = true };
            }
        }
    }
    else {
        while (length < str.size()) {
            const std::u8string_view remainder = str.substr(length);
            const auto [code_point, units] = utf8::decode_and_length_or_throw(remainder);
            switch (code_point) {
            case '\"': {
                return { .length = length + 1,
                         .encoding_prefix_length = encoding_prefix_length,
                         .raw = raw,
                         .terminated = true };
            }
            case U'\\': {
                length += std::size_t(units) + 1;
                break;
            }
            case U'\n':
                return { .length = length,
                         .encoding_prefix_length = encoding_prefix_length,
                         .raw = raw,
                         .terminated = false };
            default: {
                length += std::size_t(units);
                break;
            }
            }
        }
    }

    return { .length = length,
             .encoding_prefix_length = encoding_prefix_length,
             .raw = raw,
             .terminated = false };
}

std::optional<Cpp_Token_Type> match_preprocessing_op_or_punc(std::u8string_view str)
{
    using enum Cpp_Token_Type;
    if (str.empty()) {
        return {};
    }
    switch (str[0]) {
    case u8'#': return str.starts_with(u8"##") ? pound_pound : pound;
    case u8'%':
        return str.starts_with(u8"%:%:") ? pound_pound_alt
            : str.starts_with(u8"%:")    ? pound_alt
            : str.starts_with(u8"%=")    ? percent_eq
            : str.starts_with(u8"%>")    ? right_square_alt
                                         : percent;
    case u8'{': return left_brace;
    case u8'}': return right_brace;
    case u8'[': return left_square;
    case u8']': return right_square;
    case u8'(': return left_parens;
    case u8')': return right_parens;
    case u8'<': {
        // https://eel.is/c++draft/lex.pptoken#4.2
        if (str.starts_with(u8"<::") && !str.starts_with(u8"<:::") && !str.starts_with(u8"<::>")) {
            return less;
        }
        return str.starts_with(u8"<=>") ? three_way
            : str.starts_with(u8"<<=")  ? less_less_eq
            : str.starts_with(u8"<=")   ? less_eq
            : str.starts_with(u8"<<")   ? less_less
            : str.starts_with(u8"<%")   ? left_brace_alt
            : str.starts_with(u8"<:")   ? left_square_alt
                                        : less;
    }
    case u8';': return semicolon;
    case u8':':
        return str.starts_with(u8":>") ? right_square_alt //
            : str.starts_with(u8"::")  ? scope
                                       : colon;
    case u8'.':
        return str.starts_with(u8"...") ? ellipsis
            : str.starts_with(u8".*")   ? member_pointer_access
                                        : dot;
    case u8'?': return question;
    case u8'-': {
        return str.starts_with(u8"->*") ? member_arrow_access
            : str.starts_with(u8"-=")   ? minus_eq
            : str.starts_with(u8"->")   ? arrow
            : str.starts_with(u8"--")   ? minus_minus
                                        : minus;
    }
    case u8'>':
        return str.starts_with(u8">>=") ? greater_greater_eq
            : str.starts_with(u8">=")   ? greater_eq
            : str.starts_with(u8">>")   ? greater_greater
                                        : greater;
    case u8'~': return tilde;
    case u8'!': return str.starts_with(u8"!=") ? exclamation_eq : exclamation;
    case u8'+':
        return str.starts_with(u8"++") ? plus_plus //
            : str.starts_with(u8"+=")  ? plus_eq
                                       : plus;

    case u8'*': return str.starts_with(u8"*=") ? asterisk_eq : asterisk;
    case u8'/': return str.starts_with(u8"/=") ? slash_eq : slash;
    case u8'^':
        return str.starts_with(u8"^^") ? caret_caret //
            : str.starts_with(u8"^=")  ? caret_eq
                                       : caret;
    case u8'&':
        return str.starts_with(u8"&=") ? amp_eq //
            : str.starts_with(u8"&&")  ? amp_amp
                                       : amp;

    case u8'|':
        return str.starts_with(u8"|=") ? pipe_eq //
            : str.starts_with(u8"||")  ? pipe_pipe
                                       : pipe;
    case u8'=': return str.starts_with(u8"==") ? eq_eq : eq;
    case u8',': return comma;
    case u8'a':
        return str.starts_with(u8"and_eq") ? keyword_and_eq
            : str.starts_with(u8"and")     ? keyword_and
                                           : std::optional<Cpp_Token_Type> {};
    case u8'o':
        return str.starts_with(u8"or_eq") ? keyword_or_eq
            : str.starts_with(u8"or")     ? keyword_or
                                          : std::optional<Cpp_Token_Type> {};
    case u8'x':
        return str.starts_with(u8"xor_eq") ? keyword_xor_eq
            : str.starts_with(u8"xor")     ? keyword_xor
                                           : std::optional<Cpp_Token_Type> {};
    case u8'n':
        return str.starts_with(u8"not_eq") ? keyword_not_eq
            : str.starts_with(u8"not")     ? keyword_not
                                           : std::optional<Cpp_Token_Type> {};
    case u8'b':
        return str.starts_with(u8"bitand") ? keyword_bitand
            : str.starts_with(u8"bitor")   ? keyword_bitor
                                           : std::optional<Cpp_Token_Type> {};
    case u8'c':
        return str.starts_with(u8"compl") ? keyword_compl //
                                          : std::optional<Cpp_Token_Type> {};
    default: return {};
    }
}

} // namespace cpp

namespace {

#define MMML_CPP_KEYWORD_ENUM_DATA(F)                                                              \
    F(intr_alignas, "_Alignas", keyword, 0)                                                        \
    F(intr_alignof, "_Alignof", keyword, 0)                                                        \
    F(intr_atomic, "_Atomic", keyword, 0)                                                          \
    F(intr_bitint, "_BitInt", keyword_type, 0)                                                     \
    F(intr_bool, "_Bool", keyword_type, 0)                                                         \
    F(intr_complex, "_Complex", keyword, 0)                                                        \
    F(intr_decimal128, "_Decimal128", keyword_type, 0)                                             \
    F(intr_decimal32, "_Decimal32", keyword_type, 0)                                               \
    F(intr_decimal64, "_Decimal64", keyword_type, 0)                                               \
    F(intr_float128, "_Float128", keyword_type, 0)                                                 \
    F(intr_float128x, "_Float128x", keyword_type, 0)                                               \
    F(intr_float16, "_Float16", keyword_type, 0)                                                   \
    F(intr_float32, "_Float32", keyword_type, 0)                                                   \
    F(intr_float32x, "_Float32x", keyword_type, 0)                                                 \
    F(intr_float64, "_Float64", keyword_type, 0)                                                   \
    F(intr_float64x, "_Float64x", keyword_type, 0)                                                 \
    F(intr_generic, "_Generic", keyword, 0)                                                        \
    F(intr_imaginary, "_Imaginary", keyword, 0)                                                    \
    F(intr_noreturn, "_Noreturn", keyword, 0)                                                      \
    F(intr_pragma, "_Pragma", keyword, 1)                                                          \
    F(intr_static_assert, "_Static_assert", keyword, 0)                                            \
    F(intr_thread_local, "_Thread_local", keyword, 0)                                              \
    F(gnu_asm, "__asm__", keyword, 0)                                                              \
    F(gnu_attribute, "__attribute__", keyword, 0)                                                  \
    F(gnu_extension, "__extension__", keyword, 0)                                                  \
    F(gnu_float128, "__float128", keyword_type, 0)                                                 \
    F(gnu_float80, "__float80", keyword_type, 0)                                                   \
    F(gnu_fp16, "__fp16", keyword_type, 0)                                                         \
    F(gnu_ibm128, "__ibm128", keyword_type, 0)                                                     \
    F(gnu_imag, "__imag__", keyword, 0)                                                            \
    F(ext_int128, "__int128", keyword_type, 0)                                                     \
    F(ext_int16, "__int16", keyword_type, 0)                                                       \
    F(ext_int256, "__int256", keyword_type, 0)                                                     \
    F(ext_int32, "__int32", keyword_type, 0)                                                       \
    F(ext_int64, "__int64", keyword_type, 0)                                                       \
    F(ext_int8, "__int8", keyword_type, 0)                                                         \
    F(gnu_label, "__label__", keyword, 0)                                                          \
    F(intel_m128, "__m128", keyword_type, 0)                                                       \
    F(intel_m128d, "__m128d", keyword_type, 0)                                                     \
    F(intel_m128i, "__m128i", keyword_type, 0)                                                     \
    F(intel_m256, "__m256", keyword_type, 0)                                                       \
    F(intel_m256d, "__m256d", keyword_type, 0)                                                     \
    F(intel_m256i, "__m256i", keyword_type, 0)                                                     \
    F(intel_m512, "__m512", keyword_type, 0)                                                       \
    F(intel_m512d, "__m512d", keyword_type, 0)                                                     \
    F(intel_m512i, "__m512i", keyword_type, 0)                                                     \
    F(intel_m64, "__m64", keyword_type, 0)                                                         \
    F(intel_mmask16, "__mmask16", keyword_type, 0)                                                 \
    F(intel_mmask32, "__mmask32", keyword_type, 0)                                                 \
    F(intel_mmask64, "__mmask64", keyword_type, 0)                                                 \
    F(intel_mmask8, "__mmask8", keyword_type, 0)                                                   \
    F(microsoft_ptr32, "__ptr32", keyword_type, 0)                                                 \
    F(microsoft_ptr64, "__ptr64", keyword_type, 0)                                                 \
    F(gnu_real, "__real__", keyword, 0)                                                            \
    F(gnu_restrict, "__restrict", keyword, 0)                                                      \
    F(kw_alignas, "alignas", keyword, 1)                                                           \
    F(kw_alignof, "alignof", keyword, 1)                                                           \
    F(kw_and, "and", keyword, 1)                                                                   \
    F(kw_and_eq, "and_eq", keyword, 1)                                                             \
    F(kw_asm, "asm", keyword_control, 1)                                                           \
    F(kw_auto, "auto", keyword, 1)                                                                 \
    F(kw_bitand, "bitand", keyword, 1)                                                             \
    F(kw_bitor, "bitor", keyword, 1)                                                               \
    F(kw_bool, "bool", keyword_constant, 1)                                                        \
    F(kw_break, "break", keyword_control, 1)                                                       \
    F(kw_case, "case", keyword_control, 1)                                                         \
    F(kw_catch, "catch", keyword_control, 1)                                                       \
    F(kw_char, "char", keyword_type, 1)                                                            \
    F(kw_char16_t, "char16_t", keyword_type, 1)                                                    \
    F(kw_char32_t, "char32_t", keyword_type, 1)                                                    \
    F(kw_char8_t, "char8_t", keyword_type, 1)                                                      \
    F(kw_class, "class", keyword, 1)                                                               \
    F(kw_co_await, "co_await", keyword_control, 1)                                                 \
    F(kw_co_return, "co_return", keyword_control, 1)                                               \
    F(kw_compl, "compl", keyword, 1)                                                               \
    F(kw_complex, "complex", keyword, 0)                                                           \
    F(kw_concept, "concept", keyword, 1)                                                           \
    F(kw_const, "const", keyword, 1)                                                               \
    F(kw_const_cast, "const_cast", keyword, 1)                                                     \
    F(kw_consteval, "consteval", keyword, 1)                                                       \
    F(kw_constexpr, "constexpr", keyword, 1)                                                       \
    F(kw_constinit, "constinit", keyword, 1)                                                       \
    F(kw_continue, "continue", keyword_control, 1)                                                 \
    F(kw_contract_assert, "contract_assert", keyword, 1)                                           \
    F(kw_decltype, "decltype", keyword, 1)                                                         \
    F(kw_default, "default", keyword, 1)                                                           \
    F(kw_delete, "delete", keyword, 1)                                                             \
    F(kw_do, "do", keyword_control, 1)                                                             \
    F(kw_double, "double", keyword_type, 1)                                                        \
    F(kw_dynamic_cast, "dynamic_cast", keyword, 1)                                                 \
    F(kw_else, "else", keyword_control, 1)                                                         \
    F(kw_enum, "enum", keyword, 1)                                                                 \
    F(kw_explicit, "explicit", keyword, 1)                                                         \
    F(kw_export, "export", keyword, 1)                                                             \
    F(kw_extern, "extern", keyword, 1)                                                             \
    F(kw_false, "false", keyword_boolean, 1)                                                       \
    F(kw_final, "final", keyword, 1)                                                               \
    F(kw_float, "float", keyword_type, 1)                                                          \
    F(kw_for, "for", keyword_control, 1)                                                           \
    F(kw_friend, "friend", keyword, 1)                                                             \
    F(kw_goto, "goto", keyword_control, 1)                                                         \
    F(kw_if, "if", keyword_control, 1)                                                             \
    F(kw_imaginary, "imaginary", keyword, 0)                                                       \
    F(kw_import, "import", keyword, 1)                                                             \
    F(kw_inline, "inline", keyword, 1)                                                             \
    F(kw_int, "int", keyword_type, 1)                                                              \
    F(kw_long, "long", keyword_type, 1)                                                            \
    F(kw_module, "module", keyword, 1)                                                             \
    F(kw_mutable, "mutable", keyword, 1)                                                           \
    F(kw_namespace, "namespace", keyword, 1)                                                       \
    F(kw_new, "new", keyword, 1)                                                                   \
    F(kw_noexcept, "noexcept", keyword, 1)                                                         \
    F(kw_noreturn, "noreturn", keyword, 0)                                                         \
    F(kw_not, "not", keyword, 1)                                                                   \
    F(kw_not_eq, "not_eq", keyword, 1)                                                             \
    F(kw_nullptr, "nullptr", keyword_constant, 1)                                                  \
    F(kw_operator, "operator", keyword, 1)                                                         \
    F(kw_or, "or", keyword, 1)                                                                     \
    F(kw_or_eq, "or_eq", keyword, 1)                                                               \
    F(kw_override, "override", keyword, 1)                                                         \
    F(kw_post, "post", keyword, 1)                                                                 \
    F(kw_pre, "pre", keyword, 1)                                                                   \
    F(kw_private, "private", keyword, 1)                                                           \
    F(kw_protected, "protected", keyword, 1)                                                       \
    F(kw_public, "public", keyword, 1)                                                             \
    F(kw_register, "register", keyword, 1)                                                         \
    F(kw_reinterpret_cast, "reinterpret_cast", keyword, 1)                                         \
    F(kw_replaceable_if_eligible, "replaceable_if_eligible", keyword, 1)                           \
    F(kw_requires, "requires", keyword, 1)                                                         \
    F(kw_restrict, "restrict", keyword, 0)                                                         \
    F(kw_return, "return", keyword_control, 1)                                                     \
    F(kw_short, "short", keyword_type, 1)                                                          \
    F(kw_signed, "signed", keyword_type, 1)                                                        \
    F(kw_sizeof, "sizeof", keyword, 1)                                                             \
    F(kw_static, "static", keyword, 1)                                                             \
    F(kw_static_assert, "static_assert", keyword, 1)                                               \
    F(kw_static_cast, "static_cast", keyword, 1)                                                   \
    F(kw_struct, "struct", keyword, 1)                                                             \
    F(kw_template, "template", keyword, 1)                                                         \
    F(kw_this, "this", keyword_constant, 1)                                                        \
    F(kw_thread_local, "thread_local", keyword, 1)                                                 \
    F(kw_throw, "throw", keyword, 1)                                                               \
    F(kw_trivially_relocatable_if_eligible, "trivially_relocatable_if_eligible", keyword, 1)       \
    F(kw_true, "true", keyword_boolean, 1)                                                         \
    F(kw_try, "try", keyword, 1)                                                                   \
    F(kw_typedef, "typedef", keyword, 1)                                                           \
    F(kw_typeid, "typeid", keyword, 1)                                                             \
    F(kw_typename, "typename", keyword, 1)                                                         \
    F(kw_typeof, "typeof", keyword, 0)                                                             \
    F(kw_typeof_unqual, "typeof_unqual", keyword, 0)                                               \
    F(kw_union, "union", keyword, 1)                                                               \
    F(kw_unsigned, "unsigned", keyword_type, 1)                                                    \
    F(kw_using, "using", keyword, 1)                                                               \
    F(kw_virtual, "virtual", keyword, 1)                                                           \
    F(kw_void, "void", keyword_type, 1)                                                            \
    F(kw_volatile, "volatile", keyword, 1)                                                         \
    F(kw_wchar_t, "wchar_t", keyword_type, 1)                                                      \
    F(kw_while, "while", keyword_control, 1)                                                       \
    F(kw_xor, "xor", keyword, 1)                                                                   \
    F(kw_xor_eq, "xor_eq", keyword, 1)

#define MMML_CPP_KEYWORD_ENUMERATOR(id, code, highlight, strict) id,

enum struct Cpp_Keyword : Default_Underlying {
    MMML_CPP_KEYWORD_ENUM_DATA(MMML_CPP_KEYWORD_ENUMERATOR)
};

#define MMML_CPP_KEYWORD_U8_CODE(id, code, highlight, strict) u8##code,
#define MMML_CPP_KEYWORD_HIGHLIGHT(id, code, highlight, strict) Highlight_Type::highlight,
#define MMML_CPP_KEYWORD_STRICT_STRING(id, code, highlight, strict) #strict

// clang-format off
// https://eel.is/c++draft/lex.key#tab:lex.key
// plus compiler extensions and alternative operator representations.
constexpr std::u8string_view cpp_keyword_names[] {
    MMML_CPP_KEYWORD_ENUM_DATA(MMML_CPP_KEYWORD_U8_CODE)
};

constexpr std::size_t cpp_keyword_count = std::size(cpp_keyword_names);

constexpr Highlight_Type cpp_keyword_highlights[cpp_keyword_count] {
    MMML_CPP_KEYWORD_ENUM_DATA(MMML_CPP_KEYWORD_HIGHLIGHT)
};

constexpr std::bitset<cpp_keyword_count> cpp_keyword_strict_set {
    MMML_CPP_KEYWORD_ENUM_DATA(MMML_CPP_KEYWORD_STRICT_STRING),
    cpp_keyword_count
};
// clang-format on

static_assert(std::ranges::is_sorted(cpp_keyword_names));

[[nodiscard]]
constexpr std::optional<Cpp_Keyword> keyword_by_identifier(std::u8string_view identifier)
{
    const std::u8string_view* const result
        = std::ranges::lower_bound // NOLINT(misc-include-cleaner)
        (cpp_keyword_names, identifier);
    if (result == std::end(cpp_keyword_names) || *result != identifier) {
        return {};
    }
    return Cpp_Keyword(result - cpp_keyword_names);
}

[[nodiscard]]
constexpr bool keyword_is_strict(Cpp_Keyword keyword)
{
    return cpp_keyword_strict_set[std::size_t(keyword)];
}

[[nodiscard]]
constexpr Highlight_Type keyword_highlight(Cpp_Keyword keyword)
{
    return cpp_keyword_highlights[std::size_t(keyword)];
}

[[nodiscard]]
std::size_t match_cpp_identifier_except_keywords(std::u8string_view str, bool strict_only)
{
    if (const std::size_t result = cpp::match_identifier(str)) {
        const std::optional<Cpp_Keyword> keyword = keyword_by_identifier(str.substr(0, result));
        if (keyword && (!strict_only || keyword_is_strict(*keyword))) {
            return 0;
        }
        return result;
    }
    return 0;
}

[[nodiscard]]
Highlight_Type cpp_token_type_highlight(cpp::Cpp_Token_Type type)
{
    switch (type) {
        using enum cpp::Cpp_Token_Type;
    case pound:
    case pound_pound:
    case pound_alt:
    case pound_pound_alt: return Highlight_Type::meta;

    case keyword_and:
    case keyword_or:
    case keyword_xor:
    case keyword_not:
    case keyword_bitand:
    case keyword_bitor:
    case keyword_compl:
    case keyword_and_eq:
    case keyword_or_eq:
    case keyword_xor_eq:
    case keyword_not_eq: return Highlight_Type::keyword;

    case semicolon:
    case colon:
    case comma: return Highlight_Type::symbol_other;

    case left_brace:
    case left_brace_alt:
    case right_brace:
    case right_brace_alt:
    case left_square:
    case left_square_alt:
    case right_square:
    case right_square_alt:
    case left_parens:
    case right_parens: return Highlight_Type::symbol_important;

    default: return Highlight_Type::symbol;
    }
}

} // namespace

bool highlight_cpp( //
    std::pmr::vector<Annotation_Span<Highlight_Type>>& out,
    std::u8string_view source,
    std::pmr::memory_resource*,
    const Highlight_Options& options
)
{
    const auto emit = [&](std::size_t begin, std::size_t length, Highlight_Type type) {
        const bool coalesce = options.coalescing && !out.empty() && out.back().value == type
            && out.back().end() == begin;
        if (coalesce) {
            out.back().length += length;
        }
        else {
            out.emplace_back(begin, length, type);
        }
    };
    // Approximately implements highlighting based on C++ tokenization,
    // as described in:
    // https://eel.is/c++draft/lex.phases
    // https://eel.is/c++draft/lex.pptoken

    std::size_t index = 0;
    // We need to keep track of whether we're on a "fresh line" for preprocessing directives.
    // A line is fresh if we've not encountered anything but whitespace on it yet.
    // https://eel.is/c++draft/cpp#def:preprocessing_directive
    bool fresh_line = true;

    while (index < source.size()) {
        const std::u8string_view remainder = source.substr(index);
        if (const std::size_t white_length = cpp::match_whitespace(remainder)) {
            fresh_line |= remainder.substr(0, white_length).contains(u8'\n');
            index += white_length;
            continue;
        }
        if (const cpp::Comment_Result line_comment = cpp::match_line_comment(remainder)) {
            const std::size_t content_length
                = line_comment.length - 2 - std::size_t(line_comment.is_terminated);
            emit(index, 2, Highlight_Type::comment_delimiter);
            emit(index + 2, content_length, Highlight_Type::comment);
            fresh_line = true;
            index += line_comment.length;
            continue;
        }
        if (const cpp::Comment_Result block_comment = cpp::match_block_comment(remainder)) {
            const std::size_t terminator_length = 2 * std::size_t(block_comment.is_terminated);
            emit(index, 2, Highlight_Type::comment_delimiter); // /*
            emit(index + 2, block_comment.length - 2 - terminator_length, Highlight_Type::comment);
            if (block_comment.is_terminated) {
                emit(index + block_comment.length - 2, 2, Highlight_Type::comment_delimiter); // */
            }
            index += block_comment.length;
            continue;
        }
        if (const cpp::String_Literal_Result literal = cpp::match_string_literal(remainder)) {
            const std::size_t suffix_length = literal.terminated
                ? match_cpp_identifier_except_keywords(
                      remainder.substr(literal.length), options.strict
                  )
                : 0;
            const std::size_t combined_length = literal.length + suffix_length;
            emit(index, combined_length, Highlight_Type::string);
            fresh_line = false;
            index += combined_length;
            continue;
        }
        if (const cpp::Character_Literal_Result literal = cpp::match_character_literal(remainder)) {
            const std::size_t suffix_length = literal.terminated
                ? match_cpp_identifier_except_keywords(
                      remainder.substr(literal.length), options.strict
                  )
                : 0;
            const std::size_t combined_length = literal.length + suffix_length;
            emit(index, combined_length, Highlight_Type::string);
            fresh_line = false;
            index += combined_length;
            continue;
        }
        if (const std::size_t number_length = cpp::match_pp_number(remainder)) {
            emit(index, number_length, Highlight_Type::number);
            fresh_line = false;
            index += number_length;
            continue;
        }
        if (const std::size_t id_length = cpp::match_identifier(remainder)) {
            const std::optional<Cpp_Keyword> keyword
                = keyword_by_identifier(remainder.substr(0, id_length));
            const auto highlight
                = keyword ? keyword_highlight(*keyword) : Highlight_Type::identifier;
            emit(index, id_length, highlight);
            fresh_line = false;
            index += id_length;
            continue;
        }
        if (const std::optional<cpp::Cpp_Token_Type> op
            = cpp::match_preprocessing_op_or_punc(remainder)) {
            const bool possible_directive
                = op == cpp::Cpp_Token_Type::pound || op == cpp::Cpp_Token_Type::pound_alt;
            if (fresh_line && possible_directive) {
                if (const auto directive = cpp::match_preprocessing_line(remainder)) {
                    fresh_line = false;
                    index += directive.length;
                    continue;
                }
            }
            const std::size_t op_length = cpp::cpp_token_type_length(*op);
            const Highlight_Type op_highlight = cpp_token_type_highlight(*op);
            emit(index, op_length, op_highlight);
            fresh_line = false;
            index += op_length;
            continue;
        }
        if (const std::size_t non_white_length = cpp::match_non_whitespace(remainder)) {
            // Don't emit any highlighting.
            // To my understanding, this currently only matches backslashes at the end of the line.
            // We don't have a separate phase for these, so whatever, this seems fine.
            fresh_line = false;
            index += non_white_length;
            continue;
        }
        MMML_ASSERT_UNREACHABLE(u8"Impossible state. One of the rules above should have matched.");
    }

    return true;
}

} // namespace mmml
