#ifndef COWEL_BUFFER_HPP
#define COWEL_BUFFER_HPP

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <span>

#include "cowel/util/assert.hpp"

#include "cowel/settings.hpp"

namespace cowel {

template <typename T, std::size_t cap, std::invocable<std::span<T>> Sink>
    requires(cap != 0)
struct Buffer {
    using value_type = T;

private:
    value_type m_buffer[cap];
    [[no_unique_address]]
    Sink m_sink;

    std::size_t m_size = 0;

public:
    [[nodiscard]]
    constexpr Buffer(const Sink& sink) noexcept(std::is_nothrow_copy_constructible_v<Sink>)
        : m_sink { sink }
    {
    }
    [[nodiscard]]
    constexpr Buffer(Sink&& sink = Sink {}) noexcept(std::is_nothrow_move_constructible_v<Sink>)
        : m_sink { std::move(sink) }
    {
    }

    [[nodiscard]]
    Buffer(const Buffer&)
        = default;
    [[nodiscard]]
    Buffer(Buffer&&)
        = default;

    Buffer& operator=(const Buffer&) = default;
    Buffer& operator=(Buffer&&) = default;

    constexpr ~Buffer()
    {
#ifdef COWEL_EXCEPTIONS
        try {
#endif
            flush();
#ifdef COWEL_EXCEPTIONS
        } catch (...) {
            ULIGHT_DEBUG_ASSERT_UNREACHABLE(u8"Exception caught during buffer flush.");
        }
#endif
    }

    /// @brief Returns the amount of elements that can be appended to the buffer before
    /// flushing.
    [[nodiscard]]
    constexpr std::size_t capacity() const noexcept
    {
        return cap;
    }

    /// @brief Returns the number of elements currently in the buffer.
    /// `size() <= capacity()` is always `true`.
    [[nodiscard]]
    constexpr std::size_t size() const noexcept
    {
        return m_size;
    }

    /// @brief Equivalent to `capacity() - size()`.
    [[nodiscard]]
    constexpr std::size_t available() const noexcept
    {
        return cap - m_size;
    }

    [[nodiscard]]
    constexpr value_type* data() noexcept
    {
        return m_buffer;
    }
    [[nodiscard]]
    constexpr const value_type* data() const noexcept
    {
        return m_buffer;
    }

    /// @brief Equivalent to `available() == 0`.
    [[nodiscard]]
    constexpr bool full() const noexcept
    {
        return m_size == cap;
    }

    /// @brief Equivalent to `size() == 0`.
    [[nodiscard]]
    constexpr bool empty() const noexcept
    {
        return m_size == 0;
    }

    /// @brief Sets the size to zero without flushing.
    constexpr void clear() noexcept
    {
        m_size = 0;
    }

    COWEL_HOT
    constexpr value_type& push_back(const value_type& e)
        requires std::is_copy_assignable_v<value_type>
    {
        if (full()) {
            flush();
        }
        return m_buffer[m_size++] = e;
    }

    COWEL_HOT
    constexpr value_type& push_back(value_type&& e)
        requires std::is_move_assignable_v<value_type>
    {
        if (full()) {
            flush();
        }
        return m_buffer[m_size++] = std::move(e);
    }

    template <typename... Args>
    constexpr value_type& emplace_back(Args&&... args)
        requires std::is_constructible_v<value_type, Args&&...>
        && std::is_move_assignable_v<value_type>
    {
        if (full()) {
            flush();
        }
        return m_buffer[m_size++] = value_type(std::forward<Args>(args)...);
    }

    template <std::input_iterator Iter, std::sentinel_for<Iter> Sentinel>
        requires std::convertible_to<std::iter_reference_t<Iter>, value_type>
    constexpr void append(Iter begin, Sentinel end)
    {
        for (; begin != end; ++begin) {
            push_back(*begin);
        }
    }

    template <std::random_access_iterator Iter>
        requires std::convertible_to<std::iter_reference_t<Iter>, value_type>
    constexpr void append(Iter begin, Iter end)
    {
        COWEL_DEBUG_ASSERT(begin <= end);
        using Diff = std::iter_difference_t<Iter>;
        while (begin != end) {
            if (full()) {
                flush();
            }
            const auto chunk_size = std::min(available(), std::size_t(end - begin));
            COWEL_DEBUG_ASSERT(chunk_size != 0);
            COWEL_DEBUG_ASSERT(begin + Diff(chunk_size) <= end);
            COWEL_DEBUG_ASSERT(m_size + chunk_size <= cap);

            std::ranges::copy(begin, begin + Diff(chunk_size), m_buffer + m_size);
            begin += Diff(chunk_size);
            m_size += chunk_size;
        }
    }

    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_reference_t<R>, value_type>
    constexpr void append_range(R&& range)
    {
        append(
            std::ranges::begin(std::forward<R>(range)), std::ranges::end(std::forward<R>(range))
        );
    }

    template <std::invocable<std::span<value_type>> F>
    constexpr void append_in_place(std::size_t count, F f)
    {
        while (count != 0) {
            if (full()) {
                flush();
            }
            const std::size_t inserted_count = std::min(count, available());
            const std::span<value_type> output_span { data() + m_size, inserted_count };
            f(output_span);
            m_size += inserted_count;
            count -= inserted_count;
        }
    }

    [[nodiscard]]
    constexpr value_type& back()
    {
        COWEL_ASSERT(!empty());
        return m_buffer[m_size - 1];
    }
    [[nodiscard]]
    constexpr const value_type& back() const
    {
        COWEL_ASSERT(!empty());
        return m_buffer[m_size - 1];
    }

    /// @brief Returns a span containing what is currently in the buffer.
    /// This view is invalidated by any operation which changes buffer contents.
    [[nodiscard]]
    constexpr std::span<value_type> span() noexcept
    {
        return { data(), size() };
    }
    /// @brief Returns a span containing what is currently in the buffer.
    /// This view is invalidated by any operation which changes buffer contents.
    [[nodiscard]]
    constexpr std::span<const value_type> span() const noexcept
    {
        return { data(), size() };
    }

    /// @brief Writes any buffered text to the underlying sink and empties the buffer.
    constexpr void flush()
    {
        if (m_size != 0) {
            m_sink(span());
            m_size = 0;
        }
    }
};

} // namespace cowel

#endif
