#include "mmml/html_writer.hpp"
#include "mmml/annotated_string.hpp"
#include "mmml/parse_utils.hpp"
#include "mmml/source_position.hpp"

namespace mmml {
namespace {

void append_escaped_text(Annotated_String::Scoped_Builder& builder, std::string_view text)
{
    while (!text.empty()) {
        const std::size_t bracket_pos = text.find_first_of("<>");
        const std::string_view snippet = text.substr(0, std::min(text.length(), bracket_pos));
        builder.append(snippet);
        if (bracket_pos == std::string_view::npos) {
            break;
        }
        else if (text[bracket_pos] == '<') {
            builder.append("&lt;");
        }
        else if (text[bracket_pos] == '>') {
            builder.append("&gt;");
        }
        else {
            MMML_ASSERT_UNREACHABLE("Logical mistake.");
        }

        text = text.substr(bracket_pos + 1);
    }
}

} // namespace

HTML_Writer::HTML_Writer(Annotated_String& out)
    : m_out(out)
{
}

void HTML_Writer::write_inner_text(std::string_view text)
{
    MMML_ASSERT(!m_in_attributes);

    auto builder = m_out.build(Annotation_Type::html_inner_text);
    append_escaped_text(builder, text);
}

void HTML_Writer::write_inner_html(std::string_view text)
{
    MMML_ASSERT(!m_in_attributes);
    m_out.append(text, Annotation_Type::html_inner_text);
}

HTML_Writer& HTML_Writer::write_preamble()
{
    MMML_ASSERT(!m_in_attributes);

    m_out.append("<!", Annotation_Type::html_tag_bracket);
    m_out.append("DOCTYPE html", Annotation_Type::html_preamble);
    m_out.append(">", Annotation_Type::html_tag_bracket);
    m_out.append('\n');

    return *this;
}

HTML_Writer& HTML_Writer::write_empty_tag(std::string_view id)
{
    MMML_ASSERT(!m_in_attributes);
    MMML_ASSERT(is_html_identifier(id));

    m_out.append('<', Annotation_Type::html_tag_bracket);
    m_out.append(id, Annotation_Type::html_tag_identifier);
    m_out.append("/>", Annotation_Type::html_tag_bracket);

    return *this;
}

HTML_Writer& HTML_Writer::open_tag(std::string_view id)
{
    MMML_ASSERT(!m_in_attributes);
    MMML_ASSERT(is_html_identifier(id));

    m_out.append('<', Annotation_Type::html_tag_bracket);
    m_out.append(id, Annotation_Type::html_tag_identifier);
    m_out.append('>', Annotation_Type::html_tag_bracket);
    ++m_depth;

    return *this;
}

Attribute_Writer HTML_Writer::open_tag_with_attributes(std::string_view id)
{
    MMML_ASSERT(!m_in_attributes);
    MMML_ASSERT(is_html_identifier(id));

    m_out.append('<', Annotation_Type::html_tag_bracket);
    m_out.append(id, Annotation_Type::html_tag_identifier);

    return Attribute_Writer { *this };
}

HTML_Writer& HTML_Writer::close_tag(std::string_view id)
{
    MMML_ASSERT(!m_in_attributes);
    MMML_ASSERT(is_html_identifier(id));
    MMML_ASSERT(m_depth != 0);

    --m_depth;

    m_out.append("</", Annotation_Type::html_tag_bracket);
    m_out.append(id, Annotation_Type::html_tag_identifier);
    m_out.append('>', Annotation_Type::html_tag_bracket);

    return *this;
}

HTML_Writer& HTML_Writer::write_comment(std::string_view comment)
{
    auto builder = m_out.build(Annotation_Type::html_comment);
    builder.append("<!--");
    append_escaped_text(builder, comment);
    builder.append("-->");
    return *this;
}

HTML_Writer& HTML_Writer::write_attribute(std::string_view key, std::string_view value)
{
    MMML_ASSERT(m_in_attributes);
    MMML_ASSERT(is_html_identifier(key));

    m_out.append(' ');
    m_out.append(key, Annotation_Type::html_attribute_key);

    if (!value.empty()) {
        m_out.append('=', Annotation_Type::html_attribute_equal);
        auto builder = m_out.build(Annotation_Type::html_attribute_value);
        if (requires_quotes_in_html_attribute(value)) {
            builder.append('"');
            builder.append(value);
            builder.append('"');
        }
        else {
            builder.append(value);
        }
    }

    return *this;
}

HTML_Writer& HTML_Writer::end_attributes()
{
    MMML_ASSERT(m_in_attributes);

    m_out.append('>', Annotation_Type::html_tag_bracket);
    m_in_attributes = false;
    ++m_depth;

    return *this;
}

HTML_Writer& HTML_Writer::end_empty_tag_attributes()
{
    MMML_ASSERT(m_in_attributes);

    m_out.append("/>", Annotation_Type::html_tag_bracket);
    m_in_attributes = false;

    return *this;
}

} // namespace mmml
