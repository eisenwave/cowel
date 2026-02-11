#ifndef COWEL_SMALL_VECTOR_HPP
#define COWEL_SMALL_VECTOR_HPP

#include <algorithm>
#include <bit>
#include <cstddef>
#include <iterator>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>

#include "cowel/util/assert.hpp"

namespace cowel {

template <typename T, std::size_t small_cap, typename Alloc = std::allocator<T>>
struct Small_Vector {
public:
    using pointer = std::allocator_traits<Alloc>::pointer;
    using const_pointer = std::allocator_traits<Alloc>::const_pointer;
    using iterator = pointer;
    using const_iterator = const_pointer;
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using difference_type = std::ptrdiff_t;

    static_assert(small_cap != 0, "Cannot create a Small_Vector with zero small capacity.");
    static_assert(
        std::is_default_constructible_v<T>,
        "Element types must be default-constructible. "
        "This is necessary for a constexpr implementation."
    );
    static_assert(
        std::is_same_v<pointer, T*>,
        "Allocators with custom pointer types are not yet supported."
    );
    static_assert(
        std::is_same_v<const_pointer, const T*>,
        "Allocators with custom pointer types are not yet supported."
    );

    static constexpr std::size_t small_capacity_v = small_cap;

private:
    std::size_t m_size = 0;
    std::size_t m_dynamic_capacity = 0;
    bool m_using_small = true;
    [[no_unique_address]]
    Alloc m_alloc {};
    T* m_dynamic_data = nullptr;
    T m_small_data[small_cap] {};

public:
    [[nodiscard]]
    constexpr Small_Vector()
        = default;

    [[nodiscard]]
    constexpr Small_Vector(const Small_Vector& other)
        : m_alloc { other.m_alloc }
    {
        append_range(other);
    }

