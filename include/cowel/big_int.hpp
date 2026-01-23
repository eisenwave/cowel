#ifndef COWEL_BIG_INT_HPP
#define COWEL_BIG_INT_HPP

#include <charconv>
#include <compare>
#include <cstddef>
#include <limits>
#include <string_view>

#include "cowel/util/ascii_algorithm.hpp"
#include "cowel/util/from_chars.hpp"
#include "cowel/util/function_ref.hpp"
#include "cowel/util/math.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/to_chars.hpp"

#include "cowel/gc.hpp"
#include "cowel/settings.hpp"

extern "C" {

#ifdef COWEL_EMSCRIPTEN
enum struct cowel_big_int_handle : cowel::Uint32 { };
enum struct cowel_big_int_handle_pair : cowel::Uint64 { };
#else
enum struct cowel_big_int_handle : std::uintptr_t { };
struct cowel_big_int_handle_pair {
    cowel_big_int_handle first;
    cowel_big_int_handle second;
};
#endif

struct cowel_big_int_div_result_t {
    cowel::Int128 small_quotient;
    cowel::Int128 small_remainder;
    bool div_by_zero;
};

enum struct cowel_big_int_from_string_status : unsigned char {
    /// @brief Conversion succeeded, and the result fits into a 128-bit integer.
    /// The resulting integer is stored in `cowel_big_int_small_result`.
    small_result,
    /// @brief Conversion succeeded.
    /// The resulting integer is stored in `cowel_big_int_big_result`.
    big_result,
    /// @brief The provided digit sequence or another function argument was invalid.
    invalid_argument,
    /// @brief The integer size exceeds implementation limits.
    result_out_of_range,
};

/// @brief Creates a host integer with the given signed 32-bit value.
COWEL_WASM_IMPORT("env", "big_int_i32")
cowel_big_int_handle cowel_big_int_i32(cowel::Int32 x);

/// @brief Creates a host integer with the given signed 64-bit value.
COWEL_WASM_IMPORT("env", "big_int_i64")
cowel_big_int_handle cowel_big_int_i64(cowel::Int64 x);

/// @brief Creates a host integer with the given signed 128-bit value.
COWEL_WASM_IMPORT("env", "big_int_i128")
cowel_big_int_handle cowel_big_int_i128(cowel::Int128 x);

/// @brief Creates a host integer with the given signed 192-bit value,
/// separated into three 64-bit integers,
/// where the first parameter is the least significant set of bits.
COWEL_WASM_IMPORT("env", "big_int_i192")
cowel_big_int_handle cowel_big_int_i192(cowel::Int64, cowel::Int64, cowel::Int64);

/// @brief Creates a host integer with value `1 << x`,
/// or zero if `x` is negative.
COWEL_WASM_IMPORT("env", "big_int_pow2_i32")
cowel_big_int_handle cowel_big_int_pow2_i32(cowel::Int32 x);

/// @brief Deletes a host integer with the given handle,
/// if that handle refers to a valid host integer.
/// @return `true` iff the given handle was valid.
COWEL_WASM_IMPORT("env", "big_int_delete")
bool cowel_big_int_delete(cowel_big_int_handle);

/// @brief Returns `0` if `x == y`,
/// `-1` if `x < y`, and
/// `1` if `x > y`.
COWEL_WASM_IMPORT("env", "big_int_compare_i32")
int cowel_big_int_compare_i32(cowel_big_int_handle x, cowel::Int32 y);

/// @brief Returns `0` if `x == y`,
/// `-1` if `x < y`, and
/// `1` if `x > y`.
COWEL_WASM_IMPORT("env", "big_int_compare_i128")
int cowel_big_int_compare_i128(cowel_big_int_handle x, cowel::Int128 y);

/// @brief Returns `0` if `x == y`,
/// `-1` if `x < y`, and
/// `1` if `x > y`.
COWEL_WASM_IMPORT("env", "big_int_compare")
int cowel_big_int_compare(cowel_big_int_handle x, cowel_big_int_handle y);

/// @brief Returns the amount of bits N required for a two's-complement N-bit integer
/// to represent the value of `x`.
/// Mathematically, this is `floor(log2(x)) + 1` for positive numbers
/// and `floor(log2(-x - 1)) + 1` for negative numbers,
/// where `log2` is the binary logarithm with `log2(0) == 0`.
COWEL_WASM_IMPORT("env", "cowel_big_int_twos_width")
int cowel_big_int_twos_width(cowel_big_int_handle x);

/// @brief Returns the amount of bits N required for a one's-complement N-bit integer
/// to represent the value of `x`.
/// Mathematically, this is `floor(log2(abs(x))) + 1`,
/// where `log2` is the binary logarithm with `log2(0) == 0`.
COWEL_WASM_IMPORT("env", "cowel_big_int_ones_width")
int cowel_big_int_ones_width(cowel_big_int_handle x);

/// @brief Returns `-x`.
COWEL_WASM_IMPORT("env", "big_int_neg")
cowel_big_int_handle cowel_big_int_neg(cowel_big_int_handle x);

/// @brief Returns `~x`.
/// That is, `-x - 1`.
COWEL_WASM_IMPORT("env", "big_int_bit_not")
cowel_big_int_handle cowel_big_int_bit_not(cowel_big_int_handle x);

/// @brief Returns the absolute value of `x`.
COWEL_WASM_IMPORT("env", "big_int_abs")
cowel_big_int_handle cowel_big_int_abs(cowel_big_int_handle x);

/// @brief Stores the value of `x` truncated to 128 bits in `cowel_big_int_small_result`.
/// Returns `true` if this resulted in loss of information,
/// i.e. if truncation actually happened, and `false` otherwise.
COWEL_WASM_IMPORT("env", "big_int_trunc_i128")
bool cowel_big_int_trunc_i128(cowel_big_int_handle x);

/// @brief Returns `x + y`.
COWEL_WASM_IMPORT("env", "big_int_add_i32")
cowel_big_int_handle cowel_big_int_add_i32(cowel_big_int_handle x, cowel::Int32 y);

/// @brief Returns `x + y`.
COWEL_WASM_IMPORT("env", "big_int_add_i128")
cowel_big_int_handle cowel_big_int_add_i128(cowel_big_int_handle x, cowel::Int128 y);

/// @brief Returns `x + y`.
COWEL_WASM_IMPORT("env", "big_int_add")
cowel_big_int_handle cowel_big_int_add(cowel_big_int_handle x, cowel_big_int_handle y);

/// @brief Returns `x - y`.
COWEL_WASM_IMPORT("env", "big_int_sub_i128")
cowel_big_int_handle cowel_big_int_sub_i128(cowel_big_int_handle x, cowel::Int128 y);

/// @brief Returns `x - y`.
COWEL_WASM_IMPORT("env", "big_int_sub")
cowel_big_int_handle cowel_big_int_sub(cowel_big_int_handle x, cowel_big_int_handle y);

/// @brief Returns `x * y`.
COWEL_WASM_IMPORT("env", "big_int_mul_i128")
cowel_big_int_handle cowel_big_int_mul_i128(cowel_big_int_handle x, cowel::Int128 y);

/// @brief Returns `x * y`.
COWEL_WASM_IMPORT("env", "big_int_mul_i128_i128")
cowel_big_int_handle cowel_big_int_mul_i128_i128(cowel::Int128 x, cowel::Int128 y);

/// @brief Returns `x * y`.
COWEL_WASM_IMPORT("env", "big_int_mul")
cowel_big_int_handle cowel_big_int_mul(cowel_big_int_handle x, cowel_big_int_handle y);

/// @brief Returns the quotient and remainder of the division `x / y`,
/// with rounding as specified by `rounding`.
/// If the quotient or remainder fit into 128-bit integers,
/// the returned host handles are zero and the 128-bit integer values are written to
/// `cowel_big_int_div_result`.
/// `cowel_big_int_div_result.div_by_zero` is set to `true` if `y` is zero,
/// otherwise it remains unmodified.
/// @returns Two handles packed into a 64-bit integer,
/// where the less significant 32 bits are the quotient,
/// and the more significant 32 bits are the remainder.
/// If `y` is zero, returns a value-initialized pair of handles.
COWEL_WASM_IMPORT("env", "big_int_div_rem")
cowel_big_int_handle_pair
cowel_big_int_div_rem(cowel::Div_Rounding rounding, cowel_big_int_handle x, cowel_big_int_handle y);

/// @brief Returns the quotient of the division `x / y`,
/// with rounding as specified by `rounding`.
/// `cowel_big_int_div_result.div_by_zero` is set to `true` if `y` is zero,
/// otherwise it remains unmodified.
COWEL_WASM_IMPORT("env", "big_int_div")
cowel_big_int_handle
cowel_big_int_div(cowel::Div_Rounding rounding, cowel_big_int_handle x, cowel_big_int_handle y);

/// @brief Returns the remainder of the division `x / y`,
/// with rounding as specified by `rounding`.
/// `cowel_big_int_div_result.div_by_zero` is set to `true` if `y` is zero,
/// otherwise it remains unmodified.
COWEL_WASM_IMPORT("env", "big_int_rem")
cowel_big_int_handle
cowel_big_int_rem(cowel::Div_Rounding rounding, cowel_big_int_handle x, cowel_big_int_handle y);

/// @brief Returns `x * pow(2, s)` rounded towards negative infinity.
/// Negative `s` is allowed and has the effect of shifting in the opposite direction.
COWEL_WASM_IMPORT("env", "big_int_shl_i128_i32")
cowel_big_int_handle cowel_big_int_shl_i128_i32(cowel::Int128 x, cowel::Int32 s);

/// @brief Returns `x * pow(2, s)` rounded towards negative infinity.
/// Negative `s` is allowed and has the effect of shifting in the opposite direction.
COWEL_WASM_IMPORT("env", "big_int_shl_i32")
cowel_big_int_handle cowel_big_int_shl_i32(cowel_big_int_handle x, cowel::Int32 s);

/// @brief Returns `x * pow(2, -s)` rounded towards negative infinity.
/// Negative `s` is allowed and has the effect of shifting in the opposite direction.
COWEL_WASM_IMPORT("env", "big_int_shr_i32")
cowel_big_int_handle cowel_big_int_shr_i32(cowel_big_int_handle x, cowel::Int32 s);

/// @brief Returns `x` raised to the power of `y`,
/// or zero if `y` is negative.
///
/// For this function, `pow(0, 0)` is defined as `0`.
/// Note that this allows for error detection by the caller;
/// if `y` is zero and the result is not `1`, the result is not mathematically defined.
COWEL_WASM_IMPORT("env", "big_int_pow_i128_i32")
cowel_big_int_handle cowel_big_int_pow_i128_i32(cowel::Int128 x, cowel::Int32 y);

/// @brief Returns `x` raised to the power of `y`,
/// or zero if `y` is negative.
///
/// For this function, `pow(0, 0)` is defined as `0`.
/// Note that this allows for error detection by the caller;
/// if `y` is zero and the result is not `1`, the result is not mathematically defined.
COWEL_WASM_IMPORT("env", "big_int_pow_i32")
cowel_big_int_handle cowel_big_int_pow_i32(cowel_big_int_handle x, cowel::Int32 y);

/// @brief Returns `x & y`.
/// Negative numbers are treated as having an infinite sequence of leading one-bits.
COWEL_WASM_IMPORT("env", "big_int_bit_and_i128")
cowel_big_int_handle cowel_big_int_bit_and_i128(cowel_big_int_handle x, cowel::Int128 y);

/// @brief Returns `x & y`.
/// Negative numbers are treated as having an infinite sequence of leading one-bits.
COWEL_WASM_IMPORT("env", "big_int_bit_and")
cowel_big_int_handle cowel_big_int_bit_and(cowel_big_int_handle x, cowel_big_int_handle y);

/// @brief Returns `x | y`.
/// Negative numbers are treated as having an infinite sequence of leading one-bits.
COWEL_WASM_IMPORT("env", "big_int_bit_or_i128")
cowel_big_int_handle cowel_big_int_bit_or_i128(cowel_big_int_handle x, cowel::Int128 y);

/// @brief Returns `x | y`.
/// Negative numbers are treated as having an infinite sequence of leading one-bits.
COWEL_WASM_IMPORT("env", "big_int_bit_or")
cowel_big_int_handle cowel_big_int_bit_or(cowel_big_int_handle x, cowel_big_int_handle y);

/// @brief Returns `x ^ y`.
/// Negative numbers are treated as having an infinite sequence of leading one-bits.
COWEL_WASM_IMPORT("env", "big_int_bit_xor_i128")
cowel_big_int_handle cowel_big_int_bit_xor_i128(cowel_big_int_handle x, cowel::Int128 y);

/// @brief Returns `x ^ y`.
/// Negative numbers are treated as having an infinite sequence of leading one-bits.
COWEL_WASM_IMPORT("env", "big_int_bit_xor")
cowel_big_int_handle cowel_big_int_bit_xor(cowel_big_int_handle x, cowel_big_int_handle y);

/// @brief Converts `x` to a UTF-8 string and writes the resulting digits into `buffer`.
/// @param buffer The point to a buffer where the UTF-8 text is written to.
/// @param size The size of the buffer.
/// Note that the required size can be precalculated by calculating the amount of binary digits
/// and scaling it according to `base`.
/// @param x The number to convert.
/// @param base The base.
/// Shall be in range [2, 36].
/// @param to_upper If `true`, outputs uppercase digits instead of lowercase digits
/// for base `11` and greater.
/// @returns The amount of digits written to `buffer` if conversions succeeded,
/// or zero if it failed (due to the buffer being too small).
COWEL_WASM_IMPORT("env", "big_int_to_string")
std::size_t cowel_big_int_to_string(
    char* buffer,
    std::size_t size,
    cowel_big_int_handle x,
    int base,
    bool to_upper
);

/// @brief Parses an integer that is represented using a sequence of digits encoded in UTF-8.
/// Note that unlike `std::from_chars`,
/// all characters in the `buffer` must be part of the digit sequence,
/// not just a prefix.
/// That is, the user is responsible for lexing the string beforehand or conversion fails.
/// @param buffer The buffer in which the UTF-8-encoded digit sequence is stored.
/// @param size The size of the buffer.
/// @param base The base of the digit sequence.
/// @returns A status indicating whether the conversion succeeded
/// and where the result is stored.
COWEL_WASM_IMPORT("env", "big_int_from_string")
cowel_big_int_from_string_status
cowel_big_int_from_string(const char* buffer, std::size_t size, int base);

#ifdef COWEL_EMSCRIPTEN
#define COWEL_BIG_INT_GLOBAL_RESULT extern
#else
#define COWEL_BIG_INT_GLOBAL_RESULT inline thread_local
#endif

/// @brief For all functions that return `cowel_big_int_handle`,
/// if the result fits into a 128-bit signed integer,
/// the result is stored in this global variable,
/// and the returned handle is zero.
COWEL_BIG_INT_GLOBAL_RESULT cowel::Int128 cowel_big_int_small_result;

/// @see `cowel_big_int_from_string`.
COWEL_BIG_INT_GLOBAL_RESULT cowel_big_int_handle cowel_big_int_big_result;

/// @see `cowel_big_int_div_rem`.
COWEL_BIG_INT_GLOBAL_RESULT cowel_big_int_div_result_t cowel_big_int_div_result;

//
}

