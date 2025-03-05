#include "mmml/util/html_writer.hpp"
#include "mmml/util/annotated_string.hpp"
#include "mmml/util/source_position.hpp"
#include "mmml/util/strings.hpp"
#include "mmml/util/unicode.hpp"

namespace mmml {
namespace {

void append_escaped_text(Annotated_String8::Scoped_Builder& builder, std::u8string_view text)
{
    while (!text.empty()) {
        const std::size_t bracket_pos = text.find_first_of(u8"<>");
        const auto snippet = text.substr(0, std::min(text.length(), bracket_pos));
        builder.append(snippet);
        if (bracket_pos == std::string_view::npos) {
            break;
        }
        else if (text[bracket_pos] == u8'<') {
            builder.append(u8"&lt;");
        }
        else if (text[bracket_pos] == u8'>') {
            builder.append(u8"&gt;");
        }
        else {
            MMML_ASSERT_UNREACHABLE(u8"Logical mistake.");
        }

        text = text.substr(bracket_pos + 1);
    }
}

} // namespace

HTML_Writer::HTML_Writer(Annotated_String8& out)
    : m_out(out)
{
}

void HTML_Writer::write_inner_text(string_view_type text)
{
    MMML_ASSERT(!m_in_attributes);

    auto builder = m_out.build(Annotation_Type::html_inner_text);
    append_escaped_text(builder, text);
}

void HTML_Writer::write_inner_html(string_view_type text)
{
    MMML_ASSERT(!m_in_attributes);
    m_out.append(text, Annotation_Type::html_inner_text);
}

HTML_Writer& HTML_Writer::write_preamble()
{
    MMML_ASSERT(!m_in_attributes);

    m_out.append(u8"<!", Annotation_Type::html_tag_bracket);
    m_out.append(u8"DOCTYPE html", Annotation_Type::html_preamble);
    m_out.append(u8">", Annotation_Type::html_tag_bracket);
    m_out.append('\n');

    return *this;
}

HTML_Writer& HTML_Writer::write_empty_tag(string_view_type id)
{
    MMML_ASSERT(!m_in_attributes);
    MMML_ASSERT(is_html_tag_name(id));

    m_out.append(u8'<', Annotation_Type::html_tag_bracket);
    m_out.append(id, Annotation_Type::html_tag_identifier);
    m_out.append(u8"/>", Annotation_Type::html_tag_bracket);

    return *this;
}

HTML_Writer& HTML_Writer::open_tag(string_view_type id)
{
    MMML_ASSERT(!m_in_attributes);
    MMML_ASSERT(is_html_tag_name(id));

    m_out.append(u8'<', Annotation_Type::html_tag_bracket);
    m_out.append(id, Annotation_Type::html_tag_identifier);
    m_out.append(u8'>', Annotation_Type::html_tag_bracket);
    ++m_depth;

    return *this;
}

Attribute_Writer HTML_Writer::open_tag_with_attributes(string_view_type id)
{
    MMML_ASSERT(!m_in_attributes);
    MMML_ASSERT(is_html_tag_name(id));

    m_out.append(u8'<', Annotation_Type::html_tag_bracket);
    m_out.append(id, Annotation_Type::html_tag_identifier);

    return Attribute_Writer { *this };
}

HTML_Writer& HTML_Writer::close_tag(string_view_type id)
{
    MMML_ASSERT(!m_in_attributes);
    MMML_ASSERT(is_html_tag_name(id));
    MMML_ASSERT(m_depth != 0);

    --m_depth;

    m_out.append(u8"</", Annotation_Type::html_tag_bracket);
    m_out.append(id, Annotation_Type::html_tag_identifier);
    m_out.append(u8'>', Annotation_Type::html_tag_bracket);

    return *this;
}

HTML_Writer& HTML_Writer::write_comment(string_view_type comment)
{
    auto builder = m_out.build(Annotation_Type::html_comment);
    builder.append(u8"<!--");
    append_escaped_text(builder, comment);
    builder.append(u8"-->");
    return *this;
}

HTML_Writer& HTML_Writer::write_attribute(string_view_type key, string_view_type value)
{
    MMML_ASSERT(m_in_attributes);
    MMML_ASSERT(is_html_attribute_name(key));

    m_out.append(u8' ');
    m_out.append(key, Annotation_Type::html_attribute_key);

    if (!value.empty()) {
        m_out.append(u8'=', Annotation_Type::html_attribute_equal);
        auto builder = m_out.build(Annotation_Type::html_attribute_value);
        if (requires_quotes_in_html_attribute(value)) {
            builder.append(u8'"');
            builder.append(value);
            builder.append(u8'"');
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

    m_out.append(u8'>', Annotation_Type::html_tag_bracket);
    m_in_attributes = false;
    ++m_depth;

    return *this;
}

HTML_Writer& HTML_Writer::end_empty_tag_attributes()
{
    MMML_ASSERT(m_in_attributes);

    m_out.append(u8"/>", Annotation_Type::html_tag_bracket);
    m_in_attributes = false;

    return *this;
}

} // namespace mmml
