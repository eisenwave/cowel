#ifndef COWEL_DRAFT_URIS_HPP
#define COWEL_DRAFT_URIS_HPP

#include <cstddef>
#include <span>
#include <string_view>

#include "cowel/util/function_ref.hpp"
#include "cowel/util/result.hpp"

#include "cowel/fwd.hpp"

namespace cowel {

// https://github.com/Eelis/cxxdraft-htmlgen/blob/eee5307c45c04761ef5c27956cbed742e08e5fb4/Render.hs#L147
enum struct Draft_Location_Type : Default_Underlying {
    /// @brief A section, like `embed`.
    section,
    /// @brief A paragraph, which just consists of an integer.
    paragraph,
    /// @brief A bullet within a paragraph,
    /// which is prefixed with a `.` and consists of the bullet number.
    bullet,
    /// @brief A sentence within a paragraph or bullet, like `sentence-1`.
    sentence,
    /// @brief An example, like `example-1`.
    example,
    /// @brief A footnote, like `footnote-1`.
    footnote,
    /// @brief A note, like `note-1`.
    note,
    /// @brief A row in a table, like `row-1`.
    row,
    /// @brief An indexed location, like `:destroy,object`.
    index_text,
    /// @brief A concept, like `concept:iterator`.
    concept_,
    /// @brief A reference to a concept, like `conceptref:iterator`.
    concept_ref,
    /// @brief A definition, like `def:object`.
    definition,
    /// @brief A nonterminal within a grammar, like `nt:expression`.
    nonterminal,
    /// @brief A reference to a nonterminal within a grammar, like `ntref:expression`.
    nonterminal_ref,
    /// @brief A formula, like `eq:sf.cmath.hermite`.
    formula,
    /// @brief A library index entry, like `lib:vector,constructor`.
    library,
    /// @brief A bibliography index entry, like `bib:iso4217`.
    bibliography,
    /// @brief A header, like `header:<cmath>`.
    header,
    /// @brief A reference to a header, like `headerref:<cmath>`.
    header_ref,
};

struct Draft_Location {
    /// @brief The type of location.
    Draft_Location_Type type;
    /// @brief The length of the separator character as well as disambiguating prefixes
    /// like `lib:` or `def:`.
    std::size_t prefix_length;
    /// @brief The length of the location in text,
    /// not including the leading separator character,
    /// and not including prefixes like `lib:` or `def:`.
    ///
    /// `prefix_length` and `text_length` sum up to the total length of the location,
    /// and the sum of all location lengths plus the length of the main `section_length`
    /// sum up to the total URI length.
    std::size_t text_length;
    /// @brief For paragraphs, sentences, bullets, and other numbered locations,
    /// the number of that location.
    std::size_t number = std::size_t(-1);

    [[nodiscard]]
    friend constexpr bool operator==(const Draft_Location&, const Draft_Location&)
        = default;
};

enum struct Draft_URI_Error : Default_Underlying {
    /// @brief General parse error.
    parse_fail,
    /// @brief More locations in the URI than the provided buffer can hold.
    too_many_locations,
};

struct Draft_URI_Info {
    /// @brief The length of the section preceding the `#` anchor separator.
    /// This can be zero, such as in `https://eel.is/c++draft/#basic`.
    std::size_t section_length;
    /// @brief The amount of locations written to `out_locations`.
    std::size_t locations;

    [[nodiscard]]
    friend constexpr bool operator==(Draft_URI_Info, Draft_URI_Info)
        = default;
};

[[nodiscard]]
Result<Draft_URI_Info, Draft_URI_Error>
parse_draft_uri(std::u8string_view uri, std::span<Draft_Location> out_locations);

enum struct Text_Format : Default_Underlying {
    none,
    number,
    section,
    code,
    grammar,
    header,
};

void verbalize_locations(
    Function_Ref<void(std::u8string_view, Text_Format)> out,
    std::span<const Draft_Location> locations,
    std::u8string_view locations_string
);

/// @brief Converts a parsed draft URI to a human-readable format.
/// For example, if the URI originally contained `expr#header:<abc>`,
/// `out` is invoked like
/// `out(u8"expr", section)`,
/// `out(u8", ", none)`,
/// `out(u8"header ", none)`,
/// `out(u8"<abc>", header)`.
///
/// This approach is entirely non-allocating.
/// If the caller wants to build a single string out of the results,
/// they can provide an `out` that pushes back to a `std::vector`, for example.
/// @param out The callback which is invoked repeatedly with the parts of the URI.
/// @param section The (possibly empty) section string,
/// which is a substring of the URI before `#`.
/// @param locations A (possibly empty) span of locations within the section.
/// @param locations_string The string representation of the locations,
/// which is a substring of the URI starting with `#`.
void verbalize_draft_uri(
    Function_Ref<void(std::u8string_view, Text_Format)> out,
    std::u8string_view section,
    std::span<const Draft_Location> locations,
    std::u8string_view locations_string
);

/// @brief Parses `uri` using `parse_draft_uri(uri, buffer)`,
/// and upon success,
/// formats the result using `verbalize_draft_uri`.
[[nodiscard]]
Result<void, Draft_URI_Error> parse_and_verbalize_draft_uri(
    Function_Ref<void(std::u8string_view, Text_Format)> out,
    std::u8string_view uri,
    std::span<Draft_Location> buffer
);

} // namespace cowel

#endif
