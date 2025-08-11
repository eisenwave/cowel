#include <algorithm>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include "ulight/ulight.hpp"

#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_ops.hpp"
#include "cowel/util/html_names.hpp"
#include "cowel/util/html_writer.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/policy/content_policy.hpp"
#include "cowel/policy/html.hpp"
#include "cowel/policy/paragraph_split.hpp"
#include "cowel/policy/syntax_highlight.hpp"

#include "cowel/fwd.hpp"
#include "cowel/output_language.hpp"
#include "cowel/parse_utils.hpp"
#include "cowel/services.hpp"
#include "cowel/settings.hpp"

namespace cowel {
namespace {

constexpr HTML_Tag_Name highlighting_tag = html_tag::h_;
constexpr HTML_Attribute_Name highlighting_attribute = html_attr::data_h;
constexpr auto highlighting_attribute_style = Attribute_Style::double_if_needed;

bool index_ranges_intersect(
    std::size_t begin_a,
    std::size_t length_a,
    std::size_t begin_b,
    std::size_t length_b
)
{
    const auto end_a = begin_a + length_a;
    const auto end_b = begin_b + length_b;
    return begin_a < end_b && begin_b < end_a;
}

/// @brief Writes HTML containing syntax highlighting elements to `out`.
/// @param out The writer.
/// @param code The highlighted source code.
/// @param begin The first index within `code` to be highlighted.
/// @param end The amount of characters to highlight.
/// @param highlights The highlights for `source`.
void generate_highlighted_html(
    Text_Buffer_HTML_Writer& out,
    std::u8string_view code,
    std::size_t begin,
    std::size_t length,
    std::span<const Highlight_Span> highlights
)
{
    if constexpr (enable_empty_string_assertions) {
        COWEL_ASSERT(length != 0);
    }
    else if (length == 0) {
        return;
    }
    COWEL_ASSERT(begin < code.length());
    COWEL_ASSERT(begin + length <= code.length());

    constexpr auto projection
        = [](const Highlight_Span& highlight) { return highlight.begin + highlight.length; };
    auto it = std::ranges::upper_bound(highlights, begin, {}, projection);
    std::size_t index = begin;

    for (; it != highlights.end() && index_ranges_intersect(begin, length, it->begin, it->length);
         ++it) {
        const Highlight_Span& highlight = *it;
        COWEL_ASSERT(highlight.begin < code.length());
        COWEL_ASSERT(highlight.begin + highlight.length <= code.length());

        // Leading non-highlighted content.
        if (highlight.begin > index) {
            out.write_inner_text(code.substr(index, highlight.begin - index));
            index = highlight.begin;
        }
        // This length limit is necessary because it is possible that the source reference ends
        // in the middle of a highlight, like:
        //     \i{in}t x = 0
        // where the keyword highlight for "int" would extend further than the reference for "in".
        const std::size_t actual_end = std::min(begin + length, highlight.begin + highlight.length);
        if (index >= actual_end) {
            // TODO: investigate whether this is actually possible;
            //       I suspect this could be an assertion.
            break;
        }

        const std::u8string_view id
            = ulight::highlight_type_short_string_u8(ulight::Highlight_Type(highlight.type));
        out.open_tag_with_attributes(highlighting_tag)
            .write_attribute(highlighting_attribute, id, highlighting_attribute_style)
            .end();
        out.write_inner_text(code.substr(index, actual_end - index));
        out.close_tag(highlighting_tag);
        index = actual_end;
    }

    // Trailing non-highlighted content, but still within the code range.
    const std::size_t code_range_end = begin + length;
    COWEL_ASSERT(index <= code_range_end);
    if (index < code_range_end) {
        out.write_inner_text(code.substr(index, code_range_end - index));
    }
}

} // namespace

bool Syntax_Highlight_Policy::write(Char_Sequence8 chars, Output_Language language)
{
    const std::size_t chars_size = chars.size();
    if constexpr (enable_empty_string_assertions) {
        COWEL_ASSERT(chars_size != 0);
    }
    else if (chars_size == 0) {
        return true;
    }

    switch (language) {
    case Output_Language::none: {
        COWEL_ASSERT_UNREACHABLE(u8"None input.");
    }
    case Output_Language::text: {
        return write_highlighted_text(chars, Span_Type::highlight);
    }
    case Output_Language::html: {
        const std::size_t initial_size = m_html_text.size();
        m_spans.push_back({ Span_Type::html, initial_size, chars_size });
        append(m_html_text, chars);
        COWEL_ASSERT(m_html_text.size() == initial_size + chars_size);
        return true;
    }
    default: {
        return false;
    }
    }
}

bool Syntax_Highlight_Policy::write_highlighted_text(Char_Sequence8 chars, Span_Type type)
{
    const std::size_t chars_size = chars.size();
    if constexpr (enable_empty_string_assertions) {
        COWEL_ASSERT(chars_size != 0);
    }
    const std::size_t initial_size = m_highlighted_text.size();
    m_spans.push_back({ type, initial_size, chars_size });
    append(m_highlighted_text, chars);
    COWEL_ASSERT(m_highlighted_text.size() == initial_size + chars_size);
    return true;
}

Result<void, Syntax_Highlight_Error>
Syntax_Highlight_Policy::dump_html_to(Text_Sink& out, Context& context, std::u8string_view language)
{
    const std::size_t initial_size = m_highlighted_text.size();
    m_highlighted_text.insert(m_highlighted_text.end(), m_suffix.begin(), m_suffix.end());

    const auto code_string = as_u8string_view(m_highlighted_text);

    Syntax_Highlighter& highlighter = context.get_highlighter();
    std::pmr::vector<Highlight_Span> highlights { context.get_transient_memory() };

    const Result<void, Syntax_Highlight_Error> result
        = highlighter(highlights, code_string, language, context.get_transient_memory());
    // Even if highlighting failed, this just means that we don't get any/correct
    // highlight spans, but the process can carry on as usual.

    m_highlighted_text.resize(initial_size);

    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };

