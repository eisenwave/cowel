#ifndef MMML_DIAGNOSTIC_HPP
#define MMML_DIAGNOSTIC_HPP

#include <compare>
#include <string>

#include "mmml/util/source_position.hpp"

#include "mmml/fwd.hpp"

namespace mmml {

enum struct Severity : Default_Underlying {
    /// @brief Alias for `debug`.
    min = 0,
    /// @brief Debugging messages.
    /// Only emitted in debug mode.
    debug = 0,
    /// @brief Minor problems. Only emitted in verbose mode.
    soft_warning = 1,
    /// @brief Major problems with the document.
    warning = 2,
    /// @brief Problems with the document that prevent proper content generation.
    /// Usually results in the generation of `\\error` directives.
    error = 3,
    /// @brief Alias for `error`.
    max = 3,
    /// @brief Greater than all other levels.
    /// No diagnostic with this level is emitted, so using it as a minimum level
    /// silences all diagnostics, even errors.
    none = 4,
};

[[nodiscard]]
constexpr std::strong_ordering operator<=>(Severity x, Severity y) noexcept
{
    return Default_Underlying(x) <=> Default_Underlying(y);
}

struct Diagnostic {
    Severity severity;
    Source_Span location;
    std::pmr::u8string message;
};

} // namespace mmml

#endif