    [[nodiscard]]
    constexpr Small_Vector(Small_Vector&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
        : m_alloc { std::move(other.m_alloc) }
    {
        move_from(std::move(other));
    }

    [[nodiscard]]
    constexpr explicit Small_Vector(const Alloc& alloc)
        : m_alloc { alloc }
    {
    }

    [[nodiscard]]
    constexpr Small_Vector(std::initializer_list<T> list, const Alloc& alloc = {})
        : m_alloc { alloc }
    {
        append_range(list);
    }

    constexpr Small_Vector& operator=(const Small_Vector& other)
    {
        if (this != &other) {
            destroy_and_deallocate();
            m_alloc = other.m_alloc;
            append_range(other);
        }
        return *this;
    }

    constexpr Small_Vector& operator=(Small_Vector&& other) //
        noexcept(noexcept(std::is_nothrow_move_constructible_v<T>))
    {
        if (this != &other) {
            destroy_and_deallocate();
            m_alloc = std::move(other.m_alloc);
            move_from(std::move(other));
        }
        return *this;
    }

    constexpr ~Small_Vector()
    {
        destroy_and_deallocate();
    }

    [[nodiscard]]
    constexpr std::size_t size() const noexcept
    {
        return m_size;
    }

    [[nodiscard]]
    constexpr bool empty() const noexcept
    {
        return m_size == 0;
    }

    [[nodiscard]]
    constexpr std::size_t capacity() const noexcept
    {
        return m_dynamic_data ? std::max(m_dynamic_capacity, small_cap) : small_cap;
    }

    [[nodiscard]]
    constexpr std::size_t small_capacity() const noexcept
    {
        return small_cap;
    }

    [[nodiscard]]
    constexpr bool small() const noexcept
    {
        return m_using_small;
    }

    [[nodiscard]]
    constexpr Alloc get_allocator() const noexcept
    {
        return m_alloc;
    }

    [[nodiscard]]
    constexpr T& operator[](const std::size_t i)
    {
        COWEL_DEBUG_ASSERT(i < m_size);
        return m_using_small ? m_small_data[i] : m_dynamic_data[i];
    }

    [[nodiscard]]
    constexpr const T& operator[](const std::size_t i) const
    {
        COWEL_DEBUG_ASSERT(i < m_size);
        return m_using_small ? m_small_data[i] : m_dynamic_data[i];
    }

    [[nodiscard]]
    constexpr T& front()
    {
        COWEL_ASSERT(!empty());
        return operator[](0);
    }

    [[nodiscard]]
    constexpr const T& front() const
    {
        COWEL_ASSERT(!empty());
        return operator[](0);
    }

    [[nodiscard]]
    constexpr T& back()
    {
        COWEL_ASSERT(!empty());
        return operator[](m_size - 1);
    }

    [[nodiscard]]
    constexpr const T& back() const
    {
        COWEL_ASSERT(!empty());
        return operator[](m_size - 1);
    }

    [[nodiscard]]
    friend constexpr bool operator==(const Small_Vector& x, const Small_Vector& y)
    {
        return x.m_size == y.m_size && std::ranges::equal(x, y);
    }

    [[nodiscard]]
    friend constexpr auto operator<=>(const Small_Vector& x, const Small_Vector& y)
        -> decltype(x.front() <=> y.front())
    {
        return std::lexicographical_compare_three_way(x.begin(), x.end(), y.begin(), y.end());
    }

    constexpr void clear() noexcept
    {
        if (!m_using_small) {
            std::ranges::destroy_n(m_dynamic_data, difference_type(m_size));
        }
        m_size = 0;
        m_using_small = true;
    }

    constexpr void reserve(const std::size_t amount)
    {
        if (amount <= capacity()) {
            return;
        }
        grow_to(amount);
    }

    constexpr void swap(Small_Vector& other) noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        if (this == &other) {
            return;
        }

        if (!m_using_small && !other.m_using_small) {
            std::swap(m_dynamic_data, other.m_dynamic_data);
            std::swap(m_dynamic_capacity, other.m_dynamic_capacity);
            std::swap(m_size, other.m_size);
            std::swap(m_using_small, other.m_using_small);
            std::swap(m_alloc, other.m_alloc);
            return;
        }

        if (m_using_small && other.m_using_small) {
            const auto common = std::min(m_size, other.m_size);
            std::ranges::swap_ranges(
                m_small_data, //
                m_small_data + common, //
                other.m_small_data, //
                other.m_small_data + common
            );
            if (m_size > other.m_size) {
                std::ranges::move(
                    m_small_data + common, //
                    m_small_data + m_size, //
                    other.m_small_data + common
                );
            }
            else if (other.m_size > m_size) {
                std::ranges::move(
                    other.m_small_data + common, //
                    other.m_small_data + other.m_size, //
                    m_small_data + common
                );
            }
            std::swap(m_size, other.m_size);
            std::swap(m_dynamic_data, other.m_dynamic_data);
            std::swap(m_dynamic_capacity, other.m_dynamic_capacity);
            std::swap(m_alloc, other.m_alloc);
            return;
        }

        if (!m_using_small && other.m_using_small) {
            std::ranges::move(other.m_small_data, other.m_small_data + other.m_size, m_small_data);
            std::swap(m_dynamic_data, other.m_dynamic_data);
            std::swap(m_dynamic_capacity, other.m_dynamic_capacity);
            std::swap(m_size, other.m_size);
            std::swap(m_using_small, other.m_using_small);
            std::swap(m_alloc, other.m_alloc);
            return;
        }

        if (m_using_small && !other.m_using_small) {
            std::ranges::move(m_small_data, m_small_data + m_size, other.m_small_data);
            std::swap(m_dynamic_data, other.m_dynamic_data);
            std::swap(m_dynamic_capacity, other.m_dynamic_capacity);
            std::swap(m_size, other.m_size);
            std::swap(m_using_small, other.m_using_small);
            std::swap(m_alloc, other.m_alloc);
            return;
        }

        COWEL_ASSERT_UNREACHABLE(u8"Logical error.");
    }

    friend constexpr void swap(Small_Vector& x, Small_Vector& y) noexcept(noexcept(x.swap(y)))
    {
        x.swap(y);
    }

    constexpr void push_back(const T& element)
    {
        emplace_back(element);
    }

