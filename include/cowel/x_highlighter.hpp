#ifndef COWEL_X_HIGHLIGHTER_HPP
#define COWEL_X_HIGHLIGHTER_HPP

#include <cstddef>
#include <memory_resource>
#include <span>
#include <string_view>
#include <vector>

#include "cowel/util/function_ref.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/typo.hpp"

#include "cowel/cowel.h"
#include "cowel/fwd.hpp"
#include "cowel/memory_resources.hpp"
#include "cowel/services.hpp"

namespace cowel {

/// @brief Runs syntax highlighting for code of a test-only language
/// where sequences of the character `x` are considered keywords.
/// Nothing else is highlighted.
inline void highlight_x(std::pmr::vector<Highlight_Span>& out, const std::u8string_view code)
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
X_Highlighter final : Syntax_Highlighter {

    [[nodiscard]]
    std::span<const std::u8string_view> get_supported_languages() const override
    {
        static constexpr std::u8string_view supported = u8"x";
        return { &supported, 1 };
    }

    [[nodiscard]]
    Distant<std::u8string_view> match_supported_language(
        const std::u8string_view language,
        std::pmr::memory_resource* const memory
    ) const override
    {
        const auto supported = get_supported_languages();
        const Distant<std::size_t> match = closest_match(supported, language, memory);
        return { supported[match.value], match.distance };
    }

    [[nodiscard]]
    Result<void, Syntax_Highlight_Error> operator()(
        std::pmr::vector<Highlight_Span>& out,
        const std::u8string_view code,
        const std::u8string_view language,
        std::pmr::memory_resource* const
    ) override
    {
        if (language == u8"x") {
            highlight_x(out, code);
            return {};
        }
        return Syntax_Highlight_Error::unsupported_language;
    }

    [[nodiscard]]
    constexpr cowel_syntax_highlighter_u8 as_cowel_syntax_highlighter()
    {
        static constexpr cowel_string_view_u8 supported_languages { u8"x", 1 };

        // We only create these Function_Refs to obtain the invoker,
        // which saves us the ugliness of down-casting and const-casting.
        Function_Ref<cowel_syntax_highlight_status(
            const cowel_syntax_highlight_buffer*, const char8_t*, size_t, const char8_t*, size_t
        ) noexcept>
            highlight_by_lang_name_fn = { const_v<highlight_by_lang_name>, this };
        Function_Ref<cowel_syntax_highlight_status(
            const cowel_syntax_highlight_buffer*, const char8_t*, size_t, size_t
        ) noexcept>
            highlight_by_lang_index_fn = { const_v<highlight_by_lang_index>, this };

        return {
            .supported_languages = &supported_languages,
            .supported_languages_size = 1,
            .highlight_by_lang_name = highlight_by_lang_name_fn.get_invoker(),
            .highlight_by_lang_index = highlight_by_lang_index_fn.get_invoker(),
            .data = this,
        };
    }

private:
    static cowel_syntax_highlight_status highlight_by_lang_name(
        X_Highlighter* const self,
        const cowel_syntax_highlight_buffer* const token_buffer,
        const char8_t* const text,
        const size_t text_length,
        const char8_t* const lang_name,
        const size_t lang_name_length
    ) noexcept
    {
        // This always uses the global allocator,
        // but since the highlighter is only used in test code,
        // this is somewhat inconsequential.
        auto* const memory = Global_Memory_Resource::get();
        std::pmr::vector<Highlight_Span> tokens { memory };
        Result<void, Syntax_Highlight_Error> result
            = (*self)(tokens, { text, text_length }, { lang_name, lang_name_length }, memory);
        if (!result) {
            switch (result.error()) {
            case Syntax_Highlight_Error::unsupported_language:
                return COWEL_SYNTAX_HIGHLIGHT_UNSUPPORTED_LANGUAGE;
            case Syntax_Highlight_Error::bad_code: return COWEL_SYNTAX_HIGHLIGHT_BAD_CODE;
            case Syntax_Highlight_Error::other: return COWEL_SYNTAX_HIGHLIGHT_ERROR;
            }
            COWEL_ASSERT_UNREACHABLE(u8"Invalid error.");
        }

        std::size_t index = 0;
        while (index < tokens.size()) {
            static_assert(sizeof(cowel_syntax_highlight_token) == sizeof(Highlight_Span));
            const std::size_t remaining = tokens.size() - index;
            const std::size_t chunk_size = std::min(remaining, token_buffer->size);
            std::memcpy(
                token_buffer->data, tokens.data() + index,
                chunk_size * sizeof(cowel_syntax_highlight_token)
            );
            token_buffer->flush(token_buffer->flush_data, token_buffer->data, chunk_size);
            index += chunk_size;
        }
        return COWEL_SYNTAX_HIGHLIGHT_OK;
    }

    static cowel_syntax_highlight_status highlight_by_lang_index(
        X_Highlighter* const self,
        const cowel_syntax_highlight_buffer* const token_buffer,
        const char8_t* const text,
        const size_t text_length,
        const size_t lang_index
    ) noexcept
    {
        if (lang_index != 0) {
            return COWEL_SYNTAX_HIGHLIGHT_UNSUPPORTED_LANGUAGE;
        }
        return highlight_by_lang_name(self, token_buffer, text, text_length, u8"x", 1);
    }
};

inline constinit X_Highlighter x_highlighter;

} // namespace cowel

#endif
