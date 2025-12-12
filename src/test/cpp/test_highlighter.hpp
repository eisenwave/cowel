#ifndef COWEL_TEST_HIGHLIGHTER_HPP
#define COWEL_TEST_HIGHLIGHTER_HPP

#include <cstddef>
#include <memory_resource>
#include <span>
#include <string_view>
#include <vector>

#include "cowel/util/result.hpp"
#include "cowel/util/typo.hpp"

#include "cowel/fwd.hpp"
#include "cowel/services.hpp"
#include "cowel/ulight_highlighter.hpp"

namespace cowel {

/// @brief Runs syntax highlighting for code of a test-only language
/// where sequences of the character `x` are considered keywords.
/// Nothing else is highlighted.
inline void highlight_x(std::pmr::vector<Highlight_Span>& out, std::u8string_view code)
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
                                        .type = Default_Underlying(Highlight_Type::keyword) };
            out.push_back(span);
        }
        prev = code[i];
    }
    if (prev == u8'x') {
        const Highlight_Span span { .begin = begin,
                                    .length = code.size() - begin,
                                    .type = Default_Underlying(Highlight_Type::keyword) };
        out.push_back(span);
    }
}

struct [[nodiscard]]
Test_Highlighter final : Syntax_Highlighter {

    [[nodiscard]]
    std::span<const std::u8string_view> get_supported_languages() const final
    {
        static auto supported = [] {
            const std::span<const std::u8string_view> ulight_langs
                = ulight_syntax_highlighter.get_supported_languages();
            std::vector<std::u8string_view> out;
            out.insert(out.end(), ulight_langs.begin(), ulight_langs.end());
            out.push_back(u8"x");
            return out;
        }();
        return supported;
    }

    [[nodiscard]]
    Distant<std::u8string_view> match_supported_language(
        std::u8string_view language,
        std::pmr::memory_resource* memory
    ) const final
    {
        const auto supported = get_supported_languages();
        const Distant<std::size_t> match = closest_match(supported, language, memory);
        return { supported[match.value], match.distance };
    }

    [[nodiscard]]
    Result<void, Syntax_Highlight_Error> operator()(
        std::pmr::vector<Highlight_Span>& out,
        std::u8string_view code,
        std::u8string_view language,
        std::pmr::memory_resource* memory
    ) final
    {
        if (language == u8"x") {
            highlight_x(out, code);
            return {};
        }
        return ulight_syntax_highlighter(out, code, language, memory);
    }
};

inline constinit Test_Highlighter test_highlighter;

} // namespace cowel

#endif
