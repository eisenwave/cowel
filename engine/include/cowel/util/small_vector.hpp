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
    union Small_Storage {
        constexpr Small_Storage() noexcept { }
        constexpr Small_Storage(const Small_Storage&) { }
        // NOLINTNEXTLINE(bugprone-unhandled-self-assignment)
        constexpr Small_Storage& operator=(const Small_Storage&)
        {
            return *this;
        }
        constexpr ~Small_Storage() noexcept { }
        T data[small_cap];
    } m_small_storage;

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
    constexpr explicit Small_Vector(const std::size_t amount, const Alloc& alloc = {})
        : m_alloc { alloc }
    {
        resize(amount);
    }
    [[nodiscard]]
    constexpr explicit Small_Vector(
        const std::size_t amount,
        const T& value,
        const Alloc& alloc = {}
    )
        : m_alloc { alloc }
    {
        resize(amount, value);
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
    T& operator[](const std::size_t i)
    {
        COWEL_DEBUG_ASSERT(i < m_size);
        return m_using_small ? small_data()[i] : m_dynamic_data[i];
    }

    [[nodiscard]]
    const T& operator[](const std::size_t i) const
    {
        COWEL_DEBUG_ASSERT(i < m_size);
        return m_using_small ? small_data()[i] : m_dynamic_data[i];
    }

    [[nodiscard]]
    T& front()
    {
        COWEL_ASSERT(!empty());
        return operator[](0);
    }

    [[nodiscard]]
    const T& front() const
    {
        COWEL_ASSERT(!empty());
        return operator[](0);
    }

    [[nodiscard]]
    T& back()
    {
        COWEL_ASSERT(!empty());
        return operator[](m_size - 1);
    }

    [[nodiscard]]
    const T& back() const
    {
        COWEL_ASSERT(!empty());
        return operator[](m_size - 1);
    }

    [[nodiscard]]
    friend constexpr bool operator==(const Small_Vector& x, const Small_Vector& y)
        requires std::equality_comparable<T>
    {
        return x.m_size == y.m_size && std::ranges::equal(x, y);
    }

    [[nodiscard]]
    friend constexpr auto operator<=>(const Small_Vector& x, const Small_Vector& y)
        requires std::three_way_comparable<T>
    {
        return std::lexicographical_compare_three_way(x.begin(), x.end(), y.begin(), y.end());
    }

    constexpr void clear() noexcept
    {
        if (m_using_small) {
            std::ranges::destroy_n(small_data(), difference_type(m_size));
        }
        else {
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

    constexpr void resize(const std::size_t amount, const T& value = {})
    {
        if (amount == 0) {
            clear();
            return;
        }
        if (amount < m_size) {
            if (m_using_small) {
                std::ranges::destroy(small_data() + amount, small_data() + m_size);
            }
            else {
                std::ranges::destroy(m_dynamic_data + amount, m_dynamic_data + m_size);
            }
        }
        else if (amount > m_size) {
            if (m_using_small && amount <= small_cap) {
                std::ranges::uninitialized_fill(
                    small_data() + m_size, small_data() + amount, value
                );
            }
            else {
                ensure_dynamic_storage(amount);
                std::ranges::uninitialized_fill(
                    m_dynamic_data + m_size, m_dynamic_data + amount, value
                );
            }
        }
        m_size = amount;
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
            T* const sd = small_data();
            T* const osd = other.small_data();
            std::ranges::swap_ranges(sd, sd + common, osd, osd + common);
            if (m_size > other.m_size) {
                std::ranges::uninitialized_move_n(
                    sd + common, difference_type(m_size - common), osd + common, osd + m_size
                );
                std::ranges::destroy_n(sd + common, difference_type(m_size - common));
            }
            else if (other.m_size > m_size) {
                std::ranges::uninitialized_move_n(
                    osd + common, difference_type(other.m_size - common), sd + common,
                    sd + other.m_size
                );
                std::ranges::destroy_n(osd + common, difference_type(other.m_size - common));
            }
            std::swap(m_size, other.m_size);
            std::swap(m_dynamic_data, other.m_dynamic_data);
            std::swap(m_dynamic_capacity, other.m_dynamic_capacity);
            std::swap(m_alloc, other.m_alloc);
            return;
        }

        if (!m_using_small && other.m_using_small) {
            std::ranges::uninitialized_move_n(
                other.small_data(), difference_type(other.m_size), small_data(),
                small_data() + other.m_size
            );
            std::ranges::destroy_n(other.small_data(), difference_type(other.m_size));
            std::swap(m_dynamic_data, other.m_dynamic_data);
            std::swap(m_dynamic_capacity, other.m_dynamic_capacity);
            std::swap(m_size, other.m_size);
            std::swap(m_using_small, other.m_using_small);
            std::swap(m_alloc, other.m_alloc);
            return;
        }

        if (m_using_small && !other.m_using_small) {
            std::ranges::uninitialized_move_n(
                small_data(), difference_type(m_size), other.small_data(),
                other.small_data() + m_size
            );
            std::ranges::destroy_n(small_data(), difference_type(m_size));
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

    void push_back(const T& element)
    {
        emplace_back(element);
    }

    void push_back(T&& element)
    {
        emplace_back(std::move(element));
    }

    template <typename... Args>
        requires std::constructible_from<T, Args&&...>
    T& emplace_back(Args&&... args)
    {
        if (m_using_small && m_size < small_cap) {
            T* const p = std::construct_at(small_data() + m_size, std::forward<Args>(args)...);
            ++m_size;
            return *p;
        }
        ensure_dynamic_storage(m_size + 1);
        return *std::construct_at(
            m_dynamic_data + m_size++, //
            std::forward<Args>(args)...
        );
    }

    void pop_back()
    {
        COWEL_ASSERT(!empty());
        if (m_using_small) {
            std::destroy_at(small_data() + m_size - 1);
        }
        else {
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
    void insert(iterator pos, Iterator begin, Sentinel sentinel)
    {
        const auto pos_index = std::size_t(pos - this->begin());

        if constexpr (requires { sentinel - begin; }) {
            const auto raw_count = sentinel - begin;
            COWEL_ASSERT(raw_count >= 0);
            const auto count = std::size_t(raw_count);
            const std::size_t new_size = m_size + count;

            if (m_using_small && new_size <= small_cap) {
                T* const sd = small_data();
                const std::size_t shift_count = m_size - pos_index;
                if (count <= shift_count) {
                    std::ranges::uninitialized_move_n(
                        sd + (m_size - count), difference_type(count), sd + m_size,
                        sd + m_size + count
                    );
                    std::ranges::move_backward(sd + pos_index, sd + (m_size - count), sd + m_size);
                    std::ranges::copy_n(
                        begin, std::iter_difference_t<Iterator>(count), sd + pos_index
                    );
                }
                else {
                    std::ranges::uninitialized_move_n(
                        sd + pos_index, difference_type(shift_count), sd + pos_index + count,
                        sd + pos_index + count + shift_count
                    );
                    const auto existing_result = std::ranges::copy_n(
                        begin, std::iter_difference_t<Iterator>(shift_count), sd + pos_index
                    );
                    const std::size_t insert_tail_count = count - shift_count;
                    std::ranges::uninitialized_copy_n(
                        existing_result.in, std::iter_difference_t<Iterator>(insert_tail_count),
                        sd + pos_index + shift_count,
                        sd + pos_index + shift_count + insert_tail_count
                    );
                }
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
    void append_range(R&& r)
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
        return m_using_small ? small_data() : m_dynamic_data;
    }
    [[nodiscard]]
    constexpr const_iterator begin() const noexcept
    {
        return m_using_small ? small_data() : m_dynamic_data;
    }
    [[nodiscard]]
    constexpr const_iterator cbegin() const noexcept
    {
        return begin();
    }

    [[nodiscard]]
    constexpr iterator end() noexcept
    {
        return m_using_small ? small_data() + m_size : m_dynamic_data + m_size;
    }
    [[nodiscard]]
    constexpr const_iterator end() const noexcept
    {
        return m_using_small ? small_data() + m_size : m_dynamic_data + m_size;
    }
    [[nodiscard]]
    constexpr const_iterator cend() const noexcept
    {
        return end();
    }

private:
    [[nodiscard]]
    constexpr T* small_data() noexcept
    {
        return m_small_storage.data;
    }

    [[nodiscard]]
    constexpr const T* small_data() const noexcept
    {
        return m_small_storage.data;
    }

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
        std::ranges::uninitialized_move_n(
            other.small_data(), difference_type(m_size), small_data(), small_data() + m_size
        );
        std::ranges::destroy_n(other.small_data(), difference_type(m_size));
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

    void move_dynamic_to_small()
    {
        std::ranges::uninitialized_move_n(
            m_dynamic_data, difference_type(m_size), small_data(), small_data() + m_size
        );
        std::ranges::destroy_n(m_dynamic_data, difference_type(m_size));
        m_using_small = true;
    }

    void move_small_to_dynamic(T* const target)
    {
        std::ranges::uninitialized_move_n(
            small_data(), difference_type(m_size), target, target + m_size
        );
        std::ranges::destroy_n(small_data(), difference_type(m_size));
        m_using_small = false;
    }

    void ensure_dynamic_storage(const std::size_t min_needed)
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
                small_data(), difference_type(m_size), new_data, new_data + m_size
            );
            std::ranges::destroy_n(small_data(), difference_type(m_size));
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