    constexpr void push_back(T&& element)
    {
        emplace_back(std::move(element));
    }

    template <typename... Args>
        requires std::constructible_from<T, Args&&...>
    constexpr T& emplace_back(Args&&... args)
    {
        if (m_using_small && m_size < small_cap) {
            m_small_data[m_size] = T(std::forward<Args>(args)...);
            return m_small_data[m_size++];
        }
        ensure_dynamic_storage(m_size + 1);
        return *std::construct_at(
            m_dynamic_data + m_size++, //
            std::forward<Args>(args)...
        );
    }

    constexpr void pop_back()
    {
        COWEL_ASSERT(!empty());
        if (!m_using_small) {
            std::destroy_at(m_dynamic_data + m_size - 1);
        }
        --m_size;
        if (!m_using_small && m_size <= small_cap) {
            move_dynamic_to_small();
        }
    }

    /// @brief Inserts the range `[begin, sentinel)` immediately prior to `pos`.
    /// @param pos The position before which to insert the elements.
    /// If this is `end()`, the elements are appended at the end of the vector.
    /// @param begin The start of the inserted range.
    /// @param sentinel A sentinel delimiting the inserted range.
    template <std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel>
    constexpr void insert(iterator pos, Iterator begin, Sentinel sentinel)
    {
        const auto pos_index = std::size_t(pos - this->begin());

        if constexpr (requires { sentinel - begin; }) {
            const auto raw_count = sentinel - begin;
            COWEL_ASSERT(raw_count >= 0);
            const auto count = std::size_t(raw_count);
            const std::size_t new_size = m_size + count;

            if (m_using_small && new_size <= small_cap) {
                std::ranges::move_backward(
                    m_small_data + pos_index, //
                    m_small_data + m_size, //
                    m_small_data + new_size
                );
                std::ranges::copy_n(
                    begin, //
                    std::iter_difference_t<Iterator>(count), //
                    m_small_data + pos_index
                );
                m_size = new_size;
                return;
            }

            ensure_dynamic_storage(new_size);
            const std::size_t shift_count = m_size - pos_index;
            if (count <= shift_count) {
                std::ranges::uninitialized_move_n(
                    m_dynamic_data + (m_size - count), //
                    difference_type(count), //
                    m_dynamic_data + m_size, //
                    m_dynamic_data + m_size + count
                );
                std::ranges::move_backward(
                    m_dynamic_data + pos_index, //
                    m_dynamic_data + (m_size - count), //
                    m_dynamic_data + m_size
                );
            }
            else {
                std::ranges::uninitialized_move_n(
                    m_dynamic_data + pos_index, //
                    difference_type(shift_count), //
                    m_dynamic_data + pos_index + count, //
                    m_dynamic_data + pos_index + count + shift_count
                );
            }

            const std::size_t existing_count = std::min(count, m_size - pos_index);
            const auto existing_result = std::ranges::copy_n(
                begin, //
                std::iter_difference_t<Iterator>(existing_count), //
                m_dynamic_data + pos_index
            );
            const std::size_t insert_tail_count = count - existing_count;
            if (insert_tail_count != 0) {
                std::ranges::uninitialized_copy_n(
                    existing_result.in,
                    std::iter_difference_t<Iterator>(insert_tail_count), //
                    m_dynamic_data + pos_index + existing_count,
                    m_dynamic_data + pos_index + existing_count + insert_tail_count
                );
            }
            m_size = new_size;
        }
        else {
            std::size_t count = 0;
            for (auto it = begin; it != sentinel; ++it) {
                push_back(*it);
                ++count;
            }
            if (count == 0) {
                return;
            }
            std::rotate(this->begin() + pos_index, this->end() - count, this->end());
        }
    }

    template <std::ranges::range R>
    constexpr void append_range(R&& r)
    {
        insert(
            end(), //
            std::ranges::begin(std::forward<R>(r)), //
            std::ranges::end(std::forward<R>(r))
        );
    }

