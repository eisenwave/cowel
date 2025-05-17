#ifndef COWEL_ULIGHT_HIGHLIGHTER_HPP
#define COWEL_ULIGHT_HIGHLIGHTER_HPP

#include <memory_resource>
#include <span>
#include <string_view>
#include <vector>

#include "cowel/services.hpp"

namespace cowel {

/// @brief A `Syntax_Highlighter` that uses the µlight library.
struct Ulight_Syntax_Highlighter final : Syntax_Highlighter {

    [[nodiscard]]
    std::span<const std::u8string_view> get_supported_languages() const final;

    [[nodiscard]]
    Distant<std::u8string_view>
    match_supported_language(std::u8string_view language, std::pmr::memory_resource* memory)
        const final;

    [[nodiscard]]
    Result<void, Syntax_Highlight_Error> operator()( //
        std::pmr::vector<Highlight_Span>& out,
        std::u8string_view code,
        std::u8string_view language,
        std::pmr::memory_resource* memory
    ) final;
};

inline constinit Ulight_Syntax_Highlighter ulight_syntax_highlighter;

} // namespace cowel

#endif
