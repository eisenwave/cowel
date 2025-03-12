#ifndef MMML_TYPO_HPP
#define MMML_TYPO_HPP

#include <compare>
#include <cstddef>
#include <memory_resource>
#include <span>
#include <string_view>

namespace mmml {

struct Typo_Result {
    std::u8string_view best_match {};
    std::size_t distance = std::size_t(-1);

    [[nodiscard]]
    constexpr operator bool() const
    {
        return distance != std::size_t(-1);
    }

    [[nodiscard]]
    friend constexpr bool operator==(const Typo_Result& x, const Typo_Result& y)
        = default;

    [[nodiscard]]
    friend constexpr std::strong_ordering
    operator<=>(const Typo_Result& x, const Typo_Result& y) noexcept
    {
        return x.distance <=> y.distance;
    }
};

/// @brief Searches for the given `needle` in the `haystack` based on Levenshtein distance.
/// There may be multiple equally good matches,
// in which case earlier elements are preferred over later elements in the `haystack`.
[[nodiscard]]
Typo_Result closest_match(
    std::span<const std::u8string_view> haystack,
    std::u8string_view needle,
    std::pmr::memory_resource* memory
);

} // namespace mmml

#endif