namespace cowel {

using Big_Int_Handle = cowel_big_int_handle;

template <typename T>
/// @brief The result value of the conversion.
struct Conversion_Result {
    T value;
    /// @brief True if the conversions has an inexact result,
    /// such as a truncated result.
    bool lossy;

    friend bool operator==(const Conversion_Result&, const Conversion_Result&) = default;
};

template <typename Q, typename R = Q>
struct Div_Result {
    Q quotient;
    R remainder;

    friend bool operator<=>(const Div_Result&, const Div_Result&) = default;
};

namespace detail {

#ifdef COWEL_EMSCRIPTEN
/// @brief Represents unique ownership over a host-side big integer,
/// such as JavaScript's `BigInt`.
struct Unique_Host_Big_Int {
private:
    Big_Int_Handle m_handle {};

public:
    [[nodiscard]]
    explicit Unique_Host_Big_Int(Big_Int_Handle handle) noexcept
        : m_handle { handle }
    {
        COWEL_ASSERT(handle != Big_Int_Handle {});
    }
    Unique_Host_Big_Int(const Unique_Host_Big_Int&) = delete;
    [[nodiscard]]
    Unique_Host_Big_Int(Unique_Host_Big_Int&&)
        = delete;

    Unique_Host_Big_Int& operator=(const Unique_Host_Big_Int&) = delete;
    Unique_Host_Big_Int& operator=(Unique_Host_Big_Int&& other) = delete;

