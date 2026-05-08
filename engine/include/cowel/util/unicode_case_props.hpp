#ifndef COWEL_UTIL_UNICODE_CASE_PROPS_HPP
#define COWEL_UTIL_UNICODE_CASE_PROPS_HPP

namespace cowel {

/// @brief Returns `true` iff `c` has the Unicode `Cased` property.
/// A character is `Cased` if it is uppercase, lowercase, or a titlecase letter
/// (see Unicode Standard, section 3.13).
[[nodiscard]]
bool is_cased(char32_t c);

/// @brief Returns `true` iff `c` has the Unicode `Case_Ignorable` property.
/// Case_Ignorable characters are skipped when applying context-sensitive
/// case transformations such as the Final_Sigma rule
/// (see Unicode Standard, section 3.13).
[[nodiscard]]
bool is_case_ignorable(char32_t c);

} // namespace cowel

#endif // COWEL_UTIL_UNICODE_CASE_PROPS_HPP
