#include <cstddef>
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

/// @brief Returns the context-sensitive lowercase mapping of the character
/// at index `pos` in the UTF-32 string `str`.
/// For most characters, this is identical to `unconditional_to_lower`.
/// For U+03A3 GREEK CAPITAL LETTER SIGMA,
/// the Unicode Final_Sigma condition is applied:
/// it yields U+03C2 GREEK SMALL LETTER FINAL SIGMA
/// when the sigma is preceded by a `Cased` character
/// (skipping `Case_Ignorable` characters)
/// and not followed by a `Cased` character
/// (skipping `Case_Ignorable` characters).
/// Otherwise it yields U+03C3 GREEK SMALL LETTER SIGMA.
/// (See Unicode Standard, section 3.13.)
[[nodiscard]]
std::u32string_view contextual_to_lower(std::u32string_view str, std::size_t pos);

} // namespace cowel
