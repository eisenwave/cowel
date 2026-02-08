#ifndef COWEL_REGEX_HPP
#define COWEL_REGEX_HPP

#include <cstdint>
#include <string_view>
#include <vector>

#include "cowel/util/fixed_string.hpp"
#include "cowel/util/result.hpp"

#include "cowel/fwd.hpp"
#include "cowel/util/unicode.hpp"

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

enum struct Reg_Exp_Flags : Default_Underlying {
    /// @brief `d`.
    indices = 1 << 0,
    /// @brief `g`.
    global = 1 << 1,
    /// @brief `i`.
    ignore_case = 1 << 2,
    /// @brief `m`.
    multiline = 1 << 3,
    /// @brief `s`.
    dot_all = 1 << 4,
    /// @brief `u`.
    unicode = 1 << 5,
    /// @brief `v`.
    unicode_sets = 1 << 6,
    /// @brief `y`.
    sticky = 1 << 7,
};

static constexpr std::u8string_view reg_exp_flags_string = u8"dgimsuvy";

[[nodiscard]]
constexpr Reg_Exp_Flags operator|(const Reg_Exp_Flags x, const Reg_Exp_Flags y)
{
    return Reg_Exp_Flags(std::to_underlying(x) | std::to_underlying(y));
}

[[nodiscard]]
constexpr Reg_Exp_Flags operator&(const Reg_Exp_Flags x, const Reg_Exp_Flags y)
{
    return Reg_Exp_Flags(std::to_underlying(x) & std::to_underlying(y));
}

constexpr Reg_Exp_Flags& operator|=(Reg_Exp_Flags& x, const Reg_Exp_Flags y)
{
    return x = x | y;
}

constexpr Reg_Exp_Flags& operator&=(Reg_Exp_Flags& x, const Reg_Exp_Flags y)
{
    return x = x & y;
}

enum struct Reg_Exp_Flags_Error_Kind : Default_Underlying {
    invalid,
    duplicate,
};

struct Reg_Exp_Flags_Error {
    Reg_Exp_Flags_Error_Kind kind;
    std::size_t index;
    std::size_t length;
};

constexpr Result<Reg_Exp_Flags, Reg_Exp_Flags_Error>
reg_exp_flags_parse(const std::u8string_view flags)
{
    Reg_Exp_Flags result {};

    for (std::size_t i = 0; i < flags.size(); ++i) {
        // Even though we take a single UTF-8 code unit out of flags
        // (rather than a code point),
        // this operation is safe because flag_string is pure ASCII,
        // so searching for only the leading code unit of a code point
        // gives us no false negatives and no false positives.
        const std::size_t flag_index = reg_exp_flags_string.find(flags[i]);
        // However, if there is an error when parsing the flags,
        // we need to decode the whole code point for diagnostic purposes.
        const auto compute_bad_length = [&] {
            return std::size_t(utf8::decode_and_length_or_replacement(flags.substr(i)).length);
        };
        if (flag_index == std::u8string_view::npos) {
            return Reg_Exp_Flags_Error {
                .kind = Reg_Exp_Flags_Error_Kind::invalid,
                .index = i,
                .length = compute_bad_length(),
            };
        }
        const auto new_flag = Reg_Exp_Flags(1 << flag_index);
        if ((result & new_flag) != Reg_Exp_Flags {}) {
            return Reg_Exp_Flags_Error {
                .kind = Reg_Exp_Flags_Error_Kind::duplicate,
                .index = i,
                .length = compute_bad_length(),
            };
        }
        result |= new_flag;
    }

    return result;
}

[[nodiscard]]
constexpr Fixed_String8<reg_exp_flags_string.size()>
reg_exp_flags_to_string(const Reg_Exp_Flags flags)
{
    Fixed_String8<reg_exp_flags_string.size()> result;
    for (std::size_t i = 0; i < reg_exp_flags_string.size(); ++i) {
        if ((std::size_t(flags) >> i) & 1) {
            result.push_back(reg_exp_flags_string[i]);
        }
    }
    return result;
}

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
    static Result<Reg_Exp, Reg_Exp_Error_Code>
    make(std::u8string_view pattern, Reg_Exp_Flags flags = {});

private:
#ifdef COWEL_BUILD_NATIVE
    Reg_Exp_Impl m_impl;
#else
    GC_Ref<Unique_Host_Reg_Exp> m_ref;
#endif
    Reg_Exp_Flags m_flags;

#ifdef COWEL_BUILD_NATIVE
    [[nodiscard]]
    Reg_Exp(const Reg_Exp_Impl& impl, const Reg_Exp_Flags flags) noexcept
        : m_impl { impl }
        , m_flags { flags }
    {
    }
    [[nodiscard]]
    Reg_Exp(Reg_Exp_Impl&& impl, const Reg_Exp_Flags flags) noexcept
        : m_impl { std::move(impl) }
        , m_flags { flags }
    {
    }
#else
    [[nodiscard]]
    Reg_Exp(GC_Ref<Unique_Host_Reg_Exp> ref, const Reg_Exp_Flags flags) noexcept
        : m_ref { std::move(ref) }
        , m_flags { flags }
    {
    }
#endif

public:
    /// @brief Returns `true` if `string` matches this regex in its entirety.
    [[nodiscard]]
    Reg_Exp_Status match(std::u8string_view string) const;

    /// @brief Returns `true` if `string` contains an occurrence of this regex.
    [[nodiscard]]
    Reg_Exp_Search_Result search(std::u8string_view string) const;

    /// @brief Replaces every occurrence of this regular expression within `string`
    /// with `replacement`.
    [[nodiscard]]
    Reg_Exp_Status replace_all(
        std::pmr::vector<char8_t>& out,
        std::u8string_view string,
        std::u8string_view replacement
    ) const;

    [[nodiscard]]
    Reg_Exp_Flags get_flags() const
    {
        return m_flags;
    }

    [[nodiscard]]
    constexpr bool is_indices() const
    {
        return (m_flags & Reg_Exp_Flags::indices) != Reg_Exp_Flags {};
    }

    [[nodiscard]]
    constexpr bool is_global() const
    {
        return (m_flags & Reg_Exp_Flags::global) != Reg_Exp_Flags {};
    }

    [[nodiscard]]
    constexpr bool is_ignore_case() const
    {
        return (m_flags & Reg_Exp_Flags::ignore_case) != Reg_Exp_Flags {};
    }

    [[nodiscard]]
    constexpr bool is_multiline() const
    {
        return (m_flags & Reg_Exp_Flags::multiline) != Reg_Exp_Flags {};
    }

    [[nodiscard]]
    constexpr bool is_dot_all() const
    {
        return (m_flags & Reg_Exp_Flags::dot_all) != Reg_Exp_Flags {};
    }

    [[nodiscard]]
    constexpr bool is_unicode() const
    {
        return (m_flags & Reg_Exp_Flags::unicode) != Reg_Exp_Flags {};
    }

    [[nodiscard]]
    constexpr bool is_unicode_sets() const
    {
        return (m_flags & Reg_Exp_Flags::unicode_sets) != Reg_Exp_Flags {};
    }

    [[nodiscard]]
    constexpr bool is_sticky() const
    {
        return (m_flags & Reg_Exp_Flags::sticky) != Reg_Exp_Flags {};
    }
};

#ifdef COWEL_BUILD_NATIVE
[[nodiscard]]
std::u32string ecma_pattern_to_boost_pattern(std::u32string_view ecma_pattern, Reg_Exp_Flags flags);
#endif

} // namespace cowel

#endif
