#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

#include "cowel/util/assert.hpp"
#include "cowel/util/draft_uris.hpp"
#include "cowel/util/from_chars.hpp"
#include "cowel/util/function_ref.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/to_chars.hpp"

namespace cowel {
namespace {

std::optional<Draft_Location> match_location(std::u8string_view uri)
{
    COWEL_ASSERT(!uri.empty());

    const std::size_t part_length = std::min(uri.length(), uri.find(u8'-'));

    const auto match_prefixed
        = [&](std::u8string_view prefix, Draft_Location_Type type) -> Draft_Location {
        if (!uri.starts_with(prefix)) {
            return Draft_Location { Draft_Location_Type::section, 0, part_length };
        }
        return Draft_Location { type, prefix.length(), uri.length() - prefix.length() };
    };

    const auto match_prefixed_with_number
        = [&](std::u8string_view prefix, Draft_Location_Type type) -> Draft_Location {
        if (!uri.starts_with(prefix)) {
            return Draft_Location { Draft_Location_Type::section, 0, part_length };
        }
        auto number = std::size_t(-1);
        const auto number_string = uri.substr(prefix.length());
        const auto number_result = from_chars(number_string, number);
        if (number_result.ec != std::errc {}) {
            return Draft_Location { type, 0, prefix.length() };
        }
        const auto text_length
            = std::size_t(number_result.ptr - reinterpret_cast<const char*>(number_string.data()));
        return Draft_Location { type, prefix.length(), text_length, number };
    };

    switch (uri[0]) {
    case u8'0':
    case u8'1':
    case u8'2':
    case u8'3':
    case u8'4':
    case u8'5':
    case u8'6':
    case u8'7':
    case u8'8':
    case u8'9': {
        return match_prefixed_with_number(u8"", Draft_Location_Type::paragraph);
    }

    case u8'.': {
        return match_prefixed_with_number(u8".", Draft_Location_Type::bullet);
    }
    case u8':': {
        return match_prefixed(u8":", Draft_Location_Type::index_text);
    }
    case u8'b': {
        return match_prefixed(u8"bib:", Draft_Location_Type::bibliography);
    }
    case u8'c': {
        if (uri.starts_with(u8"conceptref:")) {
            return Draft_Location { Draft_Location_Type::concept_ref, 11, uri.length() - 11 };
        }
        return match_prefixed(u8"concept:", Draft_Location_Type::concept_);
    }
    case u8'e': {
        if (uri.starts_with(u8"eq:")) {
            return Draft_Location { Draft_Location_Type::formula, 3, uri.length() - 3 };
        }
        return match_prefixed_with_number(u8"example-", Draft_Location_Type::example);
    }
    case u8'd': {
        return match_prefixed(u8"def:", Draft_Location_Type::definition);
    }
    case u8'f': {
        return match_prefixed_with_number(u8"footnote-", Draft_Location_Type::footnote);
    }
    case u8'h': {
        if (uri.starts_with(u8"headerref:")) {
            return Draft_Location { Draft_Location_Type::header_ref, 10, uri.length() - 10 };
        }
        return match_prefixed(u8"header:", Draft_Location_Type::header);
    }
    case u8'l': {
        return match_prefixed(u8"lib:", Draft_Location_Type::library);
    }
    case u8'n': {
        if (uri.starts_with(u8"nt:")) {
            return Draft_Location { Draft_Location_Type::nonterminal, 3, uri.length() - 3 };
        }
        if (uri.starts_with(u8"ntref:")) {
            return Draft_Location { Draft_Location_Type::nonterminal_ref, 6, uri.length() - 6 };
        }
        return match_prefixed_with_number(u8"note-", Draft_Location_Type::note);
    }
    case u8'r': {
        return match_prefixed_with_number(u8"row-", Draft_Location_Type::row);
    }
    case u8's': {
        return match_prefixed_with_number(u8"sentence-", Draft_Location_Type::sentence);
    }
    default: {
        return Draft_Location { Draft_Location_Type::section, 0, part_length };
    }
    }
    COWEL_ASSERT_UNREACHABLE(u8"Switch should have returned in all cases.");
}

} // namespace

[[nodiscard]]
Result<Draft_URI_Info, Draft_URI_Error>
parse_draft_uri(std::u8string_view uri, std::span<Draft_Location> out_locations)
{
    std::size_t locations = 0;
    const std::size_t anchor_pos = uri.find(u8'#');
    if (anchor_pos == std::u8string_view::npos) {
        return Draft_URI_Info { uri.length(), locations };
    }

    uri.remove_prefix(anchor_pos);

    while (uri.length() > 1) {
        const bool has_separator = uri[0] == u8'#' || uri[0] == u8'-';
        if (has_separator) {
            uri.remove_prefix(1);
        }
        else if (uri[0] != u8'.') {
            return Draft_URI_Error::parse_fail;
        }

        std::optional<Draft_Location> location = match_location(uri);
        if (!location) {
            return Draft_URI_Error::parse_fail;
        }
        if (locations >= out_locations.size()) {
            return Draft_URI_Error::too_many_locations;
        }
        uri.remove_prefix(location->prefix_length + location->text_length);

        if (has_separator) {
            location->prefix_length += 1;
        }
        out_locations[locations++] = *location;
    }

    if (!uri.empty()) {
        return Draft_URI_Error::parse_fail;
    }

    return Draft_URI_Info { anchor_pos, locations };
}

namespace {

void verbalize_location(
    Function_Ref<void(std::u8string_view, Text_Format)> out,
    Draft_Location_Type type,
    const std::u8string_view text
)
{
    const auto output_replacing = [&](std::u8string_view str, char8_t c,
                                      std::u8string_view replacement, Text_Format part_format) {
        while (!str.empty()) {
            const std::size_t raw_length = str.find(c);
            const std::u8string_view part = str.substr(0, std::min(str.length(), raw_length));
            if (part_format == Text_Format::code) {
                // In C++ draft links, trailing underscores are used to disambiguate duplicates.
                // For example, "constructor" is the first constructor,
                // the next one is "constructor_", etc.
                // Furthermore, some names are special, like "constructor",
                // which isn't actually code but refers to the constructor of a class.
                //
                // We verbalize this by:
                //  1. Printing disambiguations like (2), (3), etc.,
                //     but only starting with the second overload, so (1) is never printed.
                //  2. Printing "constructor" with normal formatting instead of code formatting.
                const std::size_t last_non_underscore = part.find_last_not_of(u8'_');
                const std::size_t trailing_underscores
                    = last_non_underscore == std::u8string_view::npos
                    ? part.length()
                    : part.length() - last_non_underscore - 1;
                const auto part_before_trailing_underscores
                    = part.substr(0, part.length() - trailing_underscores);

                const auto special_part_format = part_before_trailing_underscores == u8"constructor"
                    ? Text_Format::none
                    : part_format;
                out(part_before_trailing_underscores, special_part_format);
                if (trailing_underscores != 0) {
                    out(u8" (", Text_Format::none);
                    out(to_characters8(trailing_underscores + 1).as_string(), Text_Format::number);
                    out(u8")", Text_Format::none);
                }
            }
            else {
                out(part, part_format);
            }
            if (raw_length == std::u8string_view::npos) {
                break;
            }
            out(replacement, Text_Format::none);
            str.remove_prefix(raw_length + 1);
        }
    };

    using enum Draft_Location_Type;
    switch (type) {
    case section: {
        out(text, Text_Format::section);
        break;
    }
    case paragraph: {
        out(u8"paragraph ", Text_Format::none);
        out(text, Text_Format::number);
        break;
    }
    case bullet: {
        out(u8"bullet ", Text_Format::none);
        out(text, Text_Format::number);
        break;
    }
    case sentence: {
        out(u8"sentence ", Text_Format::none);
        out(text, Text_Format::number);
        break;
    }
    case example: {
        out(u8"example ", Text_Format::none);
        out(text, Text_Format::number);
        break;
    }
    case footnote: {
        out(u8"footnote ", Text_Format::none);
        out(text, Text_Format::number);
        break;
    }
    case note: {
        out(u8"note ", Text_Format::none);
        out(text, Text_Format::number);
        break;
    }
    case row: {
        out(u8"row ", Text_Format::none);
        out(text, Text_Format::number);
        break;
    }
    case index_text: {
        out(u8"\"", Text_Format::none);
        output_replacing(text, u8',', u8", ", Text_Format::none);
        out(u8"\"", Text_Format::none);
        break;
    }
    case concept_: {
        out(u8"concept ", Text_Format::none);
        out(text, Text_Format::code);
        break;
    }
    case concept_ref: {
        out(u8"reference to concept ", Text_Format::none);
        out(text, Text_Format::code);
        break;
    }
    case definition: {
        out(u8"definition of \"", Text_Format::none);
        output_replacing(text, u8'_', u8" ", Text_Format::none);
        out(u8"\"", Text_Format::none);
        break;
    }
    case nonterminal: {
        out(text, Text_Format::grammar);
        break;
    }
    case nonterminal_ref: {
        out(u8"reference to ", Text_Format::none);
        out(text, Text_Format::grammar);
        break;
    }
    case formula: {
        out(u8"formula ", Text_Format::none);
        out(text, Text_Format::section);
        break;
    }
    case library: {
        output_replacing(text, u8',', u8", ", Text_Format::code);
        break;
    }
    case bibliography: {
        out(u8"bibliography ", Text_Format::none);
        out(text, Text_Format::none);
        break;
    }
    case header: {
        out(u8"header ", Text_Format::none);
        out(text, Text_Format::header);
        break;
    }
    case header_ref: {
        out(u8"reference to header ", Text_Format::none);
        out(text, Text_Format::header);
        break;
    }
    }
}

} // namespace

void verbalize_locations(
    Function_Ref<void(std::u8string_view, Text_Format)> out,
    std::span<const Draft_Location> locations,
    std::u8string_view locations_string
)
{
    bool first = true;
    for (const auto& location : locations) {
        if (!first) {
            out(u8", ", Text_Format::none);
        }
        first = false;
        locations_string.remove_prefix(location.prefix_length);
        const auto text = locations_string.substr(0, location.text_length);
        verbalize_location(out, location.type, text);
        locations_string.remove_prefix(location.text_length);
    }
}

void verbalize_draft_uri(
    Function_Ref<void(std::u8string_view, Text_Format)> out,
    std::u8string_view section,
    std::span<const Draft_Location> locations,
    std::u8string_view locations_string
)
{
    if (!section.empty()) {
        out(section, Text_Format::section);
    }
    if (!locations.empty()) {
        if (!section.empty()) {
            out(u8" ", Text_Format::none);
        }
        verbalize_locations(out, locations, locations_string);
    }
}

Result<void, Draft_URI_Error> parse_and_verbalize_draft_uri(
    Function_Ref<void(std::u8string_view, Text_Format)> out,
    std::u8string_view uri,
    std::span<Draft_Location> buffer
)
{
    const Result<Draft_URI_Info, Draft_URI_Error> r = parse_draft_uri(uri, buffer);
    if (!r) {
        return r.error();
    }
    const auto section = uri.substr(0, r->section_length);
    const auto locations_string = uri.substr(r->section_length);
    verbalize_draft_uri(out, section, buffer.subspan(0, r->locations), locations_string);
    return {};
}

} // namespace cowel