    for (const Output_Span& span : m_spans) {
        switch (span.type) {
        case Span_Type::html: {
            const auto snippet = as_u8string_view(m_html_text).substr(span.begin, span.length);
            writer.write_inner_html(snippet);
            break;
        }
        case Span_Type::highlight: {
            generate_highlighted_html(writer, code_string, span.begin, span.length, highlights);
            break;
        }
        case Span_Type::phantom: {
            // Deliberately do nothing.
            // Phantom text is only relevant to generating highlights,
            // but does not appear in the output.
            break;
        }
        }
    }

    buffer.flush();
    return result;
}

void Paragraph_Split_Policy::split_into_paragraphs(std::u8string_view text)
{
    const auto new_state = text.empty()                    ? m_line_state
        : text.ends_with(u8'\r') || text.ends_with(u8'\n') ? Blank_Line_Initial_State::normal
                                                           : Blank_Line_Initial_State::middle;

    if (m_line_state == Blank_Line_Initial_State::middle) {
        // We need to consider the special case of a single leading `\n`.
        // This is technically a blank line when it appears at the start of a string,
        // but is irrelevant to forming paragraphs.
        //
        // For example, we could have two `\b{}` directives separated by a single newline.
        // This is a blank line when looking at the contents of the `ast::Text` node,
        // but isn't a blank line within the context of the document.
        if (const Blank_Line blank = find_blank_line_sequence(text);
            blank.begin == 0 && blank.length == 1) {
            HTML_Content_Policy::write(text[0], Output_Language::html);
            text.remove_prefix(1);
        }
    }

    while (!text.empty()) {
        const Blank_Line blank = find_blank_line_sequence(text);
        if (!blank) {
            COWEL_ASSERT(blank.begin == 0);
            enter_paragraph();
            HTML_Content_Policy::write(text, Output_Language::text);
            break;
        }

        // If the blank isn't at the start of the text,
        // that means we have some plain character prior to the blank
        // which we need write first.
        if (blank.begin != 0) {
            enter_paragraph();
            HTML_Content_Policy::write(text.substr(0, blank.begin), Output_Language::text);
            text.remove_prefix(blank.begin);
            COWEL_ASSERT(text.length() >= blank.length);
        }
        leave_paragraph();
        HTML_Content_Policy::write(text.substr(0, blank.length), Output_Language::text);
        text.remove_prefix(blank.length);
    }

    m_line_state = new_state;
}

} // namespace cowel
