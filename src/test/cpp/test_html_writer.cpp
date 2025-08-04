#include <array>
#include <cstddef>
#include <memory_resource>
#include <span>
#include <string_view>

#include <gtest/gtest.h>

#include "cowel/util/html_entities.hpp"
#include "cowel/util/html_writer.hpp"

#include "cowel/policy/capture.hpp"

#include "cowel/output_language.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

[[nodiscard]]
std::u8string_view as_view(std::span<const char8_t> span)
{
    return { span.data(), span.size() };
}

struct HTML_Writer_Test : testing::Test {
    std::pmr::monotonic_buffer_resource memory;
    Vector_Text_Sink out { Output_Language::html, &memory };
    HTML_Writer writer { out };
};

TEST_F(HTML_Writer_Test, empty)
{
    constexpr std::u8string_view expected;
    EXPECT_EQ(expected, as_view(*out));
}

TEST_F(HTML_Writer_Test, inner_html)
{
    constexpr std::u8string_view expected = u8"<html>Hello, world!</html>"sv;

    writer.write_inner_html(u8"<html>Hello, world!</html>"sv);

    EXPECT_EQ(expected, as_view(*out));
}

TEST_F(HTML_Writer_Test, inner_text)
{
    constexpr std::u8string_view expected = u8"&lt;hello&amp;"sv;

    writer.write_inner_text(u8"<hello&"sv);

    EXPECT_EQ(expected, as_view(*out));
}

TEST_F(HTML_Writer_Test, tag)
{
    constexpr std::u8string_view expected = u8"<b>Hello, world!</b>"sv;

    writer.open_tag(html_tag::b);
    writer.write_inner_text(u8"Hello, world!"sv);
    writer.close_tag(html_tag::b);

    EXPECT_EQ(expected, as_view(*out));
}

TEST_F(HTML_Writer_Test, empty_tag)
{
    constexpr std::u8string_view expected = u8"<br/>"sv;

    writer.write_self_closing_tag(html_tag::br);

    EXPECT_EQ(expected, as_view(*out));
}

TEST_F(HTML_Writer_Test, empty_attributes)
{
    constexpr std::u8string_view expected = u8"<x a b=\"\" c></x>"sv;
    constexpr HTML_Tag_Name tag { u8"x" };

    writer.open_tag_with_attributes(tag)
        .write_empty_attribute(HTML_Attribute_Name(u8"a"sv), Attribute_Style::double_if_needed)
        .write_empty_attribute(HTML_Attribute_Name(u8"b"sv), Attribute_Style::always_double)
        .write_empty_attribute(HTML_Attribute_Name(u8"c"sv), Attribute_Style::double_if_needed)
        .end();
    writer.close_tag(tag);

    EXPECT_EQ(expected, as_view(*out));
}

TEST_F(HTML_Writer_Test, attributes_with_values_quotes_if_needed)
{
    constexpr std::u8string_view expected = u8"<x id=name class='a b' hidden></x>"sv;
    constexpr HTML_Tag_Name tag { u8"x" };

    writer.open_tag_with_attributes(tag)
        .write_id(u8"name"sv, Attribute_Style::single_if_needed)
        .write_class(u8"a b"sv, Attribute_Style::single_if_needed)
        .write_attribute(html_attr::hidden, u8""sv, Attribute_Style::single_if_needed)
        .end();
    writer.close_tag(tag);

    EXPECT_EQ(expected, as_view(*out));
}

TEST_F(HTML_Writer_Test, attributes_with_values_always_quotes)
{
    constexpr std::u8string_view expected = u8"<x id='name' class='a b' hidden=''></x>"sv;
    constexpr HTML_Tag_Name tag { u8"x" };

    writer.open_tag_with_attributes(tag)
        .write_id(u8"name"sv, Attribute_Style::always_single)
        .write_class(u8"a b"sv, Attribute_Style::always_single)
        .write_attribute(html_attr::hidden, u8""sv, Attribute_Style::always_single)
        .end();
    writer.close_tag(tag);

    EXPECT_EQ(expected, as_view(*out));
}

TEST_F(HTML_Writer_Test, attributes_but_empty)
{
    constexpr std::u8string_view expected = u8"<br/>"sv;

    writer.open_tag_with_attributes(html_tag::br).end_empty();

    EXPECT_EQ(expected, as_view(*out));
}

TEST_F(HTML_Writer_Test, attributes_escape)
{
    constexpr std::u8string_view expected = u8"<x id='&apos;'/>"sv;

    writer.open_tag_with_attributes(HTML_Tag_Name(u8"x"sv))
        .write_id(u8"'"sv, Attribute_Style::single_if_needed)
        .end_empty();

    EXPECT_EQ(expected, as_view(*out));
}

TEST(HTML_Entities, empty)
{
    constexpr std::array<char32_t, 2> expected {};
    const std::array<char32_t, 2> actual = code_points_by_character_reference_name(u8""sv);

    EXPECT_EQ(expected, actual);
}

TEST(HTML_Entities, amp)
{
    constexpr std::array<char32_t, 2> expected { U'&' };
    const std::array<char32_t, 2> actual = code_points_by_character_reference_name(u8"amp"sv);

    EXPECT_EQ(expected, actual);
}

TEST(HTML_Entities, bne)
{
    constexpr std::array<char32_t, 2> expected { U'\u003D', U'\u20E5' };
    const std::array<char32_t, 2> actual = code_points_by_character_reference_name(u8"bne"sv);

    EXPECT_EQ(expected, actual);
}

TEST(HTML_Entities, all_found)
{
    constexpr std::array<char32_t, 2> unexpected {};

    for (const std::u8string_view name : html_character_names) {
        const std::array<char32_t, 2> result = code_points_by_character_reference_name(name);
        EXPECT_NE(result, unexpected);
        const std::size_t length = result[1] != 0 ? 2uz : 1uz;
        const std::u32string_view other_string { result.data(), length };
        const std::u32string_view result_string = string_by_character_reference_name(name);
        EXPECT_EQ(result_string, other_string);
    }
}

} // namespace
} // namespace cowel
