#ifndef MMML_CODE_LANGUAGE_HPP
#define MMML_CODE_LANGUAGE_HPP

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string_view>

#include "mmml/fwd.hpp"

namespace mmml {

#define MMML_CODE_LANGUAGE_ENUM_DATA(F)                                                            \
    F(c, "C")                                                                                      \
    F(cpp, "C++")                                                                                  \
    F(css, "CSS")                                                                                  \
    F(html, "HTML")                                                                                \
    F(java, "Java")                                                                                \
    F(javascript, "JavaScript")                                                                    \
    F(mmml, "MMML")                                                                                \
    F(typescript, "TypeScript")

#define MMML_CODE_LANGUAGE_ENUMERATOR(id, name) id,

enum struct Code_Language : Default_Underlying {
    MMML_CODE_LANGUAGE_ENUM_DATA(MMML_CODE_LANGUAGE_ENUMERATOR)
};

namespace detail {

struct Code_Language_Entry {
    std::u8string_view key;
    Code_Language value;
};

// Based on Highlight.JS names.
// https://github.com/highlightjs/highlight.js/blob/main/SUPPORTED_LANGUAGES.md?plain=1

// clang-format off
constexpr Code_Language_Entry code_language_by_name[] {
    { u8"c", Code_Language::c },
    { u8"c++", Code_Language::cpp },
    { u8"cc", Code_Language::cpp },
    { u8"cplusplus", Code_Language::cpp },
    { u8"cpp", Code_Language::cpp },
    { u8"css", Code_Language::css },
    { u8"cts", Code_Language::typescript },
    { u8"cxx", Code_Language::cpp },
    { u8"h", Code_Language::c },
    { u8"h++", Code_Language::cpp },
    { u8"hpp", Code_Language::cpp },
    { u8"htm", Code_Language::html },
    { u8"html", Code_Language::html },
    { u8"hxx", Code_Language::cpp },
    { u8"java", Code_Language::java },
    { u8"javascript", Code_Language::javascript },
    { u8"js", Code_Language::javascript },
    { u8"jsx", Code_Language::javascript },
    { u8"mmml", Code_Language::mmml },
    { u8"mts", Code_Language::typescript },
    { u8"ts", Code_Language::typescript },
    { u8"tsx", Code_Language::typescript },
    { u8"typescript", Code_Language::typescript },
};
// clang-format on

static_assert(std::ranges::is_sorted(code_language_by_name, {}, &Code_Language_Entry::key));

#define MMML_CODE_LANGUAGE_U8_NAME(id, name) u8##name,

inline constexpr std::u8string_view code_language_names[] {
    MMML_CODE_LANGUAGE_ENUM_DATA(MMML_CODE_LANGUAGE_U8_NAME)
};

} // namespace detail

[[nodiscard]]
constexpr std::u8string_view code_language_name(Code_Language lang)
{
    return detail::code_language_names[std::size_t(lang)];
}

[[nodiscard]]
constexpr std::optional<Code_Language> code_language_by_name(std::u8string_view name)
{
    const auto* const entry = std::ranges::lower_bound(
        detail::code_language_by_name, name, {}, &detail::Code_Language_Entry::key
    );
    if (entry == std::end(detail::code_language_by_name) || entry->key != name) {
        return {};
    }
    return entry->value;
}

} // namespace mmml

#endif