    ~Unique_Host_Big_Int()
    {
        const bool success = cowel_big_int_delete(m_handle);
        COWEL_ASSERT(success);
        m_handle = {};
    }

    [[nodiscard]]
    Big_Int_Handle handle() const noexcept
    {
        return m_handle;
    }
};

using Big_Int_Backend = Unique_Host_Big_Int;
#else
/// @brief A sufficiently large and aligned type for a `boost::multiprecision::cpp_int`
/// to live inside.
///
/// This acts as an opaque wrapper which manages the lifetime of a `cpp_int`,
/// without requiring an include of `<boost/multiprecision/cpp_int.hpp>` in all headers
/// that use `Cpp_Int`; this would have massive compilation cost.
struct Big_Int_Backend {
private:
    alignas(16) unsigned char m_storage[32];

public:
    Big_Int_Backend();
    Big_Int_Backend(const Big_Int_Backend&) = delete;
    Big_Int_Backend(Big_Int_Backend&&) = delete;

    Big_Int_Backend& operator=(const Big_Int_Backend&) = delete;
    Big_Int_Backend& operator=(Big_Int_Backend&&) = delete;

    ~Big_Int_Backend();

    [[nodiscard]]
    auto& get();
};

[[nodiscard]]
inline GC_Node* get_handle_node(const Big_Int_Handle handle) noexcept
{
    return reinterpret_cast<GC_Node*>(std::uintptr_t(handle)); // NOLINT
}
#endif

} // namespace detail

/// @brief An arbitrary precision integer.
///
/// A `Big_Int` uses reference-counting to store an immutable allocated digit sequence.
/// This digit sequence could exist in the host (for WASM build) or could be stored
/// directly in memory for native builds.
/// In either case, this makes `Big_Int` cheaply copyable and movable,
/// and it makes the container itself small.
///
/// Furthermore, `Big_Int` is optimized for small integers;
/// for values representable as a signed 128-bit integer,
/// the value is directly stored in the container, without allocations.
struct Big_Int {
    static const Big_Int zero;
    static const Big_Int one;

    /// @brief Equivalent to `Big_Int(1) << exponent`.
    [[nodiscard]]
    static constexpr Big_Int pow2(const int exponent)
    {
        if (exponent < 127) {
            return exponent >= 0 ? Big_Int { Int128 { 1 } << exponent } : zero;
        }
        return from_host_result(cowel_big_int_pow2_i32(exponent));
    }

    [[nodiscard]]
    static constexpr Big_Int from_host_result(const Big_Int_Handle handle) noexcept
    {
        if (handle == Big_Int_Handle {}) {
            return Big_Int { cowel_big_int_small_result };
        }
#ifdef COWEL_EMSCRIPTEN
        return Big_Int { handle };
#else
        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        auto* const gc_node = detail::get_handle_node(handle);
        return Big_Int { GC_Ref<detail::Big_Int_Backend> { gc_node } };
#endif
    }

private:
    bool m_is_small;
    union {
        Underaligned_Int128_Storage m_int128;
        GC_Ref<detail::Big_Int_Backend> m_host_handle;
    };

