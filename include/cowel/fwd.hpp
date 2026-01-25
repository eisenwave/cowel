#ifndef COWEL_CONFIG_HPP
#define COWEL_CONFIG_HPP

#include "cowel/settings.hpp"

COWEL_IF_DEBUG() // silence unused warning for settings.hpp

namespace cowel {

/// @brief The default underlying type for scoped enumerations.
using Default_Underlying = unsigned char;

#define COWEL_ENUM_STRING_CASE(...)                                                                \
    case __VA_ARGS__: return #__VA_ARGS__

#define COWEL_ENUM_STRING_CASE8(...)                                                               \
    case __VA_ARGS__: return u8## #__VA_ARGS__

struct Content_Policy;
struct Context;

/// @brief The floating-point type corresponding to COWEL's `float` type.
using Float = double;
/// @brief The type corresponding to COWEL's `int` type.
/// While the COWEL language has unbounded integers,
/// this is not implemented yet.
/// Using 128-bit integers at least provides support beyond 64-bit.
using Integer = Int128;

enum struct Syntax_Highlight_Error : Default_Underlying {
    unsupported_language,
    bad_code,
    other,
};

/// @brief A numeric file identifier.
enum struct File_Id : int { main = -1 }; // NOLINT(performance-enum-size)

/// @brief A stack frame index.
/// The special value `root = -1` expresses top-level content,
/// i.e. content which is not expanded from any macro.
enum struct Frame_Index : int { root = -1 }; // NOLINT(performance-enum-size)

} // namespace cowel

#endif
