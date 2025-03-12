#ifndef MMML_UNICODE_HPP
#define MMML_UNICODE_HPP

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <string_view>

#include "mmml/util/assert.hpp"
#include "mmml/util/result.hpp"

#include "mmml/fwd.hpp"

namespace mmml::utf8 {

/// @brief Returns the length of the UTF-8 unit sequence (including `c`)
/// that is encoded when `c` is the first unit in that sequence.
///
/// Returns `0` if `c` is not a valid leading code unit,
/// such as if it begins with `10` or `111110`.
[[nodiscard]]
constexpr int sequence_length(char8_t c) noexcept
{
    constexpr unsigned long lookup = 0b100'011'010'000'001;
    const int leading_ones = std::countl_one(static_cast<unsigned char>(c));
    return leading_ones > 4 ? 0 : int((lookup >> (leading_ones * 3)) & 0x7);
}

struct Code_Point_And_Length {
    char32_t code_point;
    int length;
};

enum struct Error_Code : Default_Underlying {
    /// @brief Attempted to obtain unicode data from an empty string.
    no_data,
    /// @brief The bits in the initial unit would require there to be more subsequent units
    /// than actually exist.
    missing_units,
    /// @brief The bit pattern is not a valid sequence of UTF-8 code units.
    /// For example, the trailing code units don't have `10` continuation bits.
    illegal_bits
};

/// @brief Extracts the next code point from UTF-8 data,
/// given a known `length`.
/// No checks for the validity of the UTF-8 data are performed,
/// such as whether continuation bits are present.
/// @param str The UTF-8 units.
/// Only the first `length` units are used for decoding.
/// @param length The amount of UTF-8 units stored in `str`,
/// in range `[1, 4]`.
[[nodiscard]]
constexpr char32_t decode_unchecked(std::array<char8_t, 4> str, int length)
{
    MMML_ASSERT(length >= 1 && length <= 4);
    // TODO: this could be optimized using bit_compress (i.e. PEXT instruction)
    // clang-format off
    switch (length) {
    case 1:
        return str[0];
    case 2:
        return (char32_t(str[0] & 0x1f) << 6)
             | (char32_t(str[1] & 0x3f) << 0);
    case 3:
        return (char32_t(str[0] & 0x0f) << 12)
             | (char32_t(str[1] & 0x3f) << 6)
             | (char32_t(str[2] & 0x3f) << 0);
    case 4:
        return (char32_t(str[0] & 0x07) << 18)
             | (char32_t(str[1] & 0x3f) << 12)
             | (char32_t(str[2] & 0x3f) << 6)
             | (char32_t(str[3] & 0x3f) << 0);
    default:
        return 0;
    }
    // clang-format on
}

namespace detail {

/// @brief For any sequence length minus one,
/// contains a mask where a bits is `1` if the corresponding bits in the code units
/// are expected to have a constant value.
///
/// For example, for a sequence length of `1`,
/// the uppermost bit in the first byte is expected to be zero,
/// so the mask at `[0]` contains a `1`-bit in that position and a `0`-bit everywhere else.
inline constexpr char8_t expectation_masks[][4] = {
    { 0x80, 0x00, 0x00, 0x00 },
    { 0xE0, 0xC0, 0x00, 0x00 },
    { 0xF0, 0xC0, 0xC0, 0x00 },
    { 0xF8, 0xC0, 0xC0, 0xC0 },
};

/// @brief For any sequence length minus one,
/// contains the bit patterns of any constant bits in the code units.
inline constexpr char8_t expectation_values[][4] = {
    { 0x00, 0x00, 0x00, 0x00 },
    { 0xC0, 0x80, 0x00, 0x00 },
    { 0xE0, 0x80, 0x80, 0x00 },
    { 0xF0, 0x80, 0x80, 0x80 },
};

} // namespace detail

/// @brief Returns `true` if `str` contains a valid (padded) UTF-8-encoded code point
/// for a known sequence length.
/// @param str The UTF-8 code units. Paddings bits need not be zero.
/// @param length The length of the code unit sequence, in `[1, 4]`.
[[nodiscard]]
constexpr bool is_valid(std::array<char8_t, 4> str, int length)
{
    MMML_ASSERT(length >= 1 && length <= 4);

    const auto str32 = std::bit_cast<std::uint32_t>(str);
    const auto mask = std::bit_cast<std::uint32_t>(detail::expectation_masks[length - 1]);
    const auto expected = std::bit_cast<std::uint32_t>(detail::expectation_values[length - 1]);

    // https://nrk.neocities.org/articles/utf8-pext
    return (str32 & mask) == expected;
}

/// @brief Returns the UTF-8-encoded code point within the units pointed to by `str`,
/// as well as the amount of UTF-8 units encoding that code point.
[[nodiscard]]
constexpr Code_Point_And_Length decode_and_length_unchecked(const char8_t* str)
{
    const int length = sequence_length(*str);
    std::array<char8_t, 4> padded {};
    std::copy(str, str + length, padded.data());
    return { .code_point = decode_unchecked(padded, length), .length = length };
}

/// @brief Returns the UTF-8-encoded code point within the units pointed to by `str`.
/// No bounds checks are performed.
[[nodiscard]]
constexpr char32_t decode_unchecked(const char8_t* str)
{
    return decode_and_length_unchecked(str).code_point;
}

/// @brief Like `decode_unchecked`,
/// but checks the integrity of the given UTF-8 data,
/// such as that continuation bits are present and have their expected value.
/// @param str The UTF-8 units.
/// Only the first `length` units are used for decoding.
/// @param length The amount of UTF-8 units stored in `str`,
/// in range `[1, 4]`.
[[nodiscard]]
constexpr Result<char32_t, Error_Code> decode(std::array<char8_t, 4> str, int length)
{
    MMML_ASSERT(length >= 1 && length <= 4);
    if (!is_valid(str, length)) {
        return Error_Code::illegal_bits;
    }

    return decode_unchecked(str, length);
}

[[nodiscard]]
constexpr Result<Code_Point_And_Length, Error_Code> //
decode_and_length(std::u8string_view str) noexcept // NOLINT(bugprone-exception-escape)
{
    if (str.empty()) {
        return Error_Code::no_data;
    }
    const int length = sequence_length(str[0]);
    if (length == 0) {
        return Error_Code::illegal_bits;
    }
    if (str.size() < std::size_t(length)) {
        return Error_Code::missing_units;
    }
    std::array<char8_t, 4> padded {};
    std::copy(str.data(), str.data() + length, padded.data());
    Result<char32_t, Error_Code> result = decode(padded, length);
    if (!result) {
        return result.error();
    }

    return Code_Point_And_Length { .code_point = *result, .length = length };
}

[[nodiscard]]
constexpr Result<void, Error_Code> is_valid(std::u8string_view str) noexcept
{
    while (!str.empty()) {
        const Result<Code_Point_And_Length, Error_Code> next = decode_and_length(str);
        if (!next) {
            return next.error();
        }
        str.remove_prefix(std::size_t(next->length));
    }
    return {};
}

/// @brief Returns the number of code points in `str`.
/// The behavior is undefined if `str` is not a valid UTF-8 string.
[[nodiscard]]
constexpr std::size_t code_points_unchecked(std::u8string_view str) noexcept
{
    std::size_t result = 0;
    while (!str.empty()) {
        const auto unit_length = std::size_t(sequence_length(str.front()));
        str.remove_prefix(unit_length);
        ++result;
    }
    return result;
}

/// @brief Thrown when decoding unicode strings fails.
struct Unicode_Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Code_Point_Iterator_Sentinel { };

struct Code_Point_Iterator {
    using difference_type = std::ptrdiff_t;
    using value_type = char32_t;
    using Sentinel = Code_Point_Iterator_Sentinel;

private:
    const char8_t* m_pointer = nullptr;
    const char8_t* m_end = nullptr;

public:
    Code_Point_Iterator() noexcept = default;