    [[nodiscard]]
    explicit Big_Int(GC_Ref<detail::Big_Int_Backend> ref) noexcept
        : m_is_small { false }
        , m_host_handle { std::move(ref) }
    {
    }

public:
    /// @brief Initializes to zero.
    [[nodiscard]]
    constexpr Big_Int() noexcept
        : m_is_small { true }
        , m_int128 {}
    {
    }
    /// @brief Initializes to the given value.
    [[nodiscard]]
    constexpr explicit Big_Int(const Int32 x) noexcept
        : Big_Int { Int128 { x } }
    {
    }
    /// @brief Initializes to the given value.
    [[nodiscard]]
    constexpr explicit Big_Int(const Int64 x) noexcept
        : Big_Int { Int128 { x } }
    {
    }
    /// @brief Initializes to the given value.
    [[nodiscard]]
    constexpr explicit Big_Int(const Int128 x) noexcept
        : m_is_small { true }
        , m_int128 { std::bit_cast<Underaligned_Int128_Storage>(Int128 { x }) }
    {
    }

    [[nodiscard]]
    constexpr Big_Int(const Big_Int& other)
        : m_is_small { other.m_is_small }
    {
        if (other.m_is_small) {
            m_int128 = other.m_int128;
        }
        else {
            new (&m_host_handle) auto(other.m_host_handle);
        }
    }

    /// @brief Move assignment operator.
    /// `other.is_zero()` is `true` after this operation.
    [[nodiscard]]
    constexpr Big_Int(Big_Int&& other) noexcept
        : m_is_small { other.m_is_small }
    {
        if (other.m_is_small) {
            m_int128 = std::exchange(other.m_int128, {});
        }
        else {
            new (&m_host_handle) auto(std::move(other.m_host_handle));
            other.set_zero();
        }
    }

    /// @brief Initializes from a given digit sequence as if by `from_characters`,
    /// except that all of `digits` (not just a prefix) must be a nonempty digit sequence.
    [[nodiscard]]
    explicit Big_Int(const std::string_view digits, const int base = 10)
        : Big_Int {}
    {
        const auto [p, ec] = from_characters(digits, *this, base);
        COWEL_ASSERT(ec == std::errc {});
        COWEL_ASSERT(p == digits.data() + digits.size());
    }

    /// @brief Initializes from a given digit sequence as if by `from_characters`,
    /// except that all of `digits` (not just a prefix) must be a nonempty digit sequence.
    [[nodiscard]]
    explicit Big_Int(const std::u8string_view digits, const int base = 10)
        : Big_Int { as_string_view(digits), base }
    {
    }

    /// @brief Copy assignment operator.
    /// Safe for self-copy-assignment.
    constexpr Big_Int& operator=(const Big_Int& other)
    {
        if (this != &other) {
            auto copy = other;
            swap(copy);
        }
        return *this;
    }
    /// @brief Move assignment operator.
    /// `other.is_zero()` is `true` after this operation.
    /// Safe for self-move-assignment,
    /// although it results in the value being zeroed rather than being preserved.
    constexpr Big_Int& operator=(Big_Int&& other) noexcept
    {
        set_zero();
        m_is_small = other.m_is_small;
        if (other.m_is_small) {
            m_int128 = other.m_int128;
        }
        else {
            m_host_handle = std::move(other.m_host_handle);
        }
        other.set_zero();
        return *this;
    }
    constexpr Big_Int& operator=(const Int128 x) noexcept
    {
        set_zero();
        m_int128 = std::bit_cast<Underaligned_Int128_Storage>(x);
        return *this;
    }

    constexpr ~Big_Int()
    {
        set_zero();
    }

    // UNARY OPERATIONS ============================================================================

    /// @brief Exchanges the value of this object with the given one.
    constexpr void swap(Big_Int& other) noexcept
    {
        if (m_is_small && other.m_is_small) {
            std::swap(m_int128, other.m_int128);
        }
        else if (!m_is_small && !other.m_is_small) {
            std::swap(m_host_handle, other.m_host_handle);
        }
        else {
            std::swap(*this, other);
        }
    }
    /// @brief Equivalent to `x.swap(y)`.
    friend constexpr void swap(Big_Int& x, Big_Int& y) noexcept
    {
        x.swap(y);
    }

    /// @brief Returns `true` if this value is zero.
    [[nodiscard]]
    constexpr bool is_zero() const noexcept
    {
        if (m_is_small) {
            return get_i128() == 0;
        }
        return cowel_big_int_compare_i32(get_host_handle(), 0) == 0;
    }

    /// @see `cowel_big_int_twos_width`
    [[nodiscard]]
    constexpr int get_twos_width() const noexcept
    {
        if (m_is_small) {
            return twos_width(get_i128());
        }
        return cowel_big_int_twos_width(get_host_handle());
    }

    /// @see `cowel_big_int_ones_width`
    [[nodiscard]]
    constexpr int get_ones_width() const noexcept
    {
        if (m_is_small) {
            return ones_width(get_i128());
        }
        return cowel_big_int_ones_width(get_host_handle());
    }

    /// @brief Equivalent to `*this <=> 0`.
    [[nodiscard]]
    constexpr std::strong_ordering compare_zero() const noexcept
    {
        return get_signum() <=> 0;
    }

    /// @brief Equivalent to `(*this > 0) - (*this < 0)`.
    [[nodiscard]]
    constexpr int get_signum() const noexcept
    {
        if (m_is_small) {
            return (get_i128() > 0) - (get_i128() < 0);
        }
        return cowel_big_int_compare_i32(get_host_handle(), 0);
    }

    /// @brief Returns a copy of this value.
    [[nodiscard]]
    constexpr Big_Int operator+() const
    {
        return *this;
    }

    /// @brief Returns this value negated.
    [[nodiscard]]
    constexpr Big_Int operator-() const
    {
        if (is_small()) {
            if (get_i128() == std::numeric_limits<Int128>::min()) [[unlikely]] {
                return from_host_result(cowel_big_int_pow2_i32(127));
            }
            return Big_Int { -get_i128() };
        }
        return from_host_result(cowel_big_int_neg(get_host_handle()));
    }

