#include "mmml/util/html_writer.hpp"
#include "mmml/util/annotated_string.hpp"
#include "mmml/util/source_position.hpp"
#include "mmml/util/strings.hpp"
#include "mmml/util/unicode.hpp"

namespace mmml {

void append(std::pmr::vector<char8_t>& out, std::u8string_view text)
{
    out.insert(out.end(), text.data(), text.data() + text.size());
}

void append_html_escaped(std::pmr::vector<char8_t>& out, std::u8string_view text)
{
    while (!text.empty()) {
        const std::size_t bracket_pos = text.find_first_of(u8"&<>");
        const auto snippet = text.substr(0, std::min(text.length(), bracket_pos));
        append(out, snippet);
        if (bracket_pos == std::string_view::npos) {
            break;
        }
        else if (text[bracket_pos] == u8'&') {
            append(out, u8"&amp;");
        }
        else if (text[bracket_pos] == u8'<') {
            append(out, u8"&lt;");
        }
        else if (text[bracket_pos] == u8'>') {
            append(out, u8"&gt;");
        }
        else {
            MMML_ASSERT_UNREACHABLE(u8"Logical mistake.");
        }

        text = text.substr(bracket_pos + 1);
    }
}

HTML_Writer::HTML_Writer(std::pmr::vector<char_type>& out)
    : m_out(out)
{
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
    append_html_escaped(m_out, text);
}

void HTML_Writer::write_inner_html(string_view_type text)
{
    MMML_ASSERT(!m_in_attributes);
    do_write(text);
}

HTML_Writer& HTML_Writer::write_preamble()
{
    MMML_ASSERT(!m_in_attributes);

    do_write(u8"<!");
    do_write(u8"DOCTYPE html");
    do_write(u8">");
    do_write('\n');

    return *this;
}

HTML_Writer& HTML_Writer::write_empty_tag(string_view_type id)
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
    append_html_escaped(m_out, comment);
    do_write(u8"-->");
    return *this;
}

HTML_Writer& HTML_Writer::write_attribute(string_view_type key, string_view_type value)
{
    MMML_ASSERT(m_in_attributes);
    MMML_ASSERT(is_html_attribute_name(key));

    do_write(u8' ');
    do_write(key);

    if (!value.empty()) {
        do_write(u8'=');
        if (requires_quotes_in_html_attribute(value)) {
            do_write(u8'"');
            do_write(value);
            do_write(u8'"');
        }
        else {
            do_write(value);
        }
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
