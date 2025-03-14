#ifndef MMML_DIAGNOSTIC_HPP
#define MMML_DIAGNOSTIC_HPP

#include <compare>
#include <string>
#include <string_view>

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

[[nodiscard]]
constexpr bool severity_is_emittable(Severity x) noexcept
{
    return x >= Severity::min && x <= Severity::max;
}

struct Diagnostic {
    /// @brief The severity of the diagnostic.
    /// `severity_is_emittable(severity)` shall be `true`.
    Severity severity;
    /// @brief The id of the diagnostic,
    /// which is a non-empty string containing a
    /// dot-separated sequence of identifier for this diagnostic.
    std::u8string_view id;
    /// @brief The span of code that is responsible for this diagnostic.
    Source_Span location;
    /// @brief The diagnostic message.
    std::pmr::u8string message;
};

namespace diagnostic {

/// @brief In `\\c`, arguments were ignored.
inline constexpr std::u8string_view c_args_ignored = u8"c.args.ignored";
/// @brief In `\\c`, the input is blank.
inline constexpr std::u8string_view c_blank = u8"c.blank";
/// @brief In `\\c`, the name is invalid, like `\\c{nonsense}`.
inline constexpr std::u8string_view c_name = u8"c.name";
/// @brief In `\\c`, parsing digits failed, like `\\c{#x1234abc}`.
inline constexpr std::u8string_view c_digits = u8"c.digits";
/// @brief In `\\c`, a nonscalar value would be encoded.
/// @see is_scalar_value
inline constexpr std::u8string_view c_nonscalar = u8"c.nonscalar";

/// @brief In `\\U`, arguments were ignored.
inline constexpr std::u8string_view U_args_ignored = u8"U.args.ignored";
/// @brief In `\\U`, the input is blank.
inline constexpr std::u8string_view U_blank = u8"U.blank";
/// @brief In `\\U`, parsing digits failed, like `\\U{abc}`.
inline constexpr std::u8string_view U_digits = u8"U.digits";
/// @brief In `\\U`, a nonscalar value would be encoded.
/// @see is_scalar_value
inline constexpr std::u8string_view U_nonscalar = u8"charref.nonscalar";

} // namespace diagnostic

} // namespace mmml

#endif
