#include <memory_resource>
#include <span>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "mmml/util/html_writer.hpp"

namespace mmml {
namespace {

[[nodiscard]]
std::u8string_view as_view(std::span<const char8_t> span)
{
    return { span.data(), span.size() };
}

struct HTML_Writer_Test : testing::Test {
    std::pmr::monotonic_buffer_resource memory;
    std::pmr::vector<char8_t> out { &memory };
    HTML_Writer writer { out };
};

TEST_F(HTML_Writer_Test, empty)
{
    constexpr std::u8string_view expected;
    EXPECT_EQ(expected, as_view(out));
}

TEST_F(HTML_Writer_Test, inner_html)
{
    constexpr std::u8string_view expected = u8"<html>Hello, world!</html>";

    writer.write_inner_html(u8"<html>Hello, world!</html>");

    EXPECT_EQ(expected, as_view(out));
}

TEST_F(HTML_Writer_Test, inner_text)
{
    constexpr std::u8string_view expected = u8"&lt;hello&amp;";

    writer.write_inner_text(u8"<hello&");

    EXPECT_EQ(expected, as_view(out));
}

TEST_F(HTML_Writer_Test, tag)
{
    constexpr std::u8string_view expected = u8"<b>Hello, world!</b>";

    writer.open_tag(u8"b");
    writer.write_inner_text(u8"Hello, world!");
    writer.close_tag(u8"b");

    EXPECT_EQ(expected, as_view(out));
}

TEST_F(HTML_Writer_Test, empty_tag)
{
    constexpr std::u8string_view expected = u8"<br/>";

    writer.write_empty_tag(u8"br");

    EXPECT_EQ(expected, as_view(out));
}

TEST_F(HTML_Writer_Test, empty_attributes)
{
    constexpr std::u8string_view expected = u8"<x a b=\"\" c></x>";

    writer.open_tag_with_attributes(u8"x")
        .write_empty_attribute(u8"a", Attribute_Style::double_if_needed)
        .write_empty_attribute(u8"b", Attribute_Style::always_double)
        .write_empty_attribute(u8"c", Attribute_Style::double_if_needed)
        .end();
    writer.close_tag(u8"x");

    EXPECT_EQ(expected, as_view(out));
}

TEST_F(HTML_Writer_Test, attributes_with_values_quotes_if_needed)
{
    constexpr std::u8string_view expected = u8"<x id=name class='a b' hidden></x>";

    writer.open_tag_with_attributes(u8"x")
        .write_attribute(u8"id", u8"name", Attribute_Style::single_if_needed)
        .write_attribute(u8"class", u8"a b", Attribute_Style::single_if_needed)
        .write_attribute(u8"hidden", u8"", Attribute_Style::single_if_needed)
        .end();
    writer.close_tag(u8"x");

    EXPECT_EQ(expected, as_view(out));
}

TEST_F(HTML_Writer_Test, attributes_with_values_always_quotes)
{
    constexpr std::u8string_view expected = u8"<x id='name' class='a b' hidden=''></x>";

    writer.open_tag_with_attributes(u8"x")
        .write_attribute(u8"id", u8"name", Attribute_Style::always_single)
        .write_attribute(u8"class", u8"a b", Attribute_Style::always_single)
        .write_attribute(u8"hidden", u8"", Attribute_Style::always_single)
        .end();
    writer.close_tag(u8"x");

    EXPECT_EQ(expected, as_view(out));
}

TEST_F(HTML_Writer_Test, attributes_but_empty)
{
    constexpr std::u8string_view expected = u8"<br/>";

    writer.open_tag_with_attributes(u8"br").end_empty();

    EXPECT_EQ(expected, as_view(out));
}

TEST_F(HTML_Writer_Test, attributes_escape)
{
    constexpr std::u8string_view expected = u8"<x id='&apos;'/>";

    writer.open_tag_with_attributes(u8"x")
        .write_attribute(u8"id", u8"'", Attribute_Style::single_if_needed)
        .end_empty();

    EXPECT_EQ(expected, as_view(out));
}

} // namespace
} // namespace mmml