    /// @brief Returns the bitwise complement of this value.
    /// That is, `-*this - 1`.
    [[nodiscard]]
    constexpr Big_Int operator~() const
    {
        if (is_small()) {
            return Big_Int { get_i128() };
        }
        return from_host_result(cowel_big_int_bit_not(get_host_handle()));
    }

    /// @brief Returns the absolute of `x`.
    [[nodiscard]]
    friend constexpr Big_Int abs(const Big_Int& x)
    {
        if (x.is_small()) {
            const auto i128 = x.get_i128();
            if (i128 == std::numeric_limits<Int128>::min()) [[unlikely]] {
                return from_host_result(cowel_big_int_pow2_i32(127));
            }
            return Big_Int { i128 < 0 ? -i128 : i128 };
        }
        return from_host_result(cowel_big_int_abs(x.get_host_handle()));
    }

    constexpr Big_Int& operator++()
    {
        if (is_small()) {
            const Int128 small = get_i128();
            if (small == std::numeric_limits<Int128>::max()) [[unlikely]] {
                *this = from_host_result(cowel_big_int_pow2_i32(127));
            }
            else {
                *this = small + 1;
            }
            return *this;
        }
        *this = from_host_result(cowel_big_int_add_i32(get_host_handle(), 1));
        return *this;
    }

    [[nodiscard("++x is better if the result is discarded anyway.")]]
    constexpr Big_Int operator++(int)
    {
        auto copy = *this;
        ++*this;
        return copy;
    }

    constexpr Big_Int& operator--()
    {
        if (is_small()) {
            const Int128 small = get_i128();
            if (small == std::numeric_limits<Int128>::max()) [[unlikely]] {
                *this = from_host_result(cowel_big_int_i192(-1, Int64(Uint64(-1) >> 1), -1));
            }
            else {
                *this = small - 1;
            }
            return *this;
        }
        *this = from_host_result(cowel_big_int_add_i32(get_host_handle(), -1));
        return *this;
    }

    [[nodiscard("--x is better if the result is discarded anyway.")]]
    constexpr Big_Int operator--(int)
    {
        auto copy = *this;
        --*this;
        return copy;
    }

    // TYPE CONVERSION =============================================================================

    [[nodiscard]]
    constexpr Conversion_Result<Int32> as_i32() const noexcept
    {
        const auto [i128, i128_lossy] = as_i128();
        return { .value = Int32(i128), .lossy = i128_lossy || Int32(i128) != i128 };
    }

    [[nodiscard]]
    constexpr Conversion_Result<Int64> as_i64() const noexcept
    {
        const auto [i128, i128_lossy] = as_i128();
        return { .value = Int64(i128), .lossy = i128_lossy || Int64(i128) != i128 };
    }

    [[nodiscard]]
    constexpr Conversion_Result<Int128> as_i128() const noexcept
    {
        if (m_is_small) {
            return { .value = get_i128(), .lossy = false };
        }
        const bool lossy = cowel_big_int_trunc_i128(get_host_handle());
        return { .value = cowel_big_int_small_result, .lossy = lossy };
    }

    /// @brief Returns the value of this integer truncated to 32 bits.
    [[nodiscard]]
    constexpr explicit operator Int32() const noexcept
    {
        return as_i32().value;
    }

    /// @brief Returns the value of this integer truncated to 64 bits.
    [[nodiscard]]
    constexpr explicit operator Int64() const noexcept
    {
        return as_i64().value;
    }

    /// @brief Returns the value of this integer truncated to 128 bits.
    [[nodiscard]]
    constexpr explicit operator Int128() const noexcept
    {
        return as_i128().value;
    }

    /// @brief Equivalent to `!is_zero()`.
    [[nodiscard]]
    constexpr explicit operator bool() const noexcept
    {
        return !is_zero();
    }

    // BINARY OPERATIONS ===========================================================================

    [[nodiscard]]
    friend constexpr bool operator==(const Big_Int& x, const Int32 y) noexcept
    {
        return x == Big_Int { y };
    }
    [[nodiscard]]
    friend constexpr bool operator==(const Big_Int& x, const Int128 y) noexcept
    {
        return x == Big_Int { y };
    }
    [[nodiscard]]
    friend constexpr bool operator==(const Big_Int& x, const Big_Int& y) noexcept
    {
        if (x.is_small()) {
            if (y.is_small()) {
                return x.get_i128() == y.get_i128();
            }
            const int result = cowel_big_int_compare_i128(y.get_host_handle(), x.get_i128());
            return result == 0;
        }
        if (y.is_small()) {
            const int result = cowel_big_int_compare_i128(x.get_host_handle(), y.get_i128());
            return result == 0;
        }
        const int result = cowel_big_int_compare(x.get_host_handle(), y.get_host_handle());
        return result == 0;
    }

    [[nodiscard]]
    friend constexpr std::strong_ordering operator<=>(const Big_Int& x, const Int32 y) noexcept
    {
        return x <=> Big_Int { y };
    }
    [[nodiscard]]
    friend constexpr std::strong_ordering operator<=>(const Big_Int& x, const Int128 y) noexcept
    {
        return x <=> Big_Int { y };
    }
    [[nodiscard]]
    friend constexpr std::strong_ordering operator<=>(const Big_Int& x, const Big_Int& y) noexcept
    {
        if (x.is_small()) {
            if (y.is_small()) {
                return x.get_i128() <=> y.get_i128();
            }
            const int result = cowel_big_int_compare_i128(y.get_host_handle(), x.get_i128());
            return -result <=> 0;
        }
        if (y.is_small()) {
            const int result = cowel_big_int_compare_i128(x.get_host_handle(), y.get_i128());
            return result <=> 0;
        }
        const int result = cowel_big_int_compare(x.get_host_handle(), y.get_host_handle());
        return result <=> 0;
    }

    [[nodiscard]]
    friend constexpr Big_Int operator+(const Big_Int& x, const Big_Int& y)
    {
        if (x.is_small()) {
            if (y.is_small()) {
                Int128 sum;
                if (add_overflow(sum, x.get_i128(), y.get_i128())) [[unlikely]] {
                    // When addition overflows, we are missing just one extra bit.
                    const auto d0 = Int64(sum);
                    const auto d1 = Int64(sum >> 64);
                    // If the sign of the result is negative, addition of positives had overflow.
                    // If the sign of the result is positive, addition of negatives had overflow.
                    // We need to sign-extend with the mathematically correct sign.
                    const auto d2 = Int64(sum < 0 ? 0 : -1);
                    const auto result = cowel_big_int_i192(d0, d1, d2);
                    return from_host_result(result);
                }
                return Big_Int { sum };
            }
            const auto result = cowel_big_int_add_i128(y.get_host_handle(), x.get_i128());
            return from_host_result(result);
        }
        if (y.is_small()) {
            const auto result = cowel_big_int_add_i128(x.get_host_handle(), y.get_i128());
            return from_host_result(result);
        }
        const auto result = cowel_big_int_add(x.get_host_handle(), y.get_host_handle());
        return from_host_result(result);
    }

