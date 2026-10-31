#include "cowel/autofix.hpp"

#include <algorithm>
#include <memory>
#include <vector>

namespace cowel {
namespace {

/// @brief The implementation of apply_source_edits,
/// which takes the offset of @p source within its file into account.
[[nodiscard]]
std::pmr::u8string apply_source_edits_impl(
    const std::u8string_view source,
    const std::size_t source_begin,
    const File_Id file,
    const std::span<const Source_Edit> edits,
    std::pmr::memory_resource* const memory
)
{
    const std::size_t source_end = source_begin + source.size();
    std::pmr::vector<const Source_Edit*> applicable { memory };
    for (const Source_Edit& edit : edits) {
        if (edit.span.file != file || edit.span.begin < source_begin
            || edit.span.end() > source_end) {
            continue;
        }
        applicable.push_back(&edit);
    }
    // Sort by beginning of the span, and for equal beginnings,
    // by the end of the span in reverse order, so that longer spans come first.
    std::ranges::sort(applicable, [](const Source_Edit* x, const Source_Edit* y) {
        return x->span.begin != y->span.begin ? x->span.begin < y->span.begin
                                              : x->span.end() > y->span.end();
    });

    std::pmr::u8string result { memory };
    result.reserve(source.size());
    std::size_t position = source_begin;
    for (const Source_Edit* const edit : applicable) {
        if (edit->span.begin < position) {
            // Overlaps with a previous edit, which takes precedence.
            continue;
        }
        result.append(source.substr(position - source_begin, edit->span.begin - position));
        result.append(edit->replacement);
        position = edit->span.end();
    }
    result.append(source.substr(position - source_begin));
    return result;
}

} // namespace

std::pmr::u8string apply_source_edits(
    const std::u8string_view source,
    const File_Id file,
    const std::span<const Source_Edit> edits,
    std::pmr::memory_resource* const memory
)
{
    return apply_source_edits_impl(source, 0, file, edits, memory);
}

std::pmr::u8string apply_source_edits(
    const std::u8string_view source,
    const std::size_t source_begin,
    const File_Id file,
    const std::span<const Source_Edit> edits,
    std::pmr::memory_resource* const memory
)
{
    return apply_source_edits_impl(source, source_begin, file, edits, memory);
}

std::span<const Source_Edit> persist_source_edits(
    const std::span<const Source_Edit> edits,
    std::pmr::memory_resource* const memory
)
{
    if (edits.empty()) {
        return std::span<const Source_Edit> {};
    }
    // The memory is deliberately never deallocated,
    // so that the edits remain valid for as long as the memory resource does.
    auto* const copy = static_cast<Source_Edit*>(
        memory->allocate(edits.size() * sizeof(Source_Edit), alignof(Source_Edit))
    );
    std::uninitialized_copy(edits.begin(), edits.end(), copy);
    return std::span<const Source_Edit> { copy, edits.size() };
}

} // namespace cowel
