#ifndef COWEL_SETTINGS_HPP
#define COWEL_SETTINGS_HPP

namespace cowel {

/// @brief If `true`, adds assertions in various places
/// which check for writing of empty strings to content policies and other places.
/// The point is to identify potential optimization opportunities/correctness problems,
/// where empty strings ultimately have no effect anyway.
inline constexpr bool enable_empty_string_assertions = false;

} // namespace cowel

#endif
