#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <new>
#include <string>
#include <string_view>

#include "cowel/settings.hpp"

#ifdef COWEL_EMSCRIPTEN
#error "This file should not be included in emscripten builds."
#endif

#include <boost/multiprecision/cpp_int.hpp>

#include "cowel/util/assert.hpp"

#include "cowel/big_int.hpp"

namespace cowel {

using boost::multiprecision::cpp_int;

namespace detail {

static_assert(sizeof(detail::Big_Int_Backend) == sizeof(cpp_int));
static_assert(alignof(detail::Big_Int_Backend) == alignof(cpp_int));

auto& Big_Int_Backend::get()
{
    return *std::launder(reinterpret_cast<cpp_int*>(m_storage));
}

Big_Int_Backend::Big_Int_Backend()
{
    new (m_storage) cpp_int {};
}

Big_Int_Backend::~Big_Int_Backend()
{
    get().~cpp_int();
}

} // namespace detail

namespace {

[[nodiscard]]
const cpp_int& access_handle(const Big_Int_Handle handle) noexcept
{
    const GC_Node* const node = detail::get_handle_node(handle);
    COWEL_ASSERT(node);
    return *std::launder(static_cast<cpp_int*>(node->get_object_pointer()));
}

[[nodiscard]]
Big_Int_Handle release_handle(GC_Ref<detail::Big_Int_Backend>& ref) noexcept
{
    GC_Node* const node = ref.unsafe_release_node();
    return Big_Int_Handle(reinterpret_cast<std::uintptr_t>(node));
}

/// @brief Wraps the `cpp_int` in a GC node
/// and returns a handle to that node.
/// Regardless whether the result fits into a 128-bit integer,
/// a node is always allocated.
[[nodiscard]]
Big_Int_Handle yield_big_result(const cpp_int& x)
{
    GC_Ref<detail::Big_Int_Backend> ref = gc_ref_make<detail::Big_Int_Backend>();
    ref->get() = x;
    return release_handle(ref);
}

/// @brief Wraps the `cpp_int` in a GC node
/// and returns a handle to that node.
/// Regardless whether the result fits into a 128-bit integer,
/// a node is always allocated.
[[nodiscard]]
Big_Int_Handle yield_big_result(cpp_int&& x)
{
    GC_Ref<detail::Big_Int_Backend> ref = gc_ref_make<detail::Big_Int_Backend>();
    ref->get() = std::move(x);
    return release_handle(ref);
}

[[nodiscard]]
int twos_width(const cpp_int& x) noexcept // NOLINT(bugprone-exception-escape)
{
    using boost::multiprecision::limb_type;
    constexpr int limb_bits = cpp_int::backend_type::limb_bits;

    const int sign = x.sign();
    if (sign == 0) [[unlikely]] {
        return 1;
    }
    if (sign > 0) {
        return int(msb(x)) + 2;
    }
    const std::size_t limb_count = x.backend().size();
    const auto* const limbs = x.backend().limbs();
    for (std::size_t i = limb_count; i-- > 0;) {
        if (limb_type significant = ~limbs[i]) {
            return int(i * limb_bits) + int(limb_bits - std::countl_zero(significant));
        }
    }
    return 1;
}

[[nodiscard]]
Big_Int_Handle yield_result(const cpp_int& x)
{
    if (twos_width(x) <= 128) {
        cowel_big_int_small_result = Int128(x);
        return {};
    }
    return yield_big_result(x);
}

[[nodiscard]]
Big_Int_Handle yield_result(cpp_int&& x)
{
    if (twos_width(x) <= 128) {
        cowel_big_int_small_result = Int128(x);
        x = {};
        return {};
    }
    return yield_big_result(std::move(x));
}

[[nodiscard]]
Div_Result<cpp_int> div_rem_to_zero(const cpp_int& x, const cpp_int& y)
{
    COWEL_ASSERT(!y.is_zero());
    Div_Result<cpp_int> result;
    divide_qr(x, y, result.quotient, result.remainder);
    return result;
}

[[nodiscard]]
Div_Result<cpp_int> div_rem_to_pos_inf(const cpp_int& x, const cpp_int& y)
{
    auto result = div_rem_to_zero(x, y);
    const bool quotient_positive = (x.sign() ^ y.sign()) >= 0;
    if (quotient_positive && !result.remainder.is_zero()) {
        ++result.quotient;
        result.remainder -= y;
    }
    return result;
}

[[nodiscard]]
cpp_int div_to_pos_inf(const cpp_int& x, const cpp_int& y)
{
    return div_rem_to_pos_inf(x, y).quotient;
}

[[nodiscard]]
cpp_int rem_to_pos_inf(const cpp_int& x, const cpp_int& y)
{
    cpp_int result = x % y;
    const bool quotient_positive = (x.sign() ^ y.sign()) >= 0;
    if (quotient_positive && !result.is_zero()) {
        result -= y;
    }
    return result;
}

[[nodiscard]]
Div_Result<cpp_int> div_rem_to_neg_inf(const cpp_int& x, const cpp_int& y)
{
    auto result = div_rem_to_zero(x, y);
    const bool quotient_negative = (x.sign() ^ y.sign()) < 0;
    if (quotient_negative && !result.remainder.is_zero()) {
        --result.quotient;
        result.remainder += y;
    }
    return result;
}

[[nodiscard]]
cpp_int div_to_neg_inf(const cpp_int& x, const cpp_int& y)
{
    return div_rem_to_neg_inf(x, y).quotient;
}

[[nodiscard]]
cpp_int rem_to_neg_inf(const cpp_int& x, const cpp_int& y)
{
    cpp_int result = x % y;
    const bool quotient_negative = (x.sign() ^ y.sign()) >= 0;
    if (quotient_negative && result != 0) {
        result += y;
    }
    return result;
}

} // namespace

} // namespace cowel

