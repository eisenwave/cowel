#ifndef COWEL_DIAGNOSTIC_HPP
#define COWEL_DIAGNOSTIC_HPP

#include <compare>
#include <string_view>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/severity.hpp"
#include "cowel/util/source_position.hpp"

#include "cowel/fwd.hpp"

namespace cowel {
namespace detail {

using Suppress_Unused_Include_Source_Position = Basic_File_Source_Position<void>;

}

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
    Char_Sequence8 id;
    /// @brief The span of code that is responsible for this diagnostic.
    File_Source_Span location;
    /// @brief The diagnostic message parts.
    Char_Sequence8 message;
};

namespace diagnostic {

// GENERAL DIAGNOSTICS =============================================================================

/// @brief A (non-fatal) error could not be produced.
inline constexpr std::u8string_view error_error = u8"error.error";

/// @brief A deprecated feature was used.
inline constexpr std::u8string_view deprecated = u8"deprecated";

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

/// @brief Duplicate arguments to a directive were provided.
inline constexpr std::u8string_view duplicate_args = u8"duplicate.args";

/// @brief The content of a directive was ignored.
inline constexpr std::u8string_view ignored_content = u8"ignored.content";

/// @brief Parse error.
inline constexpr std::u8string_view parse = u8"parse";

/// @brief An attempt was made to expand an ellipsis argument
/// outside of a macro expansion,
/// meaning it has nothing to expand to.
inline constexpr std::u8string_view ellipsis_outside = u8"ellipsis.outside";

/// @brief In syntax highlighting,
/// the given language is not supported.
inline constexpr std::u8string_view highlight_language = u8"highlight.language";
/// @brief In syntax highlighting,
/// the code could not be highlighted because it is malformed.
inline constexpr std::u8string_view highlight_malformed = u8"highlight.malformed";
/// @brief In syntax highlighting,
/// something went wrong.
inline constexpr std::u8string_view highlight_error = u8"highlight.error";

/// @brief The parsed value of a literal is too large to be represented as a value.
inline constexpr std::u8string_view literal_out_of_range = u8"literal.out-of-range";

/// @brief Unable to perform arithmetic due to a parse error.
inline constexpr std::u8string_view arithmetic_parse = u8"arithmetic.parse";
/// @brief Division by zero in arithmetic.
inline constexpr std::u8string_view arithmetic_div_by_zero = u8"arithmetic.div-by-zero";

/// @brief Attempting to reinterpret negative integer as float.
inline constexpr std::u8string_view reinterpret_out_of_range = u8"reinterpret.out-of-range";

/// @brief In a raw text directive (`\script` or `\style`),
/// an unexpected closing tag was encountered.
inline constexpr std::u8string_view raw_text_closing = u8"raw-text.closing";

/// @brief An argument type does not match a parameter type.
inline constexpr std::u8string_view type_mismatch = u8"type.mismatch";

// DIRECTIVE-SPECIFIC DIAGNOSTICS ==================================================================

/// @brief In an HTML element directive,
/// the provided tag name is invalid.
inline constexpr std::u8string_view html_element_name_invalid = u8"html.element.name.invalid";

/// @brief In a `\cowel_include` directive,
/// no file path was provided.
inline constexpr std::u8string_view file_path_missing = u8"file.path.empty";

/// @brief In a `\cowel_include` directive,
/// the file could not be loaded.
inline constexpr std::u8string_view file_io = u8"file.io";

/// @brief In a `\hl` directive,
/// no name parameter was provided.
inline constexpr std::u8string_view highlight_name_missing = u8"highlight.name.missing";
/// @brief In a `\hl` directive,
/// the given highlight name is not valid.
inline constexpr std::u8string_view highlight_name_invalid = u8"highlight.name.invalid";

/// @brief In `\cowel_char`,
/// the input is blank.
inline constexpr std::u8string_view char_blank = u8"char.blank";
/// @brief In `\cowel_char`,
/// parsing digits failed, like `\cowel_char_by_num{abc}`.
inline constexpr std::u8string_view char_digits = u8"char.digits";
/// @brief In `\cowel_char`,
/// the given name does not match any Unicode character name.
inline constexpr std::u8string_view char_name = u8"char.name";
/// @brief In `\cowel_char`,
/// a nonscalar value would be encoded.
/// @see is_scalar_value
inline constexpr std::u8string_view char_nonscalar = u8"char.nonscalar";
/// @brief In a `\cowel_char` directive,
/// the input is corrupted UTF-8 text.
inline constexpr std::u8string_view char_corrupted = u8"char.corrupted";
/// @brief The `zfill` argument could not be parsed as an integer.
inline constexpr std::u8string_view char_zfill_not_an_integer = u8"char.zfill.parse";
/// @brief The `zfill` argument could not be parsed as an integer.
inline constexpr std::u8string_view char_zfill_range = u8"char.zfill.range";
/// @brief The `base` argument could not be parsed as an integer.
inline constexpr std::u8string_view char_base_not_an_integer = u8"char.base.parse";
/// @brief The `base` argument could not be parsed as an integer.
inline constexpr std::u8string_view char_base_range = u8"char.base.range";
/// @brief The `lower` argument is neither `yes` nor `no`.
inline constexpr std::u8string_view char_lower_invalid = u8"char.lower.invalid";

/// @brief In `\cowel_invoke`,
/// the directive name is invalid.
inline constexpr std::u8string_view invoke_name_invalid = u8"invoke.name.invalid";
/// @brief In `\cowel_invoke`,
/// name lookup failed.
inline constexpr std::u8string_view invoke_lookup_failed = u8"invoke.lookup";

/// @brief In `\cowel_alias`,
/// no (target or alias) name was provided.
inline constexpr std::u8string_view alias_name_missing = u8"alias.name.missing";
/// @brief In `\cowel_alias`,
/// generation of a name failed or a name is invalid,
/// which is considered a fatal error.
inline constexpr std::u8string_view alias_name_invalid = u8"alias.name.invalid";
/// @brief In `\cowel_alias`,
/// the target was not found.
inline constexpr std::u8string_view alias_lookup = u8"alias.lookup";
/// @brief In `\cowel_alias`,
/// an attempt was made to define an alias which already exists.
inline constexpr std::u8string_view alias_duplicate = u8"alias.duplicate";

/// @brief In `\cowel_macro`,
/// no macro name was provided.
inline constexpr std::u8string_view macro_name_missing = u8"macro.name.missing";
/// @brief In `\cowel_macro`,
/// generation of a name failed or a name is invalid,
/// which is considered a fatal error.
inline constexpr std::u8string_view macro_name_invalid = u8"macro.name.invalid";
/// @brief In `\cowel_macro`,
/// an attempt was made to define a macro which already exists.
inline constexpr std::u8string_view macro_duplicate = u8"macro.duplicate";

/// @brief In `\cowel_put`,
/// the target name is invalid.
inline constexpr std::u8string_view put_invalid = u8"put.invalid";
/// @brief In `\cowel_put`,
/// the target is an integer and thus refers to a positional argument,
/// but not enough positional arguments were provided.
inline constexpr std::u8string_view put_out_of_range = u8"put.range";
/// @brief In `\cowel_put`,
/// there is no surrounding macro which expands this directive.
inline constexpr std::u8string_view put_outside = u8"put.outside";

// LEGACY DIRECTIVE DIAGNOSTICS ====================================================================

namespace code {

/// @brief In `\code`, the given `nested` parameter is not `yes` or `no`.
inline constexpr std::u8string_view nested_invalid = u8"code:nested.invalid";

} // namespace code

namespace codeblock {

/// @brief In `\codeblock`, the given `borders` parameter is not `yes` or `no`.
inline constexpr std::u8string_view borders_invalid = u8"codeblock:borders.invalid";

} // namespace codeblock

namespace h {

/// @brief In `\hN` headings, the given `listed` parameter is not `yes` or `no`.
inline constexpr std::u8string_view listed_invalid = u8"h:listed.invalid";
/// @brief In `\hN` headings, the given `show-number` parameter is not `yes` or `no`.
inline constexpr std::u8string_view show_number_invalid = u8"h:show-number.invalid";

} // namespace h

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

namespace macro {

/// @brief A `\put[...]` pseudo-directive was used outside of an argument list,
/// making expansion impossible.
inline constexpr std::u8string_view put_args_outside_args = u8"macro:put.args.outside-args";

/// @brief The content of a `\put` directive is invalid.
inline constexpr std::u8string_view put_invalid = u8"macro:put.invalid";

/// @brief The index of a positional argument was given to a `\put` directive,
/// but not enough positional arguments were provided.
inline constexpr std::u8string_view put_out_of_range = u8"macro:put.out-of-range";

} // namespace macro

namespace math {

/// @brief In a `\math` directive,
/// text was not properly enclosed in `\mi`, `\mn`, etc.
inline constexpr std::u8string_view text = u8"math:text";

} // namespace math

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
