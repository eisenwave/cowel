#ifndef COWEL_CODE_POINT_NAMES_HPP
#define COWEL_CODE_POINT_NAMES_HPP

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include "cowel/util/fixed_string.hpp"

namespace cowel {

/// @brief Returns a Unicode code point by its character name,
/// or one of its "control", "alternate", or "correction" aliases.
///
/// If the name does not match that of any code point in the database,
/// `char32_t(-1)` is returned.
[[nodiscard]]
char32_t code_point_by_name(std::u8string_view name) noexcept;

struct Code_Point_Name_Match {
    std::string name;
    std::size_t distance = 0;
    char32_t value = 0;
};

/// @brief Finds the nearest code point name matches for a search pattern.
/// @param pattern The queried name pattern.
/// @param out_matches Output buffer receiving matches sorted by distance then name.
/// @return The number of results written to `out_matches`.
[[nodiscard]]
std::size_t nearest_matches_for_codepoint_name(
    std::u8string_view pattern,
    std::span<Code_Point_Name_Match> out_matches
);

/// @brief Finds Unicode code point names that start with a given prefix.
/// @param out Output buffer receiving matching names.
/// @param prefix The queried name prefix.
/// @return The number of results written to `out`.
[[nodiscard]]
std::size_t
code_point_names_starting_with(std::span<Fixed_String8<96>> out, std::u8string_view prefix);

/// @brief Returns the Unicode name of a code point
/// as defined in the Unicode Character Database.
/// @param code_point The Unicode code point to look up.
/// @return The name, or an empty string if the code point has no name.
[[nodiscard]]
Fixed_String8<96> code_point_name(char32_t code_point) noexcept;

/// @brief Returns the most suitable name for display of a code point.
/// Specifically, this returns a name from the following name sources,
/// in that order of precedence:
/// - `correction` name alias
/// - `code_point_name(code_point)`
/// - `control` name alias
/// - `alternate` name alias
/// - `figment` name alias
///
/// Note that names from the `abbreviation` name alias category are never returned.
/// Also note that only one of the aliases is returned,
/// though there are code points like `U+000A` with multiple aliases in the same category.
/// @param code_point The Unicode code point to look up.
/// @return The display name,
/// or an empty string if the code point has no name and no aliases.
[[nodiscard]]
Fixed_String8<96> code_point_display_name(char32_t code_point) noexcept;

} // namespace cowel

#endif
