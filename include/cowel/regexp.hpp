#ifndef COWEL_REGEX_HPP
#define COWEL_REGEX_HPP

#include <cstdint>
#include <string_view>

#include "cowel/util/result.hpp"

#include "cowel/fwd.hpp"

#ifdef COWEL_BUILD_WASM
#include "cowel/gc.hpp"
#endif

namespace cowel {

struct Reg_Exp_Match {
    std::size_t index;
    std::size_t length;
};

} // namespace cowel

namespace cowel {

enum struct Reg_Exp_Handle : std::uintptr_t { };

enum struct Reg_Exp_Error_Code : Default_Underlying {
    /// @brief The given pattern is not valid.
    bad_pattern,
};

enum struct Reg_Exp_Status : Default_Underlying {
    /// @brief Execution completed; no match was found.
    unmatched,
    /// @brief Execution completed; a match was found.
    matched,
    /// @brief The given handle is not valid.
    invalid,
    /// @brief An error occurred while trying to execute the regular expression,
    /// such as exceeding time limits.
    execution_error,
};

struct Reg_Exp_Search_Result {
    Reg_Exp_Status status;
    Reg_Exp_Match match;
};

struct In_Place_Tag { };

struct Reg_Exp;

#ifdef COWEL_BUILD_NATIVE
struct Reg_Exp_Impl {
private:
    alignas(8) unsigned char m_storage[16];

public:
    Reg_Exp_Impl() noexcept;
    Reg_Exp_Impl(const Reg_Exp_Impl&) noexcept;
    Reg_Exp_Impl(Reg_Exp_Impl&&) noexcept;

    Reg_Exp_Impl& operator=(const Reg_Exp_Impl&) noexcept;
    Reg_Exp_Impl& operator=(Reg_Exp_Impl&&) noexcept;

    ~Reg_Exp_Impl();

private:
    template <typename T>
    Reg_Exp_Impl(In_Place_Tag, T&&) noexcept;

    [[nodiscard]]
    auto& get();
    [[nodiscard]]
    const auto& get() const;

    friend Reg_Exp;
};
#else
/// @brief Represents unique ownership over a host-side big integer,
/// such as JavaScript's `BigInt`.
struct Unique_Host_Reg_Exp {
private:
    Reg_Exp_Handle m_handle {};

public:
    [[nodiscard]]
    explicit Unique_Host_Reg_Exp(const Reg_Exp_Handle handle) noexcept
        : m_handle { handle }
    {
        COWEL_ASSERT(handle != Reg_Exp_Handle {});
    }
    Unique_Host_Reg_Exp(const Unique_Host_Reg_Exp&) = delete;
    [[nodiscard]]
    Unique_Host_Reg_Exp(Unique_Host_Reg_Exp&&)
        = delete;

    Unique_Host_Reg_Exp& operator=(const Unique_Host_Reg_Exp&) = delete;
    Unique_Host_Reg_Exp& operator=(Unique_Host_Reg_Exp&& other) = delete;

    ~Unique_Host_Reg_Exp();

    [[nodiscard]]
    Reg_Exp_Handle handle() const noexcept
    {
        return m_handle;
    }
};
#endif

/// @brief Represents an ECMA-Script-flavored regular expression.
///
/// A `Reg_Exp` has shared ownership over the underlying compiled regular expression,
/// meaning that both copying and moving are relatively inexpensive.
struct Reg_Exp {
public:
    [[nodiscard]]
    static Result<Reg_Exp, Reg_Exp_Error_Code> make(std::u8string_view pattern);

private:
#ifdef COWEL_BUILD_NATIVE
    Reg_Exp_Impl m_impl;
#else
    GC_Ref<Unique_Host_Reg_Exp> m_ref;
#endif

#ifdef COWEL_BUILD_NATIVE
    [[nodiscard]]
    Reg_Exp(const Reg_Exp_Impl& impl) noexcept
        : m_impl { impl }
    {
    }
    [[nodiscard]]
    Reg_Exp(Reg_Exp_Impl&& impl) noexcept
        : m_impl { std::move(impl) }
    {
    }
#else
    [[nodiscard]]
    Reg_Exp(GC_Ref<Unique_Host_Reg_Exp> ref) noexcept
        : m_ref { std::move(ref) }
    {
    }
#endif

public:
    /// @brief Returns `true` if `string` matches this regex in its entirety.
    [[nodiscard]]
    Reg_Exp_Status test(std::u8string_view string) const;

    /// @brief Returns `true` if `string` matches this regex in its entirety.
    [[nodiscard]]
    Reg_Exp_Search_Result search(std::u8string_view string) const;
};

#ifdef COWEL_BUILD_NATIVE
[[nodiscard]]
std::u32string ecma_pattern_to_boost_pattern(std::u32string_view ecma_pattern);
#endif

} // namespace cowel

#endif