    [[nodiscard]]
    friend constexpr Big_Int operator-(const Big_Int& x, const Big_Int& y)
    {
        if (x.is_small()) {
            if (y.is_small()) {
                Int128 diff;
                if (sub_overflow(diff, x.get_i128(), y.get_i128())) [[unlikely]] {
                    // See operator+ comments for explanation.
                    // operator- is analogous; if overflow happens, the sign bit is "wrong",
                    // or not really a sign bit.
                    const auto d0 = Int64(diff);
                    const auto d1 = Int64(diff >> 64);
                    const auto d2 = Int64(diff < 0 ? 0 : -1);
                    const auto result = cowel_big_int_i192(d0, d1, d2);
                    return from_host_result(result);
                }
                return Big_Int { diff };
            }
            const auto result = cowel_big_int_sub_i128(y.get_host_handle(), x.get_i128());
            return from_host_result(result);
        }
        if (y.is_small()) {
            const auto result = cowel_big_int_sub_i128(x.get_host_handle(), y.get_i128());
            return from_host_result(result);
        }
        const auto result = cowel_big_int_sub(x.get_host_handle(), y.get_host_handle());
        return from_host_result(result);
    }

    [[nodiscard]]
    friend constexpr Big_Int operator*(const Big_Int& x, const Big_Int& y)
    {
        if (x.is_small()) {
            if (y.is_small()) {
                Int128 product;
                if (mul_overflow(product, x.get_i128(), y.get_i128())) [[unlikely]] {
                    const auto result = cowel_big_int_mul_i128_i128(x.get_i128(), y.get_i128());
                    return from_host_result(result);
                }
                return Big_Int { product };
            }
            const auto result = cowel_big_int_mul_i128(y.get_host_handle(), x.get_i128());
            return from_host_result(result);
        }
        if (y.is_small()) {
            const auto result = cowel_big_int_mul_i128(x.get_host_handle(), y.get_i128());
            return from_host_result(result);
        }
        const auto result = cowel_big_int_mul(x.get_host_handle(), y.get_host_handle());
        return from_host_result(result);
    }

    [[nodiscard]]
    friend Big_Int operator/(const Big_Int& x, const Big_Int& y)
    {
        return div(x, y);
    }

    [[nodiscard]]
    friend Big_Int operator%(const Big_Int& x, const Big_Int& y)
    {
        return rem(x, y);
    }

    [[nodiscard]]
    friend constexpr Div_Result<Big_Int, Big_Int> div_rem(
        const Big_Int& x, //
        const Big_Int& y,
        const Div_Rounding rounding = Div_Rounding::to_zero
    )
    {
        if (y.is_small()) {
            COWEL_ASSERT(!y.is_zero());
        }
        if (x.is_small() && y.is_small()) {
            if (x.get_i128() == std::numeric_limits<Int128>::min() && y.get_i128() == -1)
                [[unlikely]] {
                return { from_host_result(cowel_big_int_pow2_i32(127)), zero };
            }
            switch (rounding) {
            case Div_Rounding::to_zero:
                return {
                    Big_Int { x.get_i128() / y.get_i128() },
                    Big_Int { x.get_i128() % y.get_i128() },
                };
            case Div_Rounding::to_pos_inf:
                return {
                    Big_Int { div_to_pos_inf(x.get_i128(), y.get_i128()) },
                    Big_Int { rem_to_pos_inf(x.get_i128(), y.get_i128()) },
                };
            case Div_Rounding::to_neg_inf:
                return {
                    Big_Int { div_to_neg_inf(x.get_i128(), y.get_i128()) },
                    Big_Int { rem_to_neg_inf(x.get_i128(), y.get_i128()) },
                };
            }
            COWEL_ASSERT_UNREACHABLE(u8"Invalid rounding.");
        }

        // While we normally avoid spilling small values into hosted integers,
        // division in particular is so expensive that the relative cost is lower.
        const Scoped_Handle xh = x.get_handle_or_upload();
        const Scoped_Handle yh = y.get_handle_or_upload();
        const cowel_big_int_handle_pair result
            = cowel_big_int_div_rem(rounding, xh.handle, yh.handle);
        COWEL_ASSERT(!cowel_big_int_div_result.div_by_zero);

        const auto [qh, rh] = std::bit_cast<std::array<Big_Int_Handle, 2>>(result);
        const Big_Int quotient = qh == Big_Int_Handle {}
            ? Big_Int { cowel_big_int_div_result.small_quotient }
            : from_host_result(qh);
        const Big_Int remainder = qh == Big_Int_Handle {}
            ? Big_Int { cowel_big_int_div_result.small_remainder }
            : from_host_result(rh);
        return { quotient, remainder };
    }

    [[nodiscard]]
    friend constexpr Big_Int
    div( //
        const Big_Int& x, //
        const Big_Int& y,
        const Div_Rounding rounding = Div_Rounding::to_zero
    )
    {
        if (y.is_small()) {
            COWEL_ASSERT(y.get_i128() != 0);
        }
        if (x.is_small() && y.is_small()) {
            if (x.get_i128() == std::numeric_limits<Int128>::min() && y.get_i128() == -1)
                [[unlikely]] {
                return from_host_result(cowel_big_int_pow2_i32(127));
            }
            switch (rounding) {
            case Div_Rounding::to_zero: {
                return Big_Int { x.get_i128() / y.get_i128() };
            }
            case Div_Rounding::to_pos_inf: {
                return Big_Int { div_to_pos_inf(x.get_i128(), y.get_i128()) };
            }
            case Div_Rounding::to_neg_inf: {
                return Big_Int { div_to_neg_inf(x.get_i128(), y.get_i128()) };
            }
            }
            COWEL_ASSERT_UNREACHABLE(u8"Invalid rounding.");
        }
        // See div_rem for rationale.
        const Scoped_Handle xh = x.get_handle_or_upload();
        const Scoped_Handle yh = y.get_handle_or_upload();
        return from_host_result(cowel_big_int_div(rounding, xh.handle, yh.handle));
    }

