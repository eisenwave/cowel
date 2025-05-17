#ifndef COWEL_RESULT_HPP
#define COWEL_RESULT_HPP

#include <utility>

#include "cowel/util/assert.hpp"

namespace cowel {

struct Error_Tag { };
struct Success_Tag { };

template <typename T, typename Error>
struct Result {
private:
    union {
        T m_value;
        Error m_error;
    };
    bool m_has_value;

public:
    [[nodiscard]]
    constexpr Result(Success_Tag, const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>)
        requires std::is_copy_constructible_v<T>
        : m_value(value)
        , m_has_value(true)
    {
    }

    [[nodiscard]]
    constexpr Result(Success_Tag, T&& value) noexcept(std::is_nothrow_move_constructible_v<T>)
        requires std::is_move_constructible_v<T>
        : m_value(std::move(value))
        , m_has_value(true)
    {
    }

    [[nodiscard]]
    constexpr Result(
        Error_Tag,
        const Error& error
    ) noexcept(std::is_nothrow_copy_constructible_v<Error>)
        requires std::is_copy_constructible_v<Error>
        : m_error(error)
        , m_has_value(false)
    {
    }

    [[nodiscard]]
    constexpr Result(Error_Tag, Error&& error) noexcept(std::is_nothrow_move_constructible_v<Error>)
        requires std::is_move_constructible_v<Error>
        : m_error(std::move(error))
        , m_has_value(false)
    {
    }

    [[nodiscard]]
    constexpr Result(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>)
        requires std::is_copy_constructible_v<T>
        : m_value(value)
        , m_has_value(true)
    {
    }

    [[nodiscard]]
    constexpr Result(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>)
        requires std::is_move_constructible_v<T>
        : m_value(std::move(value))
        , m_has_value(true)
    {
    }

    [[nodiscard]]
    constexpr Result(const Error& error) noexcept(std::is_nothrow_copy_constructible_v<Error>)
        requires std::is_copy_constructible_v<Error>
        : m_error(error)
        , m_has_value(false)
    {
    }

    [[nodiscard]]
    constexpr Result(Error&& error) noexcept(std::is_nothrow_move_constructible_v<Error>)
        requires std::is_move_constructible_v<Error>
        : m_error(std::move(error))
        , m_has_value(false)
    {
    }

    [[nodiscard]]
    constexpr Result(const Result& other
    ) noexcept(std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_copy_constructible_v<Error>)
        requires(std::is_copy_constructible_v<T> && std::is_copy_constructible_v<Error>)
        : m_has_value(other.m_has_value)
    {
        if (other.m_has_value) {
            std::construct_at(std::addressof(m_value), other.m_value);
        }
        else {
            std::construct_at(std::addressof(m_error), other.m_error);
        }
    }

    [[nodiscard]]
    constexpr Result(Result&& other
    ) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<Error>)
        requires(std::is_move_constructible_v<T> && std::is_move_constructible_v<Error>)
        : m_has_value(other.m_has_value)
    {
        if (other.m_has_value) {
            std::construct_at(std::addressof(m_value), std::move(other.m_value));
        }
        else {
            std::construct_at(std::addressof(m_error), std::move(other.m_error));
        }
    }

    constexpr Result& operator=(const Result& other) noexcept
        requires(std::is_nothrow_copy_assignable_v<T> && std::is_nothrow_copy_assignable_v<Error>)
    {
        if (this == &other) {
            // do nothing
        }
        else if (m_has_value && other.m_has_value) {
            m_value = other.m_value;
        }
        else if (!m_has_value && !other.m_has_value) {
            m_error = other.m_error;
        }
        else if (m_has_value && !other.m_has_value) {
            std::destroy_at(std::addressof(m_value));
            std::construct_at(std::addressof(m_error), other.m_error);
            m_has_value = false;
        }
        else {
            std::destroy_at(std::addressof(m_error));
            std::construct_at(std::addressof(m_value), other.m_value);
            m_has_value = true;
        }
        return *this;
    }

    constexpr Result& operator=(Result&& other) noexcept
        requires(std::is_nothrow_move_assignable_v<T> && std::is_nothrow_move_assignable_v<Error>)
    {
        if (m_has_value && other.m_has_value) {
            m_value = std::move(other.m_value);
        }
        else if (!m_has_value && !other.m_has_value) {
            m_error = std::move(other.m_error);
        }
        else if (m_has_value && !other.m_has_value) {
            std::destroy_at(std::addressof(m_value));
            std::construct_at(std::addressof(m_error), std::move(other.m_error));
            m_has_value = false;
        }
        else {
            std::destroy_at(std::addressof(m_error));
            std::construct_at(std::addressof(m_value), std::move(other.m_value));
            m_has_value = true;
        }
    }

    constexpr ~Result(
    ) noexcept(std::is_nothrow_destructible_v<T> && std::is_nothrow_destructible_v<Error>)
    {
        if (m_has_value) {
            std::destroy_at(std::addressof(m_value));
        }
        else {
            std::destroy_at(std::addressof(m_error));
        }
    }

    [[nodiscard]]
    constexpr bool has_value() const noexcept
    {
        return m_has_value;
    }

    [[nodiscard]]
    constexpr explicit operator bool() const noexcept
    {
        return m_has_value;
    }

    [[nodiscard]]
    constexpr const T* operator->() const
    {
        COWEL_ASSERT(m_has_value);
        return std::addressof(m_value);
    }

    [[nodiscard]]
    constexpr T* operator->()
    {
        COWEL_ASSERT(m_has_value);
        return std::addressof(m_value);
    }

    [[nodiscard]]
    constexpr const T& operator*() const&
    {
        COWEL_ASSERT(m_has_value);
        return m_value;
    }

