#ifndef COWEL_META_HPP
#define COWEL_META_HPP

#include <concepts>
#include <type_traits>

#include "ulight/const.hpp"

#include "cowel/settings.hpp"

namespace cowel {

using ulight::const_if_t;
using ulight::const_like_t;
using ulight::const_v;
using ulight::Constant;
using ulight::Follow_Ref_Const_If;
using ulight::follow_ref_const_if_t;

template <typename>
inline constexpr bool dependent_false = false;

template <typename T>
concept trivial = std::is_trivially_copyable_v<T> && std::is_trivially_default_constructible_v<T>;

template <typename T>
concept byte_sized = sizeof(T) == 1;

template <typename T>
concept byte_like = byte_sized<T> && trivial<T>;

template <typename T, typename... Us>
concept one_of = (std::same_as<T, Us> || ...);

template <typename T>
concept char_like = byte_like<T> && one_of<T, char, char8_t>;

template <typename T>
concept cv_unqualified = !std::is_const_v<T> && !std::is_volatile_v<T>;

template <typename T>
concept signed_or_unsigned = one_of<
    T,
    signed char,
    signed short,
    signed int,
    signed long,
    signed long long,
    Int128,
    unsigned char,
    unsigned short,
    unsigned int,
    unsigned long,
    unsigned long long,
    Uint128>;

template <typename T>
concept no_cv_floating = cv_unqualified<T> && std::floating_point<T>;

} // namespace cowel

#endif