    [[nodiscard]]
    constexpr iterator begin() noexcept
    {
        return m_using_small ? std::ranges::begin(m_small_data) //
                             : m_dynamic_data;
    }
    [[nodiscard]]
    constexpr const_iterator begin() const noexcept
    {
        return m_using_small ? std::ranges::begin(m_small_data) //
                             : m_dynamic_data;
    }
    [[nodiscard]]
    constexpr const_iterator cbegin() const noexcept
    {
        return begin();
    }

    [[nodiscard]]
    constexpr iterator end() noexcept
    {
        return m_using_small ? std::ranges::begin(m_small_data) + m_size //
                             : m_dynamic_data + m_size;
    }
    [[nodiscard]]
    constexpr const_iterator end() const noexcept
    {
        return m_using_small ? std::ranges::begin(m_small_data) + m_size //
                             : m_dynamic_data + m_size;
    }
    [[nodiscard]]
    constexpr const_iterator cend() const noexcept
    {
        return end();
    }

private:
    constexpr void destroy_and_deallocate() noexcept
    {
        clear();
        if (m_dynamic_data) {
            std::allocator_traits<Alloc>::deallocate(m_alloc, m_dynamic_data, m_dynamic_capacity);
            m_dynamic_data = nullptr;
            m_dynamic_capacity = 0;
            m_using_small = true;
        }
    }

    // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
    constexpr void move_from(Small_Vector&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        if (!other.m_using_small) {
            m_dynamic_data = std::exchange(other.m_dynamic_data, nullptr);
            m_dynamic_capacity = std::exchange(other.m_dynamic_capacity, 0);
            m_size = std::exchange(other.m_size, 0);
            m_using_small = false;
            other.m_using_small = true;
            return;
        }

        m_size = std::exchange(other.m_size, 0);
        std::ranges::move(other.m_small_data, other.m_small_data + m_size, m_small_data);
        m_using_small = true;
        if (other.m_dynamic_data) {
            m_dynamic_data = std::exchange(other.m_dynamic_data, nullptr);
            m_dynamic_capacity = std::exchange(other.m_dynamic_capacity, 0);
        }
    }

    [[nodiscard]]
    constexpr std::size_t next_capacity(const std::size_t min_needed) const
    {
        return std::max(
            {
                2 * small_cap,
                m_dynamic_capacity * 2,
                std::bit_ceil(min_needed),
            }
        );
    }

    constexpr void move_dynamic_to_small()
    {
        std::ranges::move(m_dynamic_data, m_dynamic_data + m_size, m_small_data);
        std::ranges::destroy_n(m_dynamic_data, difference_type(m_size));
        m_using_small = true;
    }

    constexpr void move_small_to_dynamic(T* const target)
    {
        std::ranges::uninitialized_move_n(
            m_small_data, difference_type(m_size), target, target + m_size
        );
        m_using_small = false;
    }

    constexpr void ensure_dynamic_storage(const std::size_t min_needed)
    {
        if (!m_dynamic_data || m_dynamic_capacity < min_needed) {
            grow_to(min_needed);
            return;
        }
        if (m_using_small) {
            move_small_to_dynamic(m_dynamic_data);
        }
    }

    constexpr void grow_to(const std::size_t min_needed)
    {
        const std::size_t new_capacity = next_capacity(min_needed);
        T* const new_data = std::allocator_traits<Alloc>::allocate(m_alloc, new_capacity);

        if (m_dynamic_data && !m_using_small) {
            std::ranges::uninitialized_move_n(
                m_dynamic_data, difference_type(m_size), new_data, new_data + m_size
            );
            std::ranges::destroy_n(m_dynamic_data, difference_type(m_size));
        }
        else {
            std::ranges::uninitialized_move_n(
                m_small_data, difference_type(m_size), new_data, new_data + m_size
            );
        }

        if (m_dynamic_data) {
            std::allocator_traits<Alloc>::deallocate(m_alloc, m_dynamic_data, m_dynamic_capacity);
        }

        m_dynamic_data = new_data;
        m_dynamic_capacity = new_capacity;
        m_using_small = false;
    }
};

} // namespace cowel

#endif
