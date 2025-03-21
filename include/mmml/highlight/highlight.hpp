#ifndef MMML_HIGHLIGHT_TOKEN_HPP
#define MMML_HIGHLIGHT_TOKEN_HPP

#include <memory_resource>
#include <string_view>
#include <vector>

#include "mmml/util/result.hpp"

#include "mmml/fwd.hpp"

#include "mmml/highlight/code_language.hpp"
#include "mmml/highlight/highlight_error.hpp"

namespace mmml {

struct Highlight_Options {
    /// @brief If `true`,
    /// adjacent spans with the same `Highlight_Type` get merged into one.
    bool coalescing = false;
    /// @brief If `true`,
    /// does not highlight keywords and other features from technical specifications,
    /// compiler extensions, from similar languages, and other "non-standard" sources.
    ///
    /// For example, if `false`, C++ highlighting also includes all C keywords.
    bool strict = false;
};

bool highlight_mmml(
    std::pmr::vector<Highlight_Span>& out,
    std::u8string_view source,
    std::pmr::memory_resource* memory,
    const Highlight_Options& options = {}
);
bool highlight_cpp(
    std::pmr::vector<Highlight_Span>& out,
    std::u8string_view source,
    std::pmr::memory_resource* memory,
    const Highlight_Options& options = {}
);

inline Result<void, Syntax_Highlight_Error> highlight(
    std::pmr::vector<Highlight_Span>& out,
    std::u8string_view source,
    Code_Language language,
    std::pmr::memory_resource* memory,
    const Highlight_Options& options = {}
)
{
    constexpr auto to_result = [](bool success) -> Result<void, Syntax_Highlight_Error> {
        if (success) {
            return {};
        }
        return Syntax_Highlight_Error::bad_code;
    };

    switch (language) {
    case Code_Language::cpp: //
        return to_result(highlight_cpp(out, source, memory, options));
    case Code_Language::mmml: //
        return to_result(highlight_mmml(out, source, memory, options));
    default: //
        return Syntax_Highlight_Error::unsupported_language;
    }
}

} // namespace mmml

#endif
