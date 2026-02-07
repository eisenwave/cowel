#include <string>
#include <string_view>

#include "cowel/util/chars.hpp"
#include "cowel/util/unicode.hpp"

#include "cowel/regexp.hpp"
#include "cowel/settings.hpp"

#ifdef COWEL_BUILD_WASM
#error "This file should not compile with emscripten!"
#endif

#ifdef COWEL_VERSION_MAJOR
// Suppress unused include settings.hpp
#endif

#include <boost/regex.hpp>
#include <boost/regex/icu.hpp>

namespace cowel {

static_assert(sizeof(Reg_Exp_Impl) == sizeof(boost::u32regex));

template <typename T>
Reg_Exp_Impl::Reg_Exp_Impl(In_Place_Tag, T&& arg) noexcept
{
    new (m_storage) boost::u32regex(std::forward<T>(arg));
}

auto& Reg_Exp_Impl::get()
{
    return *std::launder(reinterpret_cast<boost::u32regex*>(m_storage));
}

const auto& Reg_Exp_Impl::get() const
{
    return *std::launder(reinterpret_cast<const boost::u32regex*>(m_storage));
}

Reg_Exp_Impl::Reg_Exp_Impl() noexcept
{
    new (m_storage) boost::u32regex;
}

Reg_Exp_Impl::Reg_Exp_Impl(const Reg_Exp_Impl& other) noexcept
    : Reg_Exp_Impl { In_Place_Tag {}, other.get() }
{
}

Reg_Exp_Impl::Reg_Exp_Impl(Reg_Exp_Impl&& other) noexcept
    : Reg_Exp_Impl { In_Place_Tag {}, std::move(other.get()) }
{
}

// We assume that boost::regex handles self-assignment in some reasonable way,
// which it should because it boils down to std::shared_ptr self-assignment.
// NOLINTNEXTLINE(bugprone-unhandled-self-assignment)
Reg_Exp_Impl& Reg_Exp_Impl::operator=(const Reg_Exp_Impl& other) noexcept
{
    get() = other.get();
    return *this;
}

Reg_Exp_Impl& Reg_Exp_Impl::operator=(Reg_Exp_Impl&& other) noexcept
{
    // boost::basic_regex has no move operations,
    // but maybe this will change in the future, so we try to std::move anyway.
    // See https://github.com/boostorg/regex/issues/270
    // NOLINTNEXTLINE(performance-move-const-arg)
    get() = std::move(other.get());
    return *this;
}

Reg_Exp_Impl::~Reg_Exp_Impl()
{
    get().~basic_regex();
}

namespace {

[[nodiscard]]
std::u32string to_utf32(const std::u8string_view utf8)
{
    std::u32string result;
    std::u8string_view remainder = utf8;
    result.reserve(remainder.size());
    while (!remainder.empty()) {
        const auto [code_point, length] = utf8::decode_and_length_or_replacement(remainder);
        result += code_point;
        remainder.remove_prefix(std::size_t(length));
    }
    return result;
}

} // namespace

[[nodiscard]]
Result<Reg_Exp, Reg_Exp_Error_Code> Reg_Exp::make(const std::u8string_view pattern)
{
    constexpr auto flags = boost::regex_constants::ECMAScript | boost::regex_constants::no_except;

    auto result = [&] -> boost::u32regex {
        if (!pattern.contains(u8"\\u")) {
            return boost::make_u32regex(pattern.begin(), pattern.end(), flags);
        }
        const std::u32string ecma_pattern = to_utf32(pattern);
        const std::u32string boost_pattern = ecma_pattern_to_boost_pattern(ecma_pattern);

        return boost::make_u32regex(boost_pattern.begin(), boost_pattern.end(), flags);
    }();

    if (result.status() != 0) {
        return Reg_Exp_Error_Code::bad_pattern;
    }
    return Reg_Exp { Reg_Exp_Impl { In_Place_Tag {}, std::move(result) } };
}

[[nodiscard]]
Reg_Exp_Status Reg_Exp::test(const std::u8string_view string) const
{
    const bool result
        = boost::u32regex_match(string.data(), string.data() + string.size(), m_impl.get());
    return result ? Reg_Exp_Status::matched : Reg_Exp_Status::unmatched;
}

[[nodiscard]]
Reg_Exp_Search_Result Reg_Exp::search(const std::u8string_view string) const
{
    boost::match_results<const char8_t*> match;
    const bool found
        = boost::u32regex_search(string.data(), string.data() + string.size(), match, m_impl.get());
    if (!found) {
        return { Reg_Exp_Status::unmatched, {} };
    }
    const auto& first = match[0];
    COWEL_ASSERT(first.matched);
    const Reg_Exp_Match result_match {
        .index = std::size_t(first.begin() - string.data()),
        .length = std::size_t(first.end() - first.begin()),
    };
    return Reg_Exp_Search_Result { Reg_Exp_Status::matched, result_match };
}

[[nodiscard]]
std::u32string ecma_pattern_to_boost_pattern(const std::u32string_view ecma_pattern)
{
    // https://www.boost.org/doc/libs/latest/libs/regex/doc/html/boost_regex/background/standards.html
    // To fix some compliance issues of Boost.Regex with ECMAScript,
    // some minor adjustments are needed.
    // Notably, even with ECMAScript flavor,
    // Boost.Regex treats \u0030 not as U+0030 DIGIT ZERO,
    // but as any uppercase character, followed by 0030 literally.

    std::u32string result;
    bool escape = false;
    for (std::size_t i = 0; i < ecma_pattern.size(); ++i) {
        if (escape) {
            if (ecma_pattern[i] == U'u') {
                constexpr auto hex_digit_predicate
                    = [](const char32_t c) { return is_ascii_hex_digit(c); };
                const bool is_unicode_escape = i + 4 < ecma_pattern.size()
                    && std::ranges::all_of(ecma_pattern.substr(i + 1, 4), hex_digit_predicate);

                if (is_unicode_escape) {
                    // For code point escapes, we transform \uDDDD into \x{DDDD}.
                    // Parsing and appending the code point literally would be bad
                    // because that character could have special meaning in regular expressions.
                    const std::u32string_view hex_digits_u32 = ecma_pattern.substr(i + 1, 4);
                    result += U"\\x{";
                    result += hex_digits_u32;
                    result += U'}';
                    i += 4;
                }
                else {
                    // For any other use of "\u" (e.g. /\uZZ/, /\u()/),
                    // we append u literally.
                    result += U'u';
                }
            }
            else {
                result += U'\\';
                result += ecma_pattern[i];
            }
            escape = false;
        }
        else if (ecma_pattern[i] == U'\\') {
            escape = true;
        }
        else {
            result += ecma_pattern[i];
        }
    }
    if (escape) {
        result += U'\\';
    }
    return result;
}

} // namespace cowel
