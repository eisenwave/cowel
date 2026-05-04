#include <string_view>

namespace cowel {

/// @brief Returns the value of the `Simple_Uppercase_Mapping` property of `c`,
/// or `c` itself if `c` is not a code point with such a property.
[[nodiscard]]
char32_t simple_to_upper(char32_t c);

/// @brief Returns the value of the `Simple_Lowercase_Mapping` property of `c`,
/// or `c` itself if `c` is not a code point with such a property.
[[nodiscard]]
char32_t simple_to_lower(char32_t c);

/// @brief Returns the code point `c` transformed to uppercase
/// using either the `Simple_Uppercase_Mapping` property of `c` if available,
/// or using one of the unconditional special case mappings,
/// or an empty string if no mapping exists.
[[nodiscard]]
std::u32string_view unconditional_to_upper(char32_t c);

/// @brief Returns the code point `c` transformed to lowercase
/// using either the `Simple_Lowercase_Mapping` property of `c` if available,
/// or using one of the unconditional special case mappings,
/// or an empty string if no mapping exists.
[[nodiscard]]
std::u32string_view unconditional_to_lower(char32_t c);

} // namespace cowel
