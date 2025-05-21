#include <algorithm>
#include <cstddef>
#include <string_view>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/chars.hpp"
#include "cowel/util/html_writer.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/unicode.hpp"
#include "cowel/util/url_encode.hpp"

namespace cowel {

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
    default: COWEL_ASSERT_UNREACHABLE(u8"We only support a handful of characters.");
    }
}

[[nodiscard]]
std::u8string_view html_entity_of(char32_t c)
{
    COWEL_DEBUG_ASSERT(is_ascii(c));
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
    COWEL_ASSERT(!m_in_attributes);
    append_html_escaped(m_out, text, u8"&<>");
}

void HTML_Writer::write_inner_text(char32_t c)
{
    COWEL_DEBUG_ASSERT(!m_in_attributes);
    COWEL_DEBUG_ASSERT(is_scalar_value(c));
    append(
        m_out,
        is_html_min_raw_passthrough_character(c) ? utf8::encode8_unchecked(c).as_string()
                                                 : html_entity_of(c)
    );
}

void HTML_Writer::write_inner_text(std::u32string_view text)
{
    COWEL_ASSERT(!m_in_attributes);
    for (const char32_t c : text) {
        write_inner_text(c);
    }
}

void HTML_Writer::write_inner_html(char32_t c)
{
    COWEL_DEBUG_ASSERT(!m_in_attributes);
    COWEL_DEBUG_ASSERT(is_scalar_value(c));
    append(m_out, utf8::encode8_unchecked(c).as_string());
}

void HTML_Writer::write_inner_html(std::u8string_view text)
{
    COWEL_ASSERT(!m_in_attributes);
    do_write(text);
}

void HTML_Writer::write_inner_html(std::u32string_view text)
{
    COWEL_ASSERT(!m_in_attributes);
    for (const char32_t c : text) {
        write_inner_html(c);
    }
}

HTML_Writer& HTML_Writer::write_preamble()
{
    COWEL_ASSERT(!m_in_attributes);
    do_write(u8"<!DOCTYPE html>\n");
    return *this;
}

HTML_Writer& HTML_Writer::write_self_closing_tag(string_view_type id)
{
    COWEL_ASSERT(!m_in_attributes);
    COWEL_ASSERT(is_html_tag_name(id));

    do_write(u8'<');
    do_write(id);
    do_write(u8"/>");

    return *this;
}

HTML_Writer& HTML_Writer::open_tag(string_view_type id)
{
    COWEL_ASSERT(!m_in_attributes);
    COWEL_ASSERT(is_html_tag_name(id));

    do_write(u8'<');
    do_write(id);
    do_write(u8'>');
    ++m_depth;

    return *this;
}

HTML_Writer& HTML_Writer::open_and_close_tag(string_view_type id)
{
    COWEL_ASSERT(!m_in_attributes);
    COWEL_ASSERT(is_html_tag_name(id));

    do_write(u8'<');
    do_write(id);
    do_write(u8"></");
    do_write(id);
    do_write(u8'>');

    return *this;
}

Attribute_Writer HTML_Writer::open_tag_with_attributes(string_view_type id)
{
    COWEL_ASSERT(!m_in_attributes);
    COWEL_ASSERT(is_html_tag_name(id));

    do_write(u8'<');
    do_write(id);

    return Attribute_Writer { *this };
}

HTML_Writer& HTML_Writer::close_tag(string_view_type id)
{
    COWEL_ASSERT(!m_in_attributes);
    COWEL_ASSERT(is_html_tag_name(id));
    COWEL_ASSERT(m_depth != 0);

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

HTML_Writer& HTML_Writer::write_attribute(
    string_view_type key,
    std::span<const string_view_type> value_parts,
    Attribute_Style style,
    Attribute_Encoding encoding
)
{
    if (std::ranges::all_of(value_parts, [](std::u8string_view s) { return s.empty(); })) {
        return write_empty_attribute(key, style);
    }

    COWEL_ASSERT(m_in_attributes);
    COWEL_ASSERT(is_html_attribute_name(key));

    do_write(u8' ');
    do_write(key);

    const char8_t quote_char = attribute_style_quote_char(style);
    do_write(u8'=');

    const bool omit_quotes = !attribute_style_demands_quotes(style)
        && std::ranges::all_of(value_parts, [](std::u8string_view s) {
               return is_html_unquoted_attribute_value(s);
           });

    if (omit_quotes) {
        for (const std::u8string_view part : value_parts) {
            switch (encoding) {
            case Attribute_Encoding::text: {
                do_write(part);
                break;
            }
            case Attribute_Encoding::url: {
                url_encode_ascii_if(std::back_inserter(m_out), part, [](char8_t c) {
                    return is_url_always_encoded(c);
                });
                break;
            }
            }
        }
    }
    else {
        do_write(quote_char);
        for (const std::u8string_view part : value_parts) {
            switch (encoding) {
            case Attribute_Encoding::text: {
                append_html_escaped(m_out, part, u8"\"'");
                break;
            }
            case Attribute_Encoding::url: {
                url_encode_ascii_if(std::back_inserter(m_out), part, [](char8_t c) {
                    static_assert(is_url_always_encoded(u8'"'));
                    static_assert(!is_url_always_encoded(u8'\''));
                    return c == u8'\'' || is_url_always_encoded(c);
                });
                break;
            }
            }
        }
        do_write(quote_char);
    }

    return *this;
}

HTML_Writer& HTML_Writer::write_empty_attribute(string_view_type key, Attribute_Style style)
{
    COWEL_ASSERT(m_in_attributes);
    COWEL_ASSERT(is_html_attribute_name(key));

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
    COWEL_ASSERT(m_in_attributes);

    do_write(u8'>');
    m_in_attributes = false;
    ++m_depth;

    return *this;
}

HTML_Writer& HTML_Writer::end_empty_tag_attributes()
{
    COWEL_ASSERT(m_in_attributes);

    do_write(u8"/>");
    m_in_attributes = false;

    return *this;
}

} // namespace cowel