extern "C" {

using namespace cowel;

cowel_big_int_handle cowel_big_int_i32(const cowel::Int32 x)
{
    return yield_big_result(x);
}

cowel_big_int_handle cowel_big_int_i64(const cowel::Int64 x)
{
    return yield_big_result(x);
}

cowel_big_int_handle cowel_big_int_i128(const cowel::Int128 x)
{
    return yield_big_result(x);
}

cowel_big_int_handle cowel_big_int_i192(
    const cowel::Int64 d0, //
    const cowel::Int64 d1,
    const cowel::Int64 d2
)
{
    cpp_int result;
    COWEL_DEBUG_ASSERT(result.is_zero());
    result = d2;
    result <<= 64;
    result |= Uint64(d1);
    result <<= 64;
    result |= Uint64(d0);
    return yield_result(std::move(result));
}

cowel_big_int_handle cowel_big_int_pow2_i32(const cowel::Int32 x)
{
    if (x < 0) {
        cowel_big_int_small_result = 0;
        return {};
    }
    if (x < 127) {
        cowel_big_int_small_result = Int128 { 1 } << x;
        return {};
    }
    GC_Ref<detail::Big_Int_Backend> ref = gc_ref_make<detail::Big_Int_Backend>();
    bit_set(ref->get(), unsigned(x));
    return release_handle(ref);
}

bool cowel_big_int_delete(const cowel_big_int_handle x)
{
    if (GC_Node* const node = detail::get_handle_node(x)) {
        node->drop_reference();
        return true;
    }
    return false;
}

int cowel_big_int_compare_i32(const cowel_big_int_handle x, const cowel::Int32 y)
{
    const cpp_int& x_int = access_handle(x);
    const int result = x_int.compare(y);
    return (result > 0) - (result < 0);
}

int cowel_big_int_compare_i128(const cowel_big_int_handle x, const cowel::Int128 y)
{
    const cpp_int& x_int = access_handle(x);
    const int result = x_int.compare(y);
    return (result > 0) - (result < 0);
}

int cowel_big_int_compare(const cowel_big_int_handle x, const cowel_big_int_handle y)
{
    const cpp_int& x_int = access_handle(x);
    const cpp_int& y_int = access_handle(y);
    const int result = x_int.compare(y_int);
    return (result > 0) - (result < 0);
}

int cowel_big_int_twos_width(const cowel_big_int_handle x)
{
    return twos_width(access_handle(x));
}

int cowel_big_int_ones_width(const cowel_big_int_handle x)
{
    const cpp_int& x_int = access_handle(x);
    const int sign = x_int.sign();
    if (sign == 0) [[unlikely]] {
        return 1;
    }
    return sign > 0 ? int(msb(x_int)) + 2 //
                    : int(msb(abs(x_int))) + 2;
}

cowel_big_int_handle cowel_big_int_neg(const cowel_big_int_handle x)
{
    return yield_result(-access_handle(x));
}

cowel_big_int_handle cowel_big_int_bit_not(const cowel_big_int_handle x)
{
    return yield_result(~access_handle(x));
}

cowel_big_int_handle cowel_big_int_abs(const cowel_big_int_handle x)
{
    const cpp_int& x_int = access_handle(x);
    if (x_int.sign() >= 0) {
        return yield_result(x_int);
    }
    return yield_result(-x_int);
}

bool cowel_big_int_trunc_i128(const cowel_big_int_handle x)
{
    const cpp_int& x_int = access_handle(x);
    const auto result = Int128(Uint128(x_int & ~Uint128(0)));
    cowel_big_int_small_result = result;
    return result != x_int;
}

cowel_big_int_handle cowel_big_int_add_i32(const cowel_big_int_handle x, const cowel::Int32 y)
{
    return yield_result(access_handle(x) + y);
}

cowel_big_int_handle cowel_big_int_add_i128(const cowel_big_int_handle x, const cowel::Int128 y)
{
    return yield_result(access_handle(x) + y);
}

cowel_big_int_handle cowel_big_int_add(const cowel_big_int_handle x, const cowel_big_int_handle y)
{
    const cpp_int& x_int = access_handle(x);
    const cpp_int& y_int = access_handle(y);
    return yield_result(x_int + y_int);
}

cowel_big_int_handle cowel_big_int_sub_i128(const cowel_big_int_handle x, const cowel::Int128 y)
{
    return yield_result(access_handle(x) - y);
}

cowel_big_int_handle cowel_big_int_sub(const cowel_big_int_handle x, const cowel_big_int_handle y)
{
    const cpp_int& x_int = access_handle(x);
    const cpp_int& y_int = access_handle(y);
    return yield_result(x_int - y_int);
}

cowel_big_int_handle cowel_big_int_mul_i128(const cowel_big_int_handle x, const cowel::Int128 y)
{
    return yield_result(access_handle(x) * y);
}

cowel_big_int_handle cowel_big_int_mul_i128_i128(const cowel::Int128 x, const cowel::Int128 y)
{
    cpp_int product = x;
    product *= y;
    return yield_result(std::move(product));
}

cowel_big_int_handle cowel_big_int_mul(const cowel_big_int_handle x, const cowel_big_int_handle y)
{
    const cpp_int& x_int = access_handle(x);
    const cpp_int& y_int = access_handle(y);
    return yield_result(x_int * y_int);
}

cowel_big_int_handle_pair cowel_big_int_div_rem(
    const cowel::Div_Rounding rounding, //
    const cowel_big_int_handle x,
    const cowel_big_int_handle y
)
{
    const cpp_int& x_int = access_handle(x);
    const cpp_int& y_int = access_handle(y);
    if (y_int.is_zero()) {
        cowel_big_int_div_result = {
            .small_quotient = 0,
            .small_remainder = 0,
            .div_by_zero = true,
        };
        return {};
    }

    auto div_result = [&] -> Div_Result<cpp_int> {
        switch (rounding) {
        case cowel::Div_Rounding::to_zero: {
            return div_rem_to_zero(x_int, y_int);
        }
        case cowel::Div_Rounding::to_pos_inf: {
            return div_rem_to_pos_inf(x_int, y_int);
        }
        case cowel::Div_Rounding::to_neg_inf: {
            return div_rem_to_neg_inf(x_int, y_int);
        }
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid rounding.");
    }();

    std::array<Big_Int_Handle, 2> handles;
    if (twos_width(div_result.quotient) <= 128) {
        handles[0] = {};
        cowel_big_int_div_result.small_quotient = Int128(div_result.quotient);
    }
    else {
        handles[0] = yield_big_result(std::move(div_result.quotient));
    }
    if (twos_width(div_result.remainder) <= 128) {
        handles[1] = {};
        cowel_big_int_div_result.small_remainder = Int128(div_result.remainder);
    }
    else {
        handles[1] = yield_big_result(std::move(div_result.remainder));
    }
    cowel_big_int_div_result.div_by_zero = false;
    return std::bit_cast<cowel_big_int_handle_pair>(handles);
}

cowel_big_int_handle cowel_big_int_div(
    const cowel::Div_Rounding rounding,
    const cowel_big_int_handle x,
    const cowel_big_int_handle y
)
{
    const cpp_int& x_int = access_handle(x);
    const cpp_int& y_int = access_handle(y);
    if (y_int.is_zero()) {
        cowel_big_int_div_result.div_by_zero = true;
        return {};
    }

    switch (rounding) {
    case cowel::Div_Rounding::to_zero: {
        return yield_result(x_int / y_int);
    }
    case cowel::Div_Rounding::to_pos_inf: {
        return yield_result(div_to_pos_inf(x_int, y_int));
    }
    case cowel::Div_Rounding::to_neg_inf: {
        return yield_result(div_to_neg_inf(x_int, y_int));
    }
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid rounding.");
}

cowel_big_int_handle cowel_big_int_rem(
    const cowel::Div_Rounding rounding,
    const cowel_big_int_handle x,
    const cowel_big_int_handle y
)
{
    const cpp_int& x_int = access_handle(x);
    const cpp_int& y_int = access_handle(y);
    if (y_int.is_zero()) {
        cowel_big_int_div_result.div_by_zero = true;
        return {};
    }

    switch (rounding) {
    case cowel::Div_Rounding::to_zero: {
        return yield_result(x_int % y_int);
    }
    case cowel::Div_Rounding::to_pos_inf: {
        return yield_result(rem_to_pos_inf(x_int, y_int));
    }
    case cowel::Div_Rounding::to_neg_inf: {
        return yield_result(rem_to_neg_inf(x_int, y_int));
    }
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid rounding.");
}

cowel_big_int_handle cowel_big_int_shl_i128_i32(const cowel::Int128 x, const cowel::Int32 s)
{
    cpp_int result = x;
    if (s >= 0) {
        result <<= s;
    }
    else {
        result >>= s;
    }
    return yield_result(std::move(result));
}

cowel_big_int_handle cowel_big_int_shl_i32(const cowel_big_int_handle x, const cowel::Int32 s)
{
    const cpp_int& x_int = access_handle(x);
    auto result = s >= 0 ? cpp_int(x_int << s) //
                         : cpp_int(x_int >> s);
    return yield_result(std::move(result));
}

cowel_big_int_handle cowel_big_int_pow_i128_i32(const cowel::Int128 x, const cowel::Int32 y)
{
    if (y < 0) [[unlikely]] {
        cowel_big_int_small_result = 0;
        return {};
    }
    if (y == 0) [[unlikely]] {
        cowel_big_int_small_result = x == 0 ? 0 : 1;
        return {};
    }
    const cpp_int x_int = x;
    return yield_result(pow(x_int, unsigned(y)));
}

cowel_big_int_handle cowel_big_int_pow_i32(const cowel_big_int_handle x, const cowel::Int32 y)
{
    if (y < 0) [[unlikely]] {
        cowel_big_int_small_result = 0;
        return {};
    }
    const cpp_int& x_int = access_handle(x);
    if (y == 0) [[unlikely]] {
        cowel_big_int_small_result = x_int.is_zero() ? 0 : 1;
        return {};
    }
    return yield_result(pow(x_int, unsigned(y)));
}

cowel_big_int_handle cowel_big_int_shr_i32(const cowel_big_int_handle x, const cowel::Int32 s)
{
    const cpp_int& x_int = access_handle(x);
    auto result = s >= 0 ? cpp_int(x_int >> s) //
                         : cpp_int(x_int << s);
    return yield_result(std::move(result));
}

cowel_big_int_handle cowel_big_int_bit_and_i128(const cowel_big_int_handle x, const cowel::Int128 y)
{
    return yield_result(access_handle(x) & y);
}

cowel_big_int_handle
cowel_big_int_bit_and(const cowel_big_int_handle x, const cowel_big_int_handle y)
{
    const cpp_int& x_int = access_handle(x);
    const cpp_int& y_int = access_handle(y);
    return yield_result(x_int & y_int);
}

cowel_big_int_handle cowel_big_int_bit_or_i128(const cowel_big_int_handle x, const cowel::Int128 y)
{
    return yield_result(access_handle(x) | y);
}

cowel_big_int_handle
cowel_big_int_bit_or(const cowel_big_int_handle x, const cowel_big_int_handle y)
{
    const cpp_int& x_int = access_handle(x);
    const cpp_int& y_int = access_handle(y);
    return yield_result(x_int | y_int);
}

cowel_big_int_handle cowel_big_int_bit_xor_i128(const cowel_big_int_handle x, const cowel::Int128 y)
{
    return yield_result(access_handle(x) ^ y);
}

cowel_big_int_handle
cowel_big_int_bit_xor(const cowel_big_int_handle x, const cowel_big_int_handle y)
{
    const cpp_int& x_int = access_handle(x);
    const cpp_int& y_int = access_handle(y);
    return yield_result(x_int ^ y_int);
}

std::size_t cowel_big_int_to_string(
    char* const buffer,
    const std::size_t size,
    const cowel_big_int_handle x,
    const int base,
    const bool to_upper
)
{
    if (buffer == nullptr || size == 0 || base < 2 || base > 36) {
        return 0;
    }

    const cpp_int& x_int = access_handle(x);
    std::string result;
    const int sign = x_int.sign();
    if (sign == 0) {
        result = "0";
        goto successful_end;
    }

    switch (base) {
    case 2: {
        constexpr auto append_digits = [](std::string& out, const cpp_int& x) {
            const auto limit = int(msb(x));
            out.reserve(std::size_t(limit) + 1);
            for (int b = limit; b >= 0; --b) {
                out += bit_test(x, unsigned(b)) ? '1' : '0';
            }
        };
        if (sign < 0) {
            result += '-';
            append_digits(result, abs(x_int));
        }
        else {
            append_digits(result, x_int);
        }
        break;
    }
    case 10: {
        result = x_int.str();
        break;
    }
    case 8:
    case 16: {
        // For octal and hex, we can use Boost's str() member function.
        // However, for some weird reason, printing negative octal and hex numbers
        // is not supported, so we need to negative ourselves.
        const auto flags = base == 16 ? std::ios_base::hex : std::ios_base::oct;
        if (sign < 0) {
            result += '-';
            result += cpp_int(abs(x_int)).str(0, flags);
        }
        else {
            result += x_int.str(0, flags);
        }
        break;
    }
    default: {
        cpp_int quotient;
        cpp_int remainder;
        auto temp = x_int;
        if (sign < 0) {
            temp = -temp;
        }
        const cpp_int cpp_base { base };
        while (!temp.is_zero()) {
            divide_qr(temp, cpp_base, quotient, remainder);
            temp = quotient;
            char buffer[2] {};
            const auto int_remainder = remainder.convert_to<int>();
            const auto [p, ec] = std::to_chars(buffer, std::end(buffer), int_remainder, base);
            COWEL_ASSERT(ec == std::errc {});
            const std::string_view buffer_string { buffer, p };
            result += buffer_string;
        }
        if (sign < 0) {
            result += '-';
        }
        std::ranges::reverse(result);
        break;
    }
    }
    if (result.size() > size) {
        return 0;
    }
    if (base > 10 && to_upper) {
        for (char& c : result) {
            c = char(to_ascii_upper(char8_t(c)));
        }
    }
successful_end:
    std::memcpy(buffer, result.data(), result.size());
    // Ensure null termination if there is sufficient space.
    if (size > result.size()) {
        buffer[result.size()] = 0;
    }
    return result.size();
}

cowel_big_int_from_string_status cowel_big_int_from_string(
    const char* const buffer, //
    const std::size_t size,
    const int base
)
{
    if (buffer == nullptr || size == 0 || base < 2 || base > 36) {
        return cowel_big_int_from_string_status::invalid_argument;
    }
    const std::string_view digits { buffer, size };

    cpp_int result;

    const char* end = digits.data() + digits.size();
    const char* p = digits.data();
    if (*p == '-') {
        ++p;
    }
    if (p == end) {
        return cowel_big_int_from_string_status::invalid_argument;
    }
    if (base == 10) {
        int digits_length = 0;
        for (; digits_length < end - p; ++digits_length) {
            if (!is_ascii_digit(char8_t(p[digits_length]))) {
                break;
            }
        }
        if (digits_length == 0) {
            return cowel_big_int_from_string_status::invalid_argument;
        }
        result.assign(std::string_view { p, p + digits_length });
    }
    else {
        const int pow_2_shift
            = std::has_single_bit(unsigned(base)) ? std::countr_zero(unsigned(base)) : 0;
        bool at_least_one_digit = false;
        for (; p != end; ++p) {
            const char c = *p;
            int digit;

            if ('0' <= c && c <= '9') {
                digit = c - '0';
            }
            else if ('A' <= c && c <= 'Z') {
                digit = c - 'A' + 10;
            }
            else if ('a' <= c && c <= 'z') {
                digit = c - 'a' + 10;
            }
            else {
                return cowel_big_int_from_string_status::invalid_argument;
            }
            if (digit > base) {
                return cowel_big_int_from_string_status::invalid_argument;
            }
            at_least_one_digit = true;
            if (pow_2_shift) {
                result <<= pow_2_shift;
                result |= digit;
            }
            else {
                result *= base;
                result += digit;
            }
        }
        if (!at_least_one_digit) {
            return cowel_big_int_from_string_status::invalid_argument;
        }
    }
    if (digits.starts_with('-')) {
        result = -result;
    }
    if (twos_width(result) <= 128) {
        cowel_big_int_small_result = Int128(result);
        COWEL_DEBUG_ASSERT(cowel_big_int_small_result == result);
        return cowel_big_int_from_string_status::small_result;
    }
    cowel_big_int_big_result = yield_big_result(result);
    return cowel_big_int_from_string_status::big_result;
}

//
}
