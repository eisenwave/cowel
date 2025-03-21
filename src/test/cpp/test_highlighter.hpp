#ifndef MMML_TEST_HIGHLIGHTER_HPP
#define MMML_TEST_HIGHLIGHTER_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory_resource>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "mmml/util/annotation_span.hpp"
#include "mmml/util/result.hpp"
#include "mmml/util/typo.hpp"

#include "mmml/fwd.hpp"
#include "mmml/services.hpp"

#include "mmml/highlight/code_language.hpp"
#include "mmml/highlight/highlight.hpp"
#include "mmml/highlight/highlight_error.hpp"

namespace mmml {

/// @brief Runs syntax highlighting for code of a test-only language
/// where sequences of the character `x` are considered keywords.
/// Nothing else is highlighted.
inline void
highlight_x(std::pmr::vector<Annotation_Span<Highlight_Type>>& out, std::u8string_view code)
{
    char8_t prev = 0;
    std::size_t begin = 0;
    for (std::size_t i = 0; i < code.size(); ++i) {
        if (code[i] == u8'x' && prev != u8'x') {
            begin = i;
        }
        if (code[i] != u8'x' && prev == u8'x') {
            const Highlight_Span span { .begin = begin,
                                        .length = i - begin,
                                        .value = Highlight_Type::keyword };
            out.push_back(span);
        }
        prev = code[i];
    }
    if (prev == u8'x') {
        const Highlight_Span span { .begin = begin,
                                    .length = code.size() - begin,
                                    .value = Highlight_Type::keyword };
        out.push_back(span);
    }
}

[[nodiscard]]
constexpr bool is_supported_test_language(Code_Language lang)
{
    return lang == Code_Language::cpp || lang == Code_Language::mmml;
}

consteval auto build_test_highlighter_support()
{
    constexpr auto proj = &detail::Code_Language_Entry::value;
    constexpr std::size_t count
        = std::ranges::count_if(detail::code_language_by_name, is_supported_test_language, proj)
        + 1;
    std::array<std::u8string_view, count> result;
    auto* out = result.data();
    for (const auto& [name, lang] : detail::code_language_by_name) {
        if (is_supported_test_language(lang)) {
            *out++ = name;
        }
    }
    result.back() = u8"x";
    return result;
}

struct Test_Highlighter final : Syntax_Highlighter {
private:
    static constexpr std::array supported = build_test_highlighter_support();

public:
    constexpr Test_Highlighter() = default;

    [[nodiscard]]
    std::span<const std::u8string_view> get_supported_languages() const final
    {
        return supported;
    }

    [[nodiscard]]
    Distant<std::u8string_view>
    match_supported_language(std::u8string_view language, std::pmr::memory_resource* memory)
        const final
    {
        const Distant<std::size_t> match = closest_match(supported, language, memory);
        return { supported[match.value], match.distance };
    }

    [[nodiscard]]
    Result<void, Syntax_Highlight_Error> operator()(
        std::pmr::vector<Highlight_Span>& out,
        std::u8string_view code,
        std::u8string_view language,
        std::pmr::memory_resource* memory
    ) const final
    {
        if (language == u8"x") {
            highlight_x(out, code);
            return {};
        }
        if (!std::ranges::contains(supported, language)) {
            return Syntax_Highlight_Error::unsupported_language;
        }
        const std::optional<Code_Language> lang = code_language_by_name(language);
        if (!lang) {
            return Syntax_Highlight_Error::unsupported_language;
        }
        highlight(out, code, *lang, memory);
        return {};
    }
};

inline constexpr Test_Highlighter test_highlighter {};

} // namespace mmml

#endif