    [[nodiscard]]
    friend constexpr Big_Int
    rem( //
        const Big_Int& x, //
        const Big_Int& y,
        const Div_Rounding rounding = Div_Rounding::to_zero
    )
    {
        if (y.is_small()) {
            COWEL_ASSERT(y.get_i128() != 0);
        }
        if (x.is_small() && y.is_small()) {
            if (y.get_i128() == -1) [[unlikely]] {
                return zero;
            }
            switch (rounding) {
            case Div_Rounding::to_zero: {
                return Big_Int { x.get_i128() % y.get_i128() };
            }
            case Div_Rounding::to_pos_inf: {
                return Big_Int { rem_to_pos_inf(x.get_i128(), y.get_i128()) };
            }
            case Div_Rounding::to_neg_inf: {
                return Big_Int { rem_to_neg_inf(x.get_i128(), y.get_i128()) };
            }
            }
            COWEL_ASSERT_UNREACHABLE(u8"Invalid rounding.");
        }
        // See div_rem for rationale.
        const Scoped_Handle xh = x.get_handle_or_upload();
        const Scoped_Handle yh = y.get_handle_or_upload();
        const auto result = cowel_big_int_rem(rounding, xh.handle, yh.handle);
        COWEL_ASSERT(!cowel_big_int_div_result.div_by_zero);
        return from_host_result(result);
    }

    [[nodiscard]]
    friend constexpr Big_Int operator<<(const Big_Int& x, const int s)
    {
        if (s < 0) [[unlikely]] {
            if (s == std::numeric_limits<int>::min()) [[unlikely]] {
                return x >> std::numeric_limits<int>::max() >> 1;
            }
            return x >> -s;
        }
        if (x.is_small()) {
            const bool may_overflow //
                = s >= 63 //
                || x >= (Int128 { 1 } << 64) //
                || x <= -(Int128 { 1 } << 64);
            if (may_overflow) [[unlikely]] {
                return from_host_result(cowel_big_int_shl_i128_i32(x.get_i128(), s));
            }
            return Big_Int { x.get_i128() << s };
        }
        return from_host_result(cowel_big_int_shl_i32(x.get_host_handle(), s));
    }

    [[nodiscard]]
    friend constexpr Big_Int operator>>(const Big_Int& x, const int s)
    {
        if (s < 0) [[unlikely]] {
            if (s == std::numeric_limits<int>::min()) [[unlikely]] {
                return x << std::numeric_limits<int>::max() << 1;
            }
            return x << -s;
        }
        if (x.is_small()) {
            if (s >= 128) [[unlikely]] {
                const int result = x.get_i128() >= 0 ? 0 : -1;
                return Big_Int { result };
            }
            return Big_Int { x.get_i128() >> s };
        }
        return from_host_result(cowel_big_int_shr_i32(x.get_host_handle(), s));
    }

    /// @brief Returns x raised to the power of y.
    /// Shall not evaluate `pow(0, 0)`.
    [[nodiscard]]
    friend constexpr Big_Int pow(const Big_Int& x, const int y)
    {
        if (y < 0) [[unlikely]] {
            return zero;
        }
        if (y == 0) [[unlikely]] {
            COWEL_ASSERT(!x.is_zero());
            return one;
        }
        if (x.is_small()) {
            return from_host_result(cowel_big_int_pow_i128_i32(x.get_i128(), y));
        }
        return from_host_result(cowel_big_int_pow_i32(x.get_host_handle(), y));
    }

    [[nodiscard]]
    friend constexpr Big_Int operator&(const Big_Int& x, const Big_Int& y)
    {
        if (x.is_small()) {
            if (y.is_small()) {
                return Big_Int { x.get_i128() & y.get_i128() };
            }
            return from_host_result(cowel_big_int_bit_and_i128(y.get_host_handle(), x.get_i128()));
        }
        if (y.is_small()) {
            return from_host_result(cowel_big_int_bit_and_i128(x.get_host_handle(), y.get_i128()));
        }
        return from_host_result(cowel_big_int_bit_and(x.get_host_handle(), y.get_host_handle()));
    }

    [[nodiscard]]
    friend constexpr Big_Int operator|(const Big_Int& x, const Big_Int& y)
    {
        if (x.is_small()) {
            if (y.is_small()) {
                return Big_Int { x.get_i128() | y.get_i128() };
            }
            return from_host_result(cowel_big_int_bit_or_i128(y.get_host_handle(), x.get_i128()));
        }
        if (y.is_small()) {
            return from_host_result(cowel_big_int_bit_or_i128(x.get_host_handle(), y.get_i128()));
        }
        return from_host_result(cowel_big_int_bit_or(x.get_host_handle(), y.get_host_handle()));
    }

    [[nodiscard]]
    friend constexpr Big_Int operator^(const Big_Int& x, const Big_Int& y)
    {
        if (x.is_small()) {
            if (y.is_small()) {
                return Big_Int { x.get_i128() ^ y.get_i128() };
            }
            return from_host_result(cowel_big_int_bit_xor_i128(y.get_host_handle(), x.get_i128()));
        }
        if (y.is_small()) {
            return from_host_result(cowel_big_int_bit_xor_i128(x.get_host_handle(), y.get_i128()));
        }
        return from_host_result(cowel_big_int_bit_xor(x.get_host_handle(), y.get_host_handle()));
    }

    constexpr Big_Int& operator+=(const Big_Int& x)
    {
        return *this = *this + x;
    }
    constexpr Big_Int& operator-=(const Big_Int& x)
    {
        return *this = *this - x;
    }
    constexpr Big_Int& operator*=(const Big_Int& x)
    {
        return *this = *this * x;
    }
    constexpr Big_Int& operator/=(const Big_Int& x)
    {
        return *this = *this / x;
    }
    constexpr Big_Int& operator%=(const Big_Int& x)
    {
        return *this = *this % x;
    }
    constexpr Big_Int& operator&=(const Big_Int& x)
    {
        return *this = *this & x;
    }
    constexpr Big_Int& operator|=(const Big_Int& x)
    {
        return *this = *this | x;
    }
    constexpr Big_Int& operator^=(const Big_Int& x)
    {
        return *this = *this ^ x;
    }
    constexpr Big_Int& operator<<=(const int s)
    {
        return *this = *this << s;
    }
    constexpr Big_Int& operator>>=(const int s)
    {
        return *this = *this >> s;
    }

    // STRING CONVERSIONS ==========================================================================

