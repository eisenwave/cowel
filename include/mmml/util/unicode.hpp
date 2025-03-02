#ifndef MMML_UNICODE_HPP
#define MMML_UNICODE_HPP

#include <bit>
#include <stdexcept>
#include <string_view>

#include "mmml/util/assert.hpp"

namespace mmml::utf8 {

/// @brief Returns the length of the UTF-8 unit sequence (including `c`)
/// that is encoded when `c` is the first unit in that sequence.
[[nodiscard]]
constexpr int sequence_length(char8_t c)
{
    constexpr auto lookup = 0x43201;
    const int leading_ones = std::countl_one(static_cast<unsigned char>(c));
    return leading_ones > 4 ? 0 : (lookup >> (leading_ones * 4)) & 0xf;
}

struct Code_Point_And_Length {
    char32_t code_point;
    int length;
};

/// @brief Extracts the next code point from UTF-8 data,
/// given a known `length` in range `[0, 4]`.
///
/// The behavior is undefined if `length` is not in that range,
/// or if `str` is not a valid range containing at least `length` characters.
[[nodiscard]]
constexpr char32_t decode_unchecked(const char8_t* str, int length)
{
    MMML_ASSERT(length <= 4);
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

[[nodiscard]]
constexpr Code_Point_And_Length decode_and_length_unchecked(const char8_t* str)
{
    const int length = sequence_length(*str);
    return { .code_point = decode_unchecked(str, length), .length = length };
}

[[nodiscard]]
constexpr char32_t decode_unchecked(const char8_t* str)
{
    return decode_and_length_unchecked(str).code_point;
}

[[nodiscard]]
constexpr Code_Point_And_Length decode_and_length(std::u8string_view str) noexcept
{
    if (str.empty()) {
        return { 0, 0 };
    }
    const int length = sequence_length(str[0]);
    if (str.size() < std::size_t(length)) {
        return { 0, 0 };
    }
    return { .code_point = decode_unchecked(str.data(), length), .length = length };
}

[[nodiscard]]
constexpr bool is_valid(std::u8string_view str) noexcept
{
    while (!str.empty()) {
        const auto [code_point, length] = decode_and_length(str);
        if (length == 0) {
            return false;
        }
        str.remove_prefix(std::size_t(length));
    }
    return true;
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
        return i.m_pointer != i.m_end;
    }

    Code_Point_Iterator& operator++()
    {
        const int length = sequence_length(*m_pointer);
        if (length == 0 || length > m_end - m_pointer) {
            throw Unicode_Error { "Corrupted UTF-8 string or past the end." };
        }
        m_pointer += length;
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
        const auto [code_point, length] = next();
        if (length == 0) {
            throw Unicode_Error { "Corrupted UTF-8 string or past the end." };
        }
        return code_point;
    }

    [[nodiscard]]
    Code_Point_And_Length next() const noexcept
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

// TODO: put into test file

// https://en.wikipedia.org/wiki/UTF-8
static_assert(sequence_length(0b0000'0000) == 1);
static_assert(sequence_length(0b1000'0000) == 0);
static_assert(sequence_length(0b1100'0000) == 2);
static_assert(sequence_length(0b1110'0000) == 3);
static_assert(sequence_length(0b1111'0000) == 4);
static_assert(sequence_length(0b1111'1000) == 0);

static_assert(decode_unchecked(u8"a") == U'a');
static_assert(decode_unchecked(u8"\u00E9") == U'\u00E9');
static_assert(decode_unchecked(u8"\u0905") == U'\u0905');
static_assert(decode_unchecked(u8"\U0001F600") == U'\U0001F600');

} // namespace mmml::utf8

#endif