    [[nodiscard]]
    constexpr T& operator*() &
    {
        COWEL_ASSERT(m_has_value);
        return m_value;
    }

    [[nodiscard]]
    constexpr const T&& operator*() const&&
    {
        COWEL_ASSERT(m_has_value);
        return std::move(m_value);
    }

    [[nodiscard]]
    constexpr T&& operator*() &&
    {
        COWEL_ASSERT(m_has_value);
        return std::move(m_value);
    }

    [[nodiscard]]
    constexpr const T& value() const&
    {
        COWEL_ASSERT(m_has_value);
        return m_value;
    }

    [[nodiscard]]
    constexpr T& value() &
    {
        COWEL_ASSERT(m_has_value);
        return m_value;
    }

    [[nodiscard]]
    constexpr const T&& value() const&&
    {
        COWEL_ASSERT(m_has_value);
        return std::move(m_value);
    }

    [[nodiscard]]
    constexpr T&& value() &&
    {
        COWEL_ASSERT(m_has_value);
        return std::move(m_value);
    }

    [[nodiscard]]
    constexpr const Error& error() const&
    {
        COWEL_ASSERT(!m_has_value);
        return m_error;
    }

    [[nodiscard]]
    constexpr Error& error() &
    {
        COWEL_ASSERT(!m_has_value);
        return m_error;
    }

    [[nodiscard]]
    constexpr const Error&& error() const&&
    {
        COWEL_ASSERT(!m_has_value);
        return std::move(m_error);
    }

    [[nodiscard]]
    constexpr Error&& error() &&
    {
        COWEL_ASSERT(!m_has_value);
        return std::move(m_error);
    }
};

// =================================================================================================

template <typename Error>
struct Result<void, Error> {
private:
    union {
        Error m_error;
    };
    bool m_has_value;

public:
    [[nodiscard]]
    constexpr Result() noexcept
        : m_has_value(true)
    {
    }

    [[nodiscard]]
    constexpr Result(const Error& error) noexcept(std::is_nothrow_copy_constructible_v<Error>)
        requires std::is_copy_constructible_v<Error>
        : m_error(error)
        , m_has_value(false)
    {
    }

    [[nodiscard]]
    constexpr Result(Error&& error) noexcept(std::is_nothrow_move_constructible_v<Error>)
        requires std::is_move_constructible_v<Error>
        : m_error(std::move(error))
        , m_has_value(false)
    {
    }

    [[nodiscard]]
    constexpr Result(const Result& other) noexcept(std::is_nothrow_copy_constructible_v<Error>)
        requires(std::is_copy_constructible_v<Error>)
        : m_has_value(other.m_has_value)
    {
        if (!other.m_has_value) {
            std::construct_at(std::addressof(m_error), other.m_error);
        }
    }

    [[nodiscard]]
    constexpr Result(Result&& other) noexcept(std::is_nothrow_move_constructible_v<Error>)
        requires(std::is_move_constructible_v<Error>)
        : m_has_value(other.m_has_value)
    {
        if (!other.m_has_value) {
            std::construct_at(std::addressof(m_error), std::move(other.m_error));
        }
    }

    // NOLINTBEGIN(bugprone-branch-clone)
    constexpr Result& operator=(const Result& other) noexcept
        requires(std::is_nothrow_copy_assignable_v<Error>)
    {
        if (this == &other) {
            // do nothing
        }
        else if (m_has_value && other.m_has_value) {
            // do nothing
        }
        else if (!m_has_value && !other.m_has_value) {
            m_error = other.m_error;
        }
        else if (m_has_value && !other.m_has_value) {
            std::construct_at(std::addressof(m_error), other.m_error);
            m_has_value = false;
        }
        else {
            std::destroy_at(std::addressof(m_error));
            m_has_value = true;
        }
        return *this;
    }

    constexpr Result& operator=(Result&& other) noexcept
        requires(std::is_nothrow_move_assignable_v<Error>)
    {
        if (this == &other) {
            // do nothing
        }
        else if (m_has_value && other.m_has_value) {
            // do nothing
        }
        else if (!m_has_value && !other.m_has_value) {
            m_error = std::move(other.m_error);
        }
        else if (m_has_value && !other.m_has_value) {
            std::construct_at(std::addressof(m_error), std::move(other.m_error));
            m_has_value = false;
        }
        else {
            std::destroy_at(std::addressof(m_error));
            m_has_value = true;
        }
        return *this;
    }
    // NOLINTEND(bugprone-branch-clone)

    constexpr ~Result() noexcept(std::is_nothrow_destructible_v<Error>)
    {
        if (!m_has_value) {
            std::destroy_at(std::addressof(m_error));
        }
    }

    [[nodiscard]]
    constexpr bool has_value() const noexcept
    {
        return m_has_value;
    }

    [[nodiscard]]
    constexpr explicit operator bool() const noexcept
    {
        return m_has_value;
    }

    constexpr void operator*() const
    {
        COWEL_ASSERT(m_has_value);
    }

    constexpr void value() const
    {
        COWEL_ASSERT(m_has_value);
    }

    [[nodiscard]]
    constexpr const Error& error() const&
    {
        COWEL_ASSERT(!m_has_value);
        return m_error;
    }

    [[nodiscard]]
    constexpr Error& error() &
    {
        COWEL_ASSERT(!m_has_value);
        return m_error;
    }

    [[nodiscard]]
    constexpr const Error&& error() const&&
    {
        COWEL_ASSERT(!m_has_value);
        return std::move(m_error);
    }

    [[nodiscard]]
    constexpr Error&& error() &&
    {
        COWEL_ASSERT(!m_has_value);
        return std::move(m_error);
    }
};

} // namespace cowel

#endif
