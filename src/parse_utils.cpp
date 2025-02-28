#include <charconv>
#include <optional>

#include "mmml/assert.hpp"
#include "mmml/parse_utils.hpp"

namespace mmml {
namespace {

std::optional<unsigned long long> parse_uinteger_digits(std::string_view text, int base)
{
    unsigned long long value;
    std::from_chars_result result = std::from_chars(text.begin(), text.end(), value, base);
    if (result.ec != std::errc {}) {
        return {};
    }
    return value;
}

} // namespace

std::optional<Text_Match> match_line_comment(std::string_view s) noexcept
{
    if (!s.starts_with("//")) {
        return {};
    }
    const std::size_t end = s.find('\n', 2);
    if (end == std::string_view::npos) {
        return Text_Match { .length = s.length(), .is_terminated = false };
    }
    return Text_Match { .length = end, .is_terminated = true };
}

std::optional<Text_Match> match_block_comment(std::string_view s) noexcept
{
    if (!s.starts_with("/*")) {
        return {};
    }
    // naive: nesting disallowed, but line comments can be nested in block comments
    const std::size_t end = s.find("*/", 2);
    if (end == std::string_view::npos) {
        return Text_Match { .length = s.length(), .is_terminated = false };
    }
    return Text_Match { .length = end + 2, .is_terminated = true };
}

[[nodiscard]]
std::optional<Text_Match> match_string_literal(std::string_view s) noexcept
{
    if (!s.starts_with('"')) {
        return {};
    }
    bool escaped = false;
    for (std::size_t i = 1; i < s.size(); ++i) {
        if (escaped) {
            escaped = false;
        }
        else if (s[i] == '\\') {
            escaped = true;
        }
        else if (s[i] == '"') {
            return Text_Match { .length = i + 1, .is_terminated = true };
        }
    }
    return Text_Match { .length = s.length(), .is_terminated = false };
}

std::size_t match_digits(std::string_view str, int base)
{
    MMML_ASSERT((base >= 2 && base <= 10) || base == 16);
    static constexpr std::string_view hexadecimal_digits = "0123456789abcdefABCDEF";

    const std::string_view digits
        = base == 16 ? hexadecimal_digits : hexadecimal_digits.substr(0, std::size_t(base));

    // std::min covers the case of std::string_view::npos
    return std::min(str.find_first_not_of(digits), str.size());
}

std::size_t match_identifier(std::string_view str) noexcept
{
    if (str.empty() || is_decimal_digit(str[0])) {
        return 0;
    }
    const std::size_t result = str.find_first_not_of(identifier_characters);
    return std::min(result, str.length());
}

Literal_Match_Result match_integer_literal(std::string_view s) noexcept
{
    if (s.empty() || !is_decimal_digit(s[0])) {
        return { Literal_Match_Status::no_digits, 0, {} };
    }
    if (s.starts_with("0b")) {
        const std::size_t digits = match_digits(s.substr(2), 2);
        if (digits == 0) {
            return { Literal_Match_Status::no_digits_following_prefix, 2, Literal_Type::binary };
        }
        return { Literal_Match_Status::ok, digits + 2, Literal_Type::binary };
    }
    if (s.starts_with("0x")) {
        const std::size_t digits = match_digits(s.substr(2), 16);
        if (digits == 0) {
            return { Literal_Match_Status::no_digits_following_prefix, 2,
                     Literal_Type::hexadecimal };
        }
        return { Literal_Match_Status::ok, digits + 2, Literal_Type::hexadecimal };
    }
    if (s[0] == '0') {
        const std::size_t digits = match_digits(s, 8);
        return { Literal_Match_Status::ok, digits,
                 digits == 1 ? Literal_Type::decimal : Literal_Type::octal };
    }
    const std::size_t digits = match_digits(s, 10);

    return { Literal_Match_Status::ok, digits, Literal_Type::decimal };
}

std::optional<unsigned long long> parse_uinteger_literal(std::string_view str) noexcept
{
    if (str.empty()) {
        return {};
    }
    if (str.starts_with("0b")) {
        return parse_uinteger_digits(str.substr(2), 2);
    }
    if (str.starts_with("0x")) {
        return parse_uinteger_digits(str.substr(2), 16);
    }
    if (str.starts_with("0")) {
        return parse_uinteger_digits(str, 8);
    }
    return parse_uinteger_digits(str, 10);
}

std::optional<long long> parse_integer_literal(std::string_view str) noexcept
{
    if (str.empty()) {
        return std::nullopt;
    }
    if (str.starts_with("-")) {
        if (auto positive = parse_uinteger_literal(str.substr(1))) {
            // Negating as Uint is intentional and prevents overflow.
            return static_cast<long long>(-*positive);
        }
        return std::nullopt;
    }
    if (auto result = parse_uinteger_literal(str)) {
        return static_cast<long long>(*result);
    }
    return std::nullopt;
}

} // namespace mmml
