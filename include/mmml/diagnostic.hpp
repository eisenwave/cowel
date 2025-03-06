#ifndef MMML_DIAGNOSTIC_HPP
#define MMML_DIAGNOSTIC_HPP

#include <compare>
#include <string>

#include "mmml/util/source_position.hpp"

#include "mmml/fwd.hpp"

namespace mmml {

enum struct Diagnostic_Type : Default_Underlying {
    /// @brief Debugging messages.
    /// Only emitted in debug mode.
    debug,
    /// @brief Minor problems. Only emitted in verbose mode.
    soft_warning,
    /// @brief Major problems with the document.
    warning,
    /// @brief Problems with the document that prevent proper content generation.
    /// Usually results in the generation of `\\error` directives.
    error,
};

[[nodiscard]]
constexpr std::strong_ordering operator<=>(Diagnostic_Type x, Diagnostic_Type y) noexcept
{
    return Default_Underlying(x) <=> Default_Underlying(y);
}

struct Diagnostic {
    Diagnostic_Type type;
    std::pmr::u8string message;
};

} // namespace mmml

#endif
