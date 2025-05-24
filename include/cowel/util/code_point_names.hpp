#ifndef COWEL_CODE_POINT_NAMES_HPP
#define COWEL_CODE_POINT_NAMES_HPP

#include <string_view>

namespace cowel {

/// @brief Returns a Unicode code point by its character name,
/// or one of its "control", "alternate", or "correction" aliases.
///
/// If the name does not match that of any code point in the database,
/// `char32_t(-1)` is returned.
[[nodiscard]]
char32_t code_point_by_name(std::u8string_view name) noexcept;

} // namespace cowel

#endif
