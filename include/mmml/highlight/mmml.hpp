#include <span>
#include <string_view>
#include <vector>

#include "mmml/highlight/highlight.hpp"

#include "mmml/fwd.hpp"

namespace mmml {

/// @brief Uses the AST instructions to create syntax highlighting information.
/// A sequence of `Annotation_Span`s is appended to `out`,
/// where gaps between spans represent non-highlighted content such as plaintext or whitespace.
void highlight_mmml(
    std::pmr::vector<Highlight_Span>& out,
    std::u8string_view source,
    std::span<const AST_Instruction> instructions,
    const Highlight_Options& options = {}
);

} // namespace mmml