    /// @brief Returns a string containing the digits representing this integer.
    /// @param base The base of the digits.
    /// Shall be in [2, 36].
    /// @param to_upper If `true`, outputs digits for base 11 or more in uppercase.
    void print_to(
        Function_Ref<void(std::string_view)> out, //
        const int base = 10,
        const bool to_upper = false
    ) const
    {
        constexpr std::size_t buffer_size = 8192;

        COWEL_ASSERT(base >= 2 && base <= 36);
        if (m_is_small) {
            const auto result = to_characters(get_i128(), base, to_upper);
            out(result);
            return;
        }
        constexpr int minus_sign_width = 1;
        const auto pessimistic_digit_count
            = std::size_t(cowel_big_int_ones_width(get_host_handle()) + minus_sign_width);

        if (pessimistic_digit_count <= buffer_size) {
            char buffer[buffer_size];
            const std::size_t length
                = cowel_big_int_to_string(buffer, buffer_size, get_host_handle(), base, to_upper);
            COWEL_ASSERT(length);
            const std::string_view string { buffer, length };
            out(string);
            return;
        }

        auto* const text = static_cast<char*>(cowel_alloc(pessimistic_digit_count, 1));
        const std::size_t length = cowel_big_int_to_string(
            text, pessimistic_digit_count, get_host_handle(), base, to_upper
        );
        COWEL_ASSERT(length);
        const std::string_view string { text, length };
        out(string);
        cowel_free(text, pessimistic_digit_count, 1);
    }

    void print_to(
        Function_Ref<void(std::u8string_view)> out,
        const int base = 10,
        const bool to_upper = false
    ) const
    {
        const Function_Ref<void(std::string_view)> delegate = {
            const_v<[](decltype(out)* const u8out, const std::string_view str) {
                (*u8out)(as_u8string_view(str));
            }>,
            &out,
        };
        print_to(delegate, base, to_upper);
    }

    /// @brief Analogous to
    /// ```cpp
    /// std::to_chars(digits.data(), digits.data() + digits.size(), out, base)
    /// ```
    /// if hypothetically, `std::to_chars` had big integer support.
    ///
    /// While there is no strict upper bound to `Big_Int`,
    /// `std::errc::result_out_of_range` may still be returned if parsing exceeds some
    /// implementation limit.
    /// @param digits A string starting with a sequence of digits in the given `base`.
    /// It is not required that the entire string is a valid digit sequence.
    /// @param out The object in which the result of parsing is stored upon success.
    /// Otherwise, it remains unmodified.
    /// @param base The base of the digit sequence.
    [[nodiscard]]
    friend std::from_chars_result
    from_characters(const std::string_view digits, Big_Int& out, const int base)
    {
        COWEL_ASSERT(base >= 2 && base <= 36);

        Int128 i128;
        const std::from_chars_result result = cowel::from_characters(digits, i128, base);
        if (result.ec == std::errc {}) {
            out = i128;
            return result;
        }
        if (result.ec == std::errc::invalid_argument) {
            return result;
        }
        COWEL_ASSERT(result.ec == std::errc::result_out_of_range);

        const std::size_t valid_digits = ascii::length_if(as_u8string_view(digits), [&](char8_t c) {
            return is_ascii_digit_base(c, base);
        });
        if (valid_digits == 0) {
            return { digits.data(), std::errc::invalid_argument };
        }
        const char* const end = digits.data() + valid_digits;

        const auto status = cowel_big_int_from_string(digits.data(), valid_digits, base);
        switch (status) {
        case cowel_big_int_from_string_status::small_result: {
            out = cowel_big_int_small_result;
            return { end, std::errc {} };
        }
        case cowel_big_int_from_string_status::big_result: {
            out = from_host_result(cowel_big_int_big_result);
            return { end, std::errc {} };
        }
        case cowel_big_int_from_string_status::invalid_argument: {
            if constexpr (is_debug_build) {
                COWEL_ASSERT_UNREACHABLE(u8"Lexing the digit sequence should have prevented this.");
            }
            return { end, std::errc::invalid_argument };
        }
        case cowel_big_int_from_string_status::result_out_of_range: {
            return { end, std::errc::result_out_of_range };
        }
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid status.");
    }

private:
    [[nodiscard, gnu::always_inline]]
    constexpr bool is_small() const
    {
        return m_is_small;
    }

    [[nodiscard, gnu::always_inline]]
    constexpr Int128 get_i128() const
    {
        COWEL_DEBUG_ASSERT(m_is_small);
        return std::bit_cast<Int128>(m_int128);
    }

    [[nodiscard]]
    constexpr Big_Int_Handle get_host_handle() const
    {
        COWEL_ASSERT(!m_is_small);
        // We have the invariant that any moved-from Big_Int_Impl is "small zero",
        // so it should be impossible that we hold an empty GC_Ref.
        COWEL_ASSERT(m_host_handle);
#ifdef COWEL_EMSCRIPTEN
        return m_host_handle->handle();
#else
        const auto node_address = reinterpret_cast<std::uintptr_t>(m_host_handle.unsafe_get_node());
        return Big_Int_Handle { node_address };
#endif
    }

    constexpr void set_zero()
    {
        if (!m_is_small) {
            m_host_handle.~GC_Ref();
        }
        m_is_small = true;
        m_int128 = {};
    }

    struct [[nodiscard]] Scoped_Handle {
        const Big_Int_Handle handle;
        const bool owned;

        [[nodiscard]]
        Scoped_Handle(Big_Int_Handle handle, bool owned)
            : handle { handle }
            , owned { owned }
        {
        }

        Scoped_Handle(const Scoped_Handle&) = delete;
        Scoped_Handle& operator=(const Scoped_Handle&) = delete;

        ~Scoped_Handle()
        {
            if (owned && handle != Big_Int_Handle {}) {
                const bool delete_success = cowel_big_int_delete(handle);
                COWEL_ASSERT(delete_success);
            }
        }
    };

    [[nodiscard]]
    Scoped_Handle get_handle_or_upload() const
    {
        if (m_is_small) {
            return Scoped_Handle { cowel_big_int_i128(get_i128()), true };
        }
        return Scoped_Handle { get_host_handle(), false };
    }
};

inline constexpr Big_Int Big_Int::zero { 0 };
inline constexpr Big_Int Big_Int::one { 1 };

[[nodiscard]]
std::from_chars_result from_characters(std::string_view digits, Big_Int& out, int base = 10);
[[nodiscard]]
inline std::from_chars_result
from_characters(const std::u8string_view digits, Big_Int& out, const int base = 10)
{
    return from_characters(as_string_view(digits), out, base);
}

[[nodiscard, gnu::always_inline]]
constexpr Big_Int operator""_n(const unsigned long long digits) noexcept
{
    return Big_Int(Int128 { digits });
}

static_assert(sizeof(Big_Int) <= 32);
static_assert(alignof(Big_Int) == alignof(unsigned long long));

} // namespace cowel

#endif
