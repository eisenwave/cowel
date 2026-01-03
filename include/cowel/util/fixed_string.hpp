#ifndef COWEL_FIXED_STRING_HPP
#define COWEL_FIXED_STRING_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <string_view>

#include "cowel/util/assert.hpp"
#include "cowel/util/meta.hpp"

namespace cowel {

template <char_like Char, std::size_t capacity>
struct Basic_Fixed_String {
    using array_type = std::array<Char, capacity>;
    using iterator = array_type::iterator;
    using const_iterator = array_type::const_iterator;
    static constexpr std::size_t max_size_v = capacity;

private:
    array_type m_buffer {};
    std::size_t m_length = 0;

public:
    [[nodiscard]]
    constexpr Basic_Fixed_String(const Char* str, std::size_t length)
        : m_length { length }
    {
        COWEL_ASSERT(length <= capacity);
        std::ranges::copy(str, str + length, m_buffer.data());
    }

    [[nodiscard]]
    constexpr Basic_Fixed_String(array_type array, std::size_t length)
        : m_buffer { array }
        , m_length { length }
    {
        COWEL_ASSERT(length <= capacity);
    }

    [[nodiscard]]
    constexpr Basic_Fixed_String(std::basic_string_view<Char> str)
        : Basic_Fixed_String { str.data(), str.size() }
    {
    }

    [[nodiscard]]
    constexpr Basic_Fixed_String(Char c) noexcept
        requires(capacity != 0)
        : m_buffer { c }
        , m_length { 1 }
    {
    }

    [[nodiscard]]
    constexpr Basic_Fixed_String() noexcept
        = default;

    [[nodiscard]]
    constexpr bool empty() const noexcept
    {
        return m_length == 0;
    }

    [[nodiscard]]
    constexpr std::size_t max_size() noexcept
    {
        return capacity;
    }
    [[nodiscard]]
    constexpr std::size_t size() const noexcept
    {
        return m_length;
    }

    [[nodiscard]]
    constexpr std::size_t length() const noexcept
    {
        return m_length;
    }

    [[nodiscard]]
    constexpr Char* data() noexcept
    {
        return m_buffer.data();
    }

    [[nodiscard]]
    constexpr const Char* data() const noexcept
    {
        return m_buffer.data();
    }

    constexpr void clear() noexcept
    {
        m_length = 0;
    }

    [[nodiscard]]
    constexpr iterator begin() noexcept
    {
        return m_buffer.begin();
    }
    [[nodiscard]]
    constexpr const_iterator begin() const noexcept
    {
        return m_buffer.begin();
    }

    [[nodiscard]]
    constexpr iterator end() noexcept
    {
        return m_buffer.begin() + std::ptrdiff_t(m_length);
    }
    [[nodiscard]]
    constexpr const_iterator end() const noexcept
    {
        return m_buffer.begin() + std::ptrdiff_t(m_length);
    }

    constexpr void erase(std::size_t index)
    {
        COWEL_ASSERT(index < m_length);
        std::shift_left(m_buffer.begin() + std::ptrdiff_t(index), m_buffer.end(), 1);
        m_length -= 1;
    }

    constexpr void remove_prefix(std::size_t n)
    {
        // TODO: Use std::ranges overload once available.
        COWEL_ASSERT(n <= m_length);
        std::shift_left(m_buffer.begin(), m_buffer.end(), std::ptrdiff_t(n));
        m_length -= n;
    }

    constexpr void remove_suffix(std::size_t n)
    {
        COWEL_ASSERT(n <= m_length);
        m_length -= n;
    }

    [[nodiscard]]
    constexpr const array_type& as_array() const
    {
        return m_buffer;
    }

    [[nodiscard]]
    constexpr std::span<Char> as_span()
    {
        return { m_buffer.data(), m_length };
    }

    [[nodiscard]]
    constexpr std::span<const Char> as_span() const
    {
        return { m_buffer.data(), m_length };
    }

    [[nodiscard]]
    constexpr std::basic_string_view<Char> as_string() const
    {
        return { m_buffer.data(), m_length };
    }

    [[nodiscard]]
    constexpr operator std::basic_string_view<Char>() const
    {
        return as_string();
    }

    [[nodiscard]]
    friend constexpr bool
    operator==(const Basic_Fixed_String& x, const Basic_Fixed_String& y) noexcept
    {
        return x.as_string() == y.as_string();
    }

    [[nodiscard]]
    friend constexpr bool
    operator==(const Basic_Fixed_String& x, std::basic_string_view<Char> y) noexcept
    {
        return x.as_string() == y;
    }

    [[nodiscard]]
    friend constexpr bool
    operator<=>(const Basic_Fixed_String& x, const Basic_Fixed_String& y) noexcept
    {
        return x.as_string() <=> y.as_string();
    }

    [[nodiscard]]
    friend constexpr bool
    operator<=>(const Basic_Fixed_String& x, std::basic_string_view<Char> y) noexcept
    {
        return x.as_string() <=> y;
    }
};

template <std::size_t capacity>
using Fixed_String = Basic_Fixed_String<char, capacity>;
template <std::size_t capacity>
using Fixed_String8 = Basic_Fixed_String<char8_t, capacity>;

} // namespace cowel

#endif
