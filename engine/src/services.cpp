#include <cstddef>
#include <memory_resource>
#include <span>
#include <string_view>
#include <vector>

#include "ulight/ulight.h"
#include "ulight/ulight.hpp"

#include "cowel/util/result.hpp"
#include "cowel/util/typo.hpp"

#include "cowel/cowel_lib.hpp"
#include "cowel/services.hpp"
#include "cowel/ulight_highlighter.hpp"

namespace cowel {

std::span<const std::u8string_view> Ulight_Syntax_Highlighter::get_supported_languages() const
{
    static const auto data = [] {
        std::vector<std::u8string_view> out;
        const std::span<const ulight_lang_entry> ulight_entries { ulight_lang_list,
                                                                  ulight_lang_list_length };
        for (const auto& [name_data, name_length, _] : ulight_entries) {
            out.push_back({ reinterpret_cast<const char8_t*>(name_data), name_length });
        }
        out.push_back(u8"x");
        return out;
    }();
    return data;
}

Distant<std::u8string_view> Ulight_Syntax_Highlighter::match_supported_language(
    std::u8string_view language,
    std::pmr::memory_resource* memory
) const
{
    const std::span<const std::u8string_view> supported = get_supported_languages();
    const Distant<std::size_t> closest = closest_match(supported, language, memory);
    return { .value = supported[closest.value], .distance = closest.distance };
}

Result<void, Syntax_Highlight_Error> Ulight_Syntax_Highlighter::operator()( //
    std::pmr::vector<Highlight_Span>& out,
    std::u8string_view code,
    std::u8string_view language,
    std::pmr::memory_resource*
)
{
    // TODO: find a way to provide memory for dynamic allocations to ulight
    static ulight::Token token_buffer[1024];

    const ulight::Lang lang = ulight::get_lang(language);
    if (lang == ulight::Lang::none) {
        return Syntax_Highlight_Error::unsupported_language;
    }

    ulight::State state;
    state.set_token_buffer(token_buffer);
    state.set_lang(lang);
    state.set_source(code);

    auto on_flush = [&](const ulight::Token* tokens, std::size_t size) {
        out.insert(out.end(), tokens, tokens + size);
    };
    state.on_flush_tokens(on_flush);

    switch (state.source_to_tokens()) {
    case ulight::Status::ok: return {};
    case ulight::Status::bad_code: return Syntax_Highlight_Error::bad_code;
    default: return Syntax_Highlight_Error::other;
    }
}

namespace {

[[nodiscard]]
Diagnostic to_diagnostic(const cowel_diagnostic_u8& diagnostic)
{
    const File_Source_Span location {
        Source_Position {
            .line = diagnostic.line,
            .column = diagnostic.column,
            .begin = diagnostic.begin,
        },
        diagnostic.length,
        File_Id(diagnostic.file_id),
    };
    return {
        .severity = Severity(diagnostic.severity),
        .id = as_u8string_view(diagnostic.id),
        .location = location,
        .message = as_u8string_view(diagnostic.message),
    };
}

} // namespace

[[nodiscard]]
Function_Ref<void(const cowel_diagnostic_u8*) noexcept> Logger::as_cowel_log_fn()
{
    static_assert(
        std::is_same_v<decltype(as_cowel_log_fn())::Invoker, cowel_log_fn_u8>,
        "Returned Function_Ref must be suitable for conversion to cowel_log_fn_u8."
    );
    constexpr auto result = [](Logger* self, const cowel_diagnostic_u8* const diagnostic) noexcept {
        COWEL_ASSERT(diagnostic);
        (*self)(to_diagnostic(*diagnostic));
    };
    return { const_v<result>, this };
}

} // namespace cowel
