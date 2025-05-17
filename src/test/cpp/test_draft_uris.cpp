#include <iostream>
#include <string_view>

#include <gtest/gtest.h>

#include "cowel/util/draft_uris.hpp"
#include "cowel/util/result.hpp"

namespace cowel {

ULIGHT_SUPPRESS_MISSING_DECLARATIONS_WARNING()

std::ostream&
operator<<(std::ostream& out, Draft_URI_Info info) // NOLINT(misc-use-internal-linkage)
{
    return out << "Draft_URI_Info{.section_length = " << info.section_length
               << ", .locations = " << info.locations << "}";
}

std::ostream&
operator<<(std::ostream& out, Draft_Location_Type location) // NOLINT(misc-use-internal-linkage)
{
    return out << int(location);
}

std::ostream&
operator<<(std::ostream& out, Draft_Location location) // NOLINT(misc-use-internal-linkage)
{
    out << "Draft_Location{.type = " << location.type //
        << ", .prefix_length = " << location.prefix_length //
        << ", .text_length = " << location.text_length;
    if (location.number != std::size_t(-1)) {
        out << ", .number = " << location.number;
    }
    return out << "}";
}

namespace {

using namespace std::literals;

TEST(Draft_URIs, no_anchor)
{
    Draft_Location buffer[1];
    constexpr std::u8string_view simple = u8"defns";

    const Result<Draft_URI_Info, Draft_URI_Error> simple_result = parse_draft_uri(simple, buffer);
    ASSERT_TRUE(simple_result);
    ASSERT_EQ(*simple_result, Draft_URI_Info(simple.length(), 0));

    constexpr std::u8string_view sections = u8"defns.undefined";
    const Result<Draft_URI_Info, Draft_URI_Error> sections_result
        = parse_draft_uri(sections, buffer);
    ASSERT_EQ(*sections_result, Draft_URI_Info(sections.length(), 0));
}

TEST(Draft_URIs, single_part)
{
    Draft_Location buffer[1];

    constexpr struct {
        std::u8string_view input;
        Draft_Location location;
    } tests[] {
        { u8"x#section", { Draft_Location_Type::section, 1, u8"section"sv.length() } },
        { u8"x#a.b.c", { Draft_Location_Type::section, 1, u8"a.b.c"sv.length() } },
        { u8"x#123", { Draft_Location_Type::paragraph, 1, u8"123"sv.length(), 123 } },
        { u8"x#.123", { Draft_Location_Type::bullet, 2, u8"123"sv.length(), 123 } },
        { u8"x#sentence-123", { Draft_Location_Type::sentence, 10, u8"123"sv.length(), 123 } },
        { u8"x#example-123", { Draft_Location_Type::example, 9, u8"123"sv.length(), 123 } },
        { u8"x#footnote-123", { Draft_Location_Type::footnote, 10, u8"123"sv.length(), 123 } },
        { u8"x#note-123", { Draft_Location_Type::note, 6, u8"123"sv.length(), 123 } },
        { u8"x#row-123", { Draft_Location_Type::row, 5, u8"123"sv.length(), 123 } },
        { u8"x#:x,y", { Draft_Location_Type::index_text, 2, u8"x,y"sv.length() } },
        { u8"x#concept:t", { Draft_Location_Type::concept_, 9, u8"t"sv.length() } },
        { u8"x#conceptref:t", { Draft_Location_Type::concept_ref, 12, u8"t"sv.length() } },
        { u8"x#def:object", { Draft_Location_Type::definition, 5, u8"object"sv.length() } },
        { u8"x#nt:expr", { Draft_Location_Type::nonterminal, 4, u8"expr"sv.length() } },
        { u8"x#ntref:expr", { Draft_Location_Type::nonterminal_ref, 7, u8"expr"sv.length() } },
        { u8"x#eq:x.y.z", { Draft_Location_Type::formula, 4, u8"x.y.z"sv.length() } },
        { u8"x#lib:malloc", { Draft_Location_Type::library, 5, u8"malloc"sv.length() } },
        { u8"x#lib:a,b_", { Draft_Location_Type::library, 5, u8"a,b_"sv.length() } },
        { u8"x#bib:iso1234", { Draft_Location_Type::bibliography, 5, u8"iso1234"sv.length() } },
        { u8"x#header:<x>", { Draft_Location_Type::header, 8, u8"<x>"sv.length() } },
        { u8"x#headerref:<x>", { Draft_Location_Type::header_ref, 11, u8"<x>"sv.length() } },
    };

    for (const auto& test : tests) {
        const Result<Draft_URI_Info, Draft_URI_Error> r = parse_draft_uri(test.input, buffer);
        ASSERT_TRUE(r);
        ASSERT_EQ(*r, Draft_URI_Info(1, 1));
        ASSERT_EQ(buffer[0], test.location);
    }
}

TEST(Draft_URIs, multi_part)
{
    static constexpr std::array<Draft_Location, 3> expected { {
        { Draft_Location_Type::paragraph, 1, 2, 15 },
        { Draft_Location_Type::bullet, 1, 3, 188 },
        { Draft_Location_Type::sentence, 10, 3, 100 },
    } };
    static constexpr std::u8string_view input = u8"ab.cd#15.188-sentence-100";
    std::array<Draft_Location, 3> actual;

    const Result<Draft_URI_Info, Draft_URI_Error> r = parse_draft_uri(input, actual);
    ASSERT_TRUE(r);
    ASSERT_EQ(*r, Draft_URI_Info(5, 3));
    ASSERT_EQ(expected, actual);
}

TEST(Draft_URIs, verbalize)
{
    Draft_Location buffer[4];

    std::u8string verbalized;

    auto inserter = [&](std::u8string_view part, Text_Format format) {
        if (format == Text_Format::section) {
            verbalized += u8'[';
        }
        verbalized += part;
        if (format == Text_Format::section) {
            verbalized += u8']';
        }
    };

    static constexpr std::u8string_view input = u8"ab.cd#15.188-sentence-100";

    const Result<void, Draft_URI_Error> result
        = parse_and_verbalize_draft_uri(inserter, input, buffer);
    ASSERT_TRUE(result);
}

} // namespace
} // namespace cowel
