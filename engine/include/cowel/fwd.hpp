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

enum struct Syntax_Highlight_Error : Default_Underlying {
    unsupported_language,
    bad_code,
    other,
};

/// @brief A numeric file identifier.
enum struct File_Id : int { main = -1 }; // NOLINT(performance-enum-size)

/// @brief The stack frame of a directive invocation, as it was pushed
/// when the invocation was processed.
struct Stack_Frame;

template <typename T>
struct GC_Ref;

/// @brief An owning, nullable reference to the stack frame in which some content appears.
///
/// Since content can be spliced lazily,
/// a value which holds a block may need to keep the frame in which the block was created alive,
/// because the parameters of an enclosing function call live in that frame.
/// This is why the reference is owning and is held by `Value`, not just by the processing pass.
///
/// A null reference expresses top-level content.
using Frame_Ref = GC_Ref<Stack_Frame>;

} // namespace cowel

#endif