    Code_Point_Iterator(std::u8string_view str) noexcept
        : m_pointer { str.data() }
        , m_end { str.data() + str.size() }
    {
    }

    [[nodiscard]]
    friend bool operator==(Code_Point_Iterator, Code_Point_Iterator) noexcept
        = default;

    [[nodiscard]]
    friend bool operator==(Code_Point_Iterator i, Code_Point_Iterator_Sentinel) noexcept
    {
        return i.m_pointer == i.m_end;
    }

    Code_Point_Iterator& operator++()
    {
        const int length = sequence_length(*m_pointer);
        if (length == 0 || length > m_end - m_pointer) {
            throw Unicode_Error { "Corrupted UTF-8 string or past the end." };
        }
        m_pointer += length;
        return *this;
    }

    Code_Point_Iterator operator++(int)
    {
        Code_Point_Iterator copy = *this;
        ++*this;
        return copy;
    }

    [[nodiscard]]
    char32_t operator*() const
    {
        const Result<Code_Point_And_Length, Error_Code> result = next();
        if (!result) {
            throw Unicode_Error { "Corrupted UTF-8 string or past the end." };
        }
        return result->code_point;
    }

    [[nodiscard]]
    Result<Code_Point_And_Length, Error_Code> next() const noexcept
    {
        const std::u8string_view str { m_pointer, m_end };
        return decode_and_length(str);
    }
};

static_assert(std::sentinel_for<Code_Point_Iterator_Sentinel, Code_Point_Iterator>);
static_assert(std::forward_iterator<Code_Point_Iterator>);

struct Code_Point_View {
    using iterator = Code_Point_Iterator;
    using const_iterator = Code_Point_Iterator;

    std::u8string_view string;

    [[nodiscard]]
    iterator begin() const noexcept
    {
        return iterator { string };
    }

    [[nodiscard]]
    iterator cbegin() const noexcept
    {
        return begin();
    }

    [[nodiscard]]
    Code_Point_Iterator_Sentinel end() const noexcept
    {
        return {};
    }

    [[nodiscard]]
    Code_Point_Iterator_Sentinel cend() const noexcept
    {
        return {};
    }
};

static_assert(std::ranges::forward_range<Code_Point_View>);

} // namespace mmml::utf8

#endif
