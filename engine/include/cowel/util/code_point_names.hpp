#ifndef COWEL_CODE_POINT_NAMES_HPP
#define COWEL_CODE_POINT_NAMES_HPP

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

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

} // namespace cowel

#endif
