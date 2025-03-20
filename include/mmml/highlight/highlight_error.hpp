#ifndef MMML_HIGHLIGHT_ERROR_HPP
#define MMML_HIGHLIGHT_ERROR_HPP

#include "mmml/fwd.hpp"

namespace mmml {

enum struct Syntax_Highlight_Error : Default_Underlying {
    /// @brief A given language hint is not supported,
    /// and the language couldn't be determined automatically.
    unsupported_language,
    /// @brief Code cannot be highlighted because it's ill-formed,
    /// and the syntax highlighter does not tolerate ill-formed code.
    bad_code,
};

} // namespace mmml

#endif
