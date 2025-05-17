#ifndef COWEL_TYPO_HPP
#define COWEL_TYPO_HPP

#include <compare>
#include <cstddef>
#include <memory_resource>
#include <span>
#include <string_view>

namespace cowel {

template <typename T>
struct Distant {
    T value {};
    std::size_t distance = std::size_t(-1);

    [[nodiscard]]
    constexpr operator bool() const
    {
        return distance != std::size_t(-1);
    }

    [[nodiscard]]
    friend constexpr bool operator==(const Distant& x, const Distant& y)
        = default;

    [[nodiscard]]
    friend constexpr std::strong_ordering operator<=>(const Distant& x, const Distant& y) noexcept
    {
        return x.distance <=> y.distance;
    }
};

/// @brief Searches for the given `needle` in the `haystack` based on Levenshtein distance.
/// There may be multiple equally good matches,
// in which case earlier elements are preferred over later elements in the `haystack`.
[[nodiscard]]
Distant<std::size_t> closest_match(
    std::span<const std::u8string_view> haystack,
    std::u8string_view needle,
    std::pmr::memory_resource* memory
);

} // namespace cowel

#endif
