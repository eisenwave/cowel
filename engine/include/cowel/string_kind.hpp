#ifndef COWEL_STRING_KIND_HPP
#define COWEL_STRING_KIND_HPP

#include "cowel/fwd.hpp"

namespace cowel {

enum struct String_Kind : Default_Underlying {
    /// @brief A string
    /// possibly containing code points outside the Basic Latin block and
    /// possibly containing malformed code units.
    unknown,
    /// @brief A string that  consist entirely of code points in the Basic Latin block,
    /// i.e. "ASCII characters".
    /// This also includes empty strings.
    ascii,
    /// @brief A correctly encoded string,
    /// possibly containing code points outside the Basic Latin block.
    unicode,
};

} // namespace cowel

#endif
