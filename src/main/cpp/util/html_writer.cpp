#include <algorithm>
#include <cstddef>
#include <string_view>
#include <vector>

#include "mmml/util/assert.hpp"
#include "mmml/util/chars.hpp"
#include "mmml/util/html_writer.hpp"
#include "mmml/util/strings.hpp"
#include "mmml/util/unicode.hpp"

namespace mmml {

namespace {

[[nodiscard]]
std::u8string_view html_entity_of(char8_t c)
{
    switch (c) {
    case u8'&': return u8"&amp;";
    case u8'<': return u8"&lt;";
    case u8'>': return u8"&gt;";
    case u8'\'': return u8"&apos;";
    case u8'"': return u8"&quot;";
    default: MMML_ASSERT_UNREACHABLE(u8"We only support a handful of characters.");
    }
}

[[nodiscard]]
std::u8string_view html_entity_of(char32_t c)
{
    MMML_DEBUG_ASSERT(is_ascii(c));
    return html_entity_of(char8_t(c));
}

} // namespace

void append(std::pmr::vector<char8_t>& out, std::u8string_view text)
{
    out.insert(out.end(), text.data(), text.data() + text.size());
}

void append_html_escaped(
    std::pmr::vector<char8_t>& out,
    std::u8string_view text,
    std::u8string_view charset
)
{
    while (!text.empty()) {
        const std::size_t bracket_pos = text.find_first_of(charset);
        const auto snippet = text.substr(0, std::min(text.length(), bracket_pos));
        append(out, snippet);
        if (bracket_pos == std::string_view::npos) {
            break;
        }
        append(out, html_entity_of(text[bracket_pos]));
        text = text.substr(bracket_pos + 1);
    }
}

void HTML_Writer::do_write(char_type c)
{
    m_out.push_back(c);
}

void HTML_Writer::do_write(string_view_type str)
{
    append(m_out, str);
}

void HTML_Writer::write_inner_text(string_view_type text)
{
    MMML_ASSERT(!m_in_attributes);
    append_html_escaped(m_out, text, u8"&<>");
}

void HTML_Writer::write_inner_text(char32_t c)
{
    MMML_DEBUG_ASSERT(!m_in_attributes);
    MMML_DEBUG_ASSERT(is_scalar_value(c));
    append(
        m_out,
        is_html_min_raw_passthrough_character(c) ? utf8::encode8_unchecked(c).as_string()
                                                 : html_entity_of(c)
    );
}

void HTML_Writer::write_inner_text(std::u32string_view text)
{
    MMML_ASSERT(!m_in_attributes);
    for (const char32_t c : text) {
        write_inner_text(c);
    }
}

void HTML_Writer::write_inner_html(char32_t c)
{
    MMML_DEBUG_ASSERT(!m_in_attributes);
    MMML_DEBUG_ASSERT(is_scalar_value(c));
    append(m_out, utf8::encode8_unchecked(c).as_string());
}

void HTML_Writer::write_inner_html(std::u8string_view text)
{
    MMML_ASSERT(!m_in_attributes);
    do_write(text);
}

void HTML_Writer::write_inner_html(std::u32string_view text)
{
    MMML_ASSERT(!m_in_attributes);
    for (const char32_t c : text) {
        write_inner_html(c);
    }
}

HTML_Writer& HTML_Writer::write_preamble()
{
    MMML_ASSERT(!m_in_attributes);
    do_write(u8"<!DOCTYPE html>\n");
    return *this;
}

HTML_Writer& HTML_Writer::write_self_closing_tag(string_view_type id)
{
    MMML_ASSERT(!m_in_attributes);
    MMML_ASSERT(is_html_tag_name(id));

    do_write(u8'<');
    do_write(id);
    do_write(u8"/>");

    return *this;
}

HTML_Writer& HTML_Writer::open_tag(string_view_type id)
{
    MMML_ASSERT(!m_in_attributes);
    MMML_ASSERT(is_html_tag_name(id));

    do_write(u8'<');
    do_write(id);
    do_write(u8'>');
    ++m_depth;

    return *this;
}

HTML_Writer& HTML_Writer::open_and_close_tag(string_view_type id)
{
    MMML_ASSERT(!m_in_attributes);
    MMML_ASSERT(is_html_tag_name(id));

    do_write(u8'<');
    do_write(id);
    do_write(u8"></");
    do_write(id);
    do_write(u8'>');

    return *this;
}

Attribute_Writer HTML_Writer::open_tag_with_attributes(string_view_type id)
{
    MMML_ASSERT(!m_in_attributes);
    MMML_ASSERT(is_html_tag_name(id));

    do_write(u8'<');
    do_write(id);

    return Attribute_Writer { *this };
}

HTML_Writer& HTML_Writer::close_tag(string_view_type id)
{
    MMML_ASSERT(!m_in_attributes);
    MMML_ASSERT(is_html_tag_name(id));
    MMML_ASSERT(m_depth != 0);

    --m_depth;

    do_write(u8"</");
    do_write(id);
    do_write(u8'>');

    return *this;
}

HTML_Writer& HTML_Writer::write_comment(string_view_type comment)
{
    do_write(u8"<!--");
    append_html_escaped(m_out, comment, u8"<>");
    do_write(u8"-->");
    return *this;
}

HTML_Writer&
HTML_Writer::write_attribute(string_view_type key, string_view_type value, Attribute_Style style)
{
    if (value.empty()) {
        return write_empty_attribute(key, style);
    }

    MMML_ASSERT(m_in_attributes);
    MMML_ASSERT(is_html_attribute_name(key));

    do_write(u8' ');
    do_write(key);

    const char8_t quote_char = attribute_style_quote_char(style);
    do_write(u8'=');
    if (!attribute_style_demands_quotes(style) && is_html_unquoted_attribute_value(value)) {
        do_write(value);
    }
    else {
        do_write(quote_char);
        append_html_escaped(m_out, value, u8"\"'");
        do_write(quote_char);
    }

    return *this;
}

HTML_Writer& HTML_Writer::write_empty_attribute(string_view_type key, Attribute_Style style)
{
    MMML_ASSERT(m_in_attributes);
    MMML_ASSERT(is_html_attribute_name(key));

    do_write(u8' ');
    do_write(key);

    switch (style) {
    case Attribute_Style::always_double: do_write(u8"=\"\""); break;
    case Attribute_Style::always_single: do_write(u8"=''"); break;
    default: break;
    }

    return *this;
}

HTML_Writer& HTML_Writer::end_attributes()
{
    MMML_ASSERT(m_in_attributes);

    do_write(u8'>');
    m_in_attributes = false;
    ++m_depth;

    return *this;
}

HTML_Writer& HTML_Writer::end_empty_tag_attributes()
{
    MMML_ASSERT(m_in_attributes);

    do_write(u8"/>");
    m_in_attributes = false;

    return *this;
}

} // namespace mmml
