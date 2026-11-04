#ifndef COWEL_AUTOFIX_HPP
#define COWEL_AUTOFIX_HPP

#include <memory_resource>
#include <span>
#include <string>

#include "cowel/util/source_position.hpp"

namespace cowel {

/// @brief A replacement of a span of source code with new text.
///
/// A sequence of `Source_Edit`s describes an auto-fix for a diagnostic.
/// The edits of an auto-fix must not overlap each other.
struct Source_Edit {
    /// @brief The span of source code that is replaced.
    File_Source_Span span;
    /// @brief The text which replaces the span.
    /// The replacement is a deletion when this is empty.
    std::pmr::u8string replacement;
};

/// @brief Produces a copy of @p source in which all @p edits have been applied.
///
/// Edits in files other than @p file, and edits whose span is out of range,
/// are ignored.
/// When two edits overlap, the edit which starts first takes precedence,
/// and the overlapping edit is ignored.
///
/// @param source The source code to be fixed.
/// @param file The file which @p source is the contents of.
/// @param edits The edits to apply, in any order.
/// @param memory The memory resource for the result.
[[nodiscard]]
std::pmr::u8string apply_source_edits(
    std::u8string_view source,
    File_Id file,
    std::span<const Source_Edit> edits,
    std::pmr::memory_resource* memory
);

/// @brief Produces a copy of a part of a file in which all @p edits have been applied.
///
/// This is the same as the other overload,
/// except that @p source is only part of the file which the spans of the edits refer to.
///
/// Edits in files other than @p file,
/// as well as edits whose span extends outside of
/// `[source_begin, source_begin + source.size())`, are ignored.
///
/// @param source The part of the file to be fixed.
/// @param source_begin The offset of the first character of @p source within its file.
/// @param file The file which @p source is a part of.
/// @param edits The edits to apply, in any order.
/// @param memory The memory resource for the result.
[[nodiscard]]
std::pmr::u8string apply_source_edits(
    std::u8string_view source,
    std::size_t source_begin,
    File_Id file,
    std::span<const Source_Edit> edits,
    std::pmr::memory_resource* memory
);

} // namespace cowel

#endif
