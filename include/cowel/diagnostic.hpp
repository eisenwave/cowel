#ifndef COWEL_DIAGNOSTIC_HPP
#define COWEL_DIAGNOSTIC_HPP

#include <compare>
#include <span>
#include <string_view>

#include "cowel/util/source_position.hpp"

#include "cowel/fwd.hpp"

namespace cowel {

enum struct Severity : Default_Underlying {
    /// @brief Alias for `debug`.
    min = 0,
    /// @brief Debugging messages.
    /// Only emitted in debug mode.
    debug = 0,
    /// @brief Minor problems. Only emitted in verbose mode.
    soft_warning = 1,
    /// @brief Major problems with the document.
    warning = 2,
    /// @brief Problems with the document that prevent proper content generation.
    /// Usually results in the generation of `\\error` directives.
    error = 3,
    /// @brief Alias for `error`.
    max = 3,
    /// @brief Greater than all other levels.
    /// No diagnostic with this level is emitted, so using it as a minimum level
    /// silences all diagnostics, even errors.
    none = 4,
};

[[nodiscard]]
constexpr std::strong_ordering operator<=>(Severity x, Severity y) noexcept
{
    return Default_Underlying(x) <=> Default_Underlying(y);
}

[[nodiscard]]
constexpr bool severity_is_emittable(Severity x) noexcept
{
    return x >= Severity::min && x <= Severity::max;
}

struct Diagnostic {
    /// @brief The severity of the diagnostic.
    /// `severity_is_emittable(severity)` shall be `true`.
    Severity severity;
    /// @brief The id of the diagnostic,
    /// which is a non-empty string containing a
    /// dot-separated sequence of identifier for this diagnostic.
    std::u8string_view id;
    /// @brief The span of code that is responsible for this diagnostic.
    Source_Span location;
    /// @brief The diagnostic message parts.
    std::span<const std::u8string_view> message;
};

namespace diagnostic {

// GENERAL DIAGNOSTICS =============================================================================

/// @brief A duplicate `id` attribute would have been generated.
inline constexpr std::u8string_view duplicate_id = u8"id.duplicate";

/// @brief In document post-processing,
/// a reference to a section was found that is not valid.
inline constexpr std::u8string_view section_ref_not_found = u8"section-ref.not-found";

/// @brief In document post-processing,
/// a reference to a section forms a circular dependency.
inline constexpr std::u8string_view section_ref_circular = u8"section-ref.circular";

/// @brief When loading a syntax highlighting theme,
/// conversion from JSON to to CSS failed.
inline constexpr std::u8string_view theme_conversion = u8"theme.conversion";

/// @brief Directive lookup failed.
inline constexpr std::u8string_view directive_lookup_unresolved = u8"directive-lookup.unresolved";

/// @brief Arguments to a directive were ignored.
inline constexpr std::u8string_view ignored_args = u8"ignored.args";

/// @brief The content of a directive was ignored.
inline constexpr std::u8string_view ignored_content = u8"ignored.content";

/// @brief When parsing, a directive block was not terminated via closing brace.
inline constexpr std::u8string_view parse_block_unclosed = u8"parse.block.unclosed";

/// @brief In syntax highlighting,
/// the given language is not supported.
inline constexpr std::u8string_view highlight_language = u8"highlight.language";
/// @brief In syntax highlighting,
/// the code could not be highlighted because it is malformed.
inline constexpr std::u8string_view highlight_malformed = u8"highlight.malformed";
/// @brief In syntax highlighting,
/// something went wrong.
inline constexpr std::u8string_view highlight_error = u8"highlight.error";

// DIRECTIVE-SPECIFIC DIAGNOSTICS ==================================================================

namespace c {

/// @brief In `\\c`, the input is blank.
inline constexpr std::u8string_view blank = u8"c:blank";
/// @brief In `\\c`, the name is invalid, like `\\c{nonsense}`.
inline constexpr std::u8string_view name = u8"c:name";
/// @brief In `\\c`, parsing digits failed, like `\\c{#x1234abc}`.
inline constexpr std::u8string_view digits = u8"c:digits";
/// @brief In `\\c`, a nonscalar value would be encoded.
/// @see is_scalar_value
inline constexpr std::u8string_view nonscalar = u8"c:nonscalar";

} // namespace c

namespace code {

/// @brief In `\code`, the given `nested` parameter is not `yes` or `no`.
inline constexpr std::u8string_view nested_invalid = u8"code:nested.invalid";

} // namespace code

namespace codeblock {

/// @brief In `\codeblock`, the given `borders` parameter is not `yes` or `no`.
inline constexpr std::u8string_view borders_invalid = u8"codeblock:borders.invalid";

} // namespace codeblock

namespace U {

/// @brief In `\\U`, the input is blank.
inline constexpr std::u8string_view blank = u8"U:blank";
/// @brief In `\\U`, parsing digits failed, like `\\U{abc}`.
inline constexpr std::u8string_view digits = u8"U:digits";
/// @brief In `\\U`, a nonscalar value would be encoded.
/// @see is_scalar_value
inline constexpr std::u8string_view nonscalar = u8"U:nonscalar";

} // namespace U

namespace hl {

/// @brief In a `\hl` directive,
/// no name parameter was provided.
inline constexpr std::u8string_view name_missing = u8"hl:name.missing";
/// @brief In a `\hl` directive,
/// the given highlight name is not valid.
inline constexpr std::u8string_view name_invalid = u8"hl:name.invalid";

} // namespace hl

namespace there {

/// @brief In a `\there` directive,
/// no section was provided.
inline constexpr std::u8string_view no_section = u8"there:no-section";

} // namespace there

namespace here {

/// @brief In a `\here` directive,
/// no section was provided.
inline constexpr std::u8string_view no_section = u8"here:no-section";

} // namespace here

namespace ref {

/// @brief In a `\ref` directive,
/// no `to` argument was provided,
/// meaning that the reference is without any target.
inline constexpr std::u8string_view to_missing = u8"ref:to.missing";

/// @brief In a `\ref` directive,
/// the target is empty.
inline constexpr std::u8string_view to_empty = u8"ref:to.empty";

/// @brief In a `\ref` directive,
/// the target cannot be classified as a URL or anything else,
/// and the target cannot be resolved as a document.
inline constexpr std::u8string_view to_unresolved = u8"ref:to.unresolved";

/// @brief In a `\ref` directive where the target is a C++ draft URL,
/// failed to verbalize the URL.
inline constexpr std::u8string_view draft_verbalization = u8"ref:draft.verbalization";

} // namespace ref

namespace bib {

/// @brief In a `\bib` directive,
/// no `id` was provided.
inline constexpr std::u8string_view id_missing = u8"bib:id.missing";

/// @brief In a `\bib` directive,
/// the specified `id` is empty.
inline constexpr std::u8string_view id_empty = u8"bib:id.empty";

/// @brief In a `\bib` directive,
/// an attempt was made to add a duplicate entry.
inline constexpr std::u8string_view duplicate = u8"bib:duplicate";

} // namespace bib

namespace wg21_head {

/// @brief In a `\wg21-head` directive,
/// no title was specified.
inline constexpr std::u8string_view no_title = u8"wg21-head.no_title";

} // namespace wg21_head

namespace macro {

/// @brief In a `\macro` directive,
/// no pattern was provided.
inline constexpr std::u8string_view no_pattern = u8"macro:pattern.none";

/// @brief In a `\macro` directive,
/// the given pattern is not a directive.
inline constexpr std::u8string_view pattern_no_directive = u8"macro:pattern.no-directive";

/// @brief In a `\macro` directive,
/// the same macro was defined multiple times.
inline constexpr std::u8string_view redefinition = u8"macro:redefinition";

} // namespace macro

} // namespace diagnostic

} // namespace cowel

#endif
