#ifndef COWEL_HTML_WRITER_HPP
#define COWEL_HTML_WRITER_HPP

#include <cstddef>
#include <string_view>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/chars.hpp"

#include "cowel/fwd.hpp"

namespace cowel {

/// @brief Appends text to a vector without any processing.
void append(std::pmr::vector<char8_t>& out, std::u8string_view text);

/// @brief Appends text to the vector where characters in `charset`
/// are replaced with their corresponding HTML entities.
/// For example, if `charset` includes `&`, `&amp;` is appended in its stead.
///
/// Currently, `charset` must be a subset of `&`, `<`, `>`, `'`, `"`.
void append_html_escaped(
    std::pmr::vector<char8_t>& out,
    std::u8string_view text,
    std::u8string_view charset
);

enum struct Attribute_Style : Default_Underlying {
    /// @brief Always use double quotes, like `id="name" class="a b" hidden=""`.
    always_double,
    /// @brief Always use single quotes, like `id='name' class='a b' hidden=''`.
    always_single,
    /// @brief Use single quotes when needed, like `id=name class="a b" hidden`.
    double_if_needed,
    /// @brief Use single quotes when needed, like `id=name class='a b' hidden`.
    single_if_needed,
};

[[nodiscard]]
constexpr bool attribute_style_demands_quotes(Attribute_Style style)
{
    return style == Attribute_Style::always_double || style == Attribute_Style::always_single;
}

[[nodiscard]]
constexpr char8_t attribute_style_quote_char(Attribute_Style style)
{
    switch (style) {
    case Attribute_Style::always_double:
    case Attribute_Style::double_if_needed: return '"';
    case Attribute_Style::always_single:
    case Attribute_Style::single_if_needed: return '\'';
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid attribute style.");
}

/// @brief A class which provides member functions for writing HTML content to a stream
/// correctly.
/// Both entire HTML documents can be written, as well as HTML snippets.
/// This writer only performs checks that are possibly without additional memory.
/// These include:
/// - verifying that given tag names and values are appropriate
/// - ensuring that the number of opened tags matches the number of closed tags
/// - ensuring that the preamble is written exactly once
///
/// To correctly use this class, the opening tags must match the closing tags.
/// I.e. for every `begin_tag(tag, type)` or `begin_tag_with_attributes(tag, type)`,
/// there must be a matching `end_tag(tag, type)`.
struct HTML_Writer {
public:
    friend struct Attribute_Writer;
    using Self = HTML_Writer;
    using char_type = char8_t;
    using string_view_type = std::u8string_view;

private:
    std::pmr::vector<char_type>& m_out;

    std::size_t m_depth = 0;
    bool m_in_attributes = false;

public:
    /// @brief Constructor.
    /// Writes nothing to the stream.
    /// `out.fail()` shall be true.
    /// @param out the output stream
    [[nodiscard]]
    explicit HTML_Writer(std::pmr::vector<char_type>& out) noexcept
        : m_out { out }
    {
    }

    HTML_Writer(const HTML_Writer&) = delete;
    HTML_Writer& operator=(const HTML_Writer&) = delete;

    ~HTML_Writer() = default;

    /// @brief Returns the output vector that this writers has been constructed with,
    /// and to which all output is written.
    [[nodiscard]]
    std::pmr::vector<char_type>& get_output() const
    {
        return m_out;
    }

    /// @brief Validates whether the HTML document is complete.
    /// Namely, `write_preamble()` must have taken place and any opened tags must have
    /// been closed.
    /// This member function has false negatives, however, it has no false positives.
    /// @return `true` if the writer is in a state where the HTML document could be considered
    /// complete, `false` otherwise.
    [[nodiscard]]
    bool is_done() const
    {
        return m_depth == 0;
    }

    /// @brief Writes the `<!DOCTYPE ...` preamble for the HTML file.
    /// For whole documents should be called exactly once, prior to any other `write` functions.
    /// However, it is not required to call this.
    Self& write_preamble();

    /// @brief Writes a self-closing tag such as `<br/>` or `<hr/>`.
    Self& write_self_closing_tag(string_view_type id);

    /// @brief Writes an HTML comment with the given contents.
    Self& write_comment(string_view_type comment);

    /// @brief Writes an opening tag such as `<div>`.
    Self& open_tag(string_view_type id);

    /// @brief Writes an opening tag immediately followed by a closing tag, like `<div></div>`.
    Self& open_and_close_tag(string_view_type id);

    /// @brief Writes an incomplete opening tag such as `<div`.
    /// Returns an `Attribute_Writer` which must be used to write attributes (if any)
    /// and complete the opening tag.
    /// @param properties the tag properties
    /// @return `*this`
    [[nodiscard]]
    Attribute_Writer open_tag_with_attributes(string_view_type id);

    /// @brief Writes a closing tag, such as `</div>`.
    /// The most recent call to `open_tag` or `open_tag_with_attributes` shall have been made with
    /// the same arguments.
    Self& close_tag(string_view_type id);

    /// @brief Writes text between tags.
    /// Text characters such as `<` or `>` which interfere with HTML are converted to entities.
    void write_inner_text(string_view_type text);
    void write_inner_text(std::u32string_view text);

    void write_inner_text(char8_t c)
    {
        COWEL_DEBUG_ASSERT(is_ascii(c));
        write_inner_text({ &c, 1 });
    }

    void write_inner_text(char32_t c);

    /// @brief Writes HTML content between tags.
    /// Unlike `write_inner_text`, does not escape any entities.
    ///
    /// WARNING: Improper use of this function can easily result in incorrect HTML output.
    void write_inner_html(std::u8string_view text);
    void write_inner_html(std::u32string_view text);

    void write_inner_html(char8_t c)
    {
        COWEL_DEBUG_ASSERT(is_ascii(c));
        do_write(c);
    }
    void write_inner_html(char32_t c);

private:
    enum struct Attribute_Encoding : Default_Underlying {
        text,
        url,
    };

    Self& write_attribute(
        string_view_type key,
        string_view_type value,
        Attribute_Style style,
        Attribute_Encoding encoding
    );
    Self& write_empty_attribute(string_view_type key, Attribute_Style style);
    Self& end_attributes();
    Self& end_empty_tag_attributes();

    void do_write(char_type);
    void do_write(string_view_type);
};

/// @brief RAII helper class which lets us write attributes more conveniently.
/// This class is not intended to be used directly, but with the help of `HTML_Writer`.
struct Attribute_Writer {
private:
    using string_view_type = HTML_Writer::string_view_type;

    HTML_Writer& m_writer;
    /// @brief If this is `true`,
    /// it would not be safe to append a `/` character to the written data because it may be
    /// included in the value of an attribute.
    /// For example, this can happen when writing `<br id=xyz`.
    /// Now appending `/>` to close the attribute would result in the `/` being appended to `xzy`.
    bool m_unsafe_slash = false;

public:
    explicit Attribute_Writer(HTML_Writer& writer)
        : m_writer(writer)
    {
        m_writer.m_in_attributes = true;
    }

    Attribute_Writer(const Attribute_Writer&) = delete;
    Attribute_Writer& operator=(const Attribute_Writer&) = delete;

    /// @brief Writes an attribute to the stream, such as `class=centered`.
    /// If `value` is empty, writes `key` on its own.
    /// If `value` requires quotes to comply with the HTML standard, quotes are added.
    /// For example, if `value` is `x y`, `key="x y"` is written.
    /// @param key the attribute key; `is_identifier(key)` shall be `true`.
    /// @param value the attribute value, or an empty string
    /// @return `*this`
    Attribute_Writer& write_attribute(
        string_view_type key,
        string_view_type value,
        Attribute_Style style = Attribute_Style::double_if_needed
    )
    {
        m_writer.write_attribute(key, value, style, HTML_Writer::Attribute_Encoding::text);
        const char8_t back = m_writer.m_out.back();
        m_unsafe_slash = back != u8'\'' && back != u8'"';
        return *this;
    }

    /// @brief Like `write_attribute`,
    /// but applies minimal URL encoding to the value.
    Attribute_Writer& write_url_attribute(
        string_view_type key,
        string_view_type value,
        Attribute_Style style = Attribute_Style::double_if_needed
    )
    {
        m_writer.write_attribute(key, value, style, HTML_Writer::Attribute_Encoding::url);
        const char8_t back = m_writer.m_out.back();
        m_unsafe_slash = back != u8'\'' && back != u8'"';
        return *this;
    }

    Attribute_Writer& write_empty_attribute(
        string_view_type key,
        Attribute_Style style = Attribute_Style::double_if_needed
    )
    {
        m_writer.write_empty_attribute(key, style);
        m_unsafe_slash = false;
        return *this;
    }

    Attribute_Writer&
    write_charset(string_view_type value, Attribute_Style style = Attribute_Style::double_if_needed)
    {
        return write_attribute(u8"charset", value, style);
    }

    Attribute_Writer&
    write_class(string_view_type value, Attribute_Style style = Attribute_Style::double_if_needed)
    {
        return write_attribute(u8"class", value, style);
    }

    Attribute_Writer&
    write_content(string_view_type value, Attribute_Style style = Attribute_Style::double_if_needed)
    {
        return write_attribute(u8"content", value, style);
    }

    Attribute_Writer& write_crossorigin()
    {
        return write_empty_attribute(u8"crossorigin");
    }

    Attribute_Writer&
    write_href(string_view_type value, Attribute_Style style = Attribute_Style::double_if_needed)
    {
        return write_url_attribute(u8"href", value, style);
    }

    Attribute_Writer&
    write_id(string_view_type value, Attribute_Style style = Attribute_Style::double_if_needed)
    {
        return write_attribute(u8"id", value, style);
    }

    Attribute_Writer&
    write_name(string_view_type value, Attribute_Style style = Attribute_Style::double_if_needed)
    {
        return write_attribute(u8"name", value, style);
    }

    Attribute_Writer&
    write_rel(string_view_type value, Attribute_Style style = Attribute_Style::double_if_needed)
    {
        return write_attribute(u8"rel", value, style);
    }

    Attribute_Writer&
    write_src(string_view_type value, Attribute_Style style = Attribute_Style::double_if_needed)
    {
        return write_attribute(u8"src", value, style);
    }

    Attribute_Writer& write_tabindex(
        string_view_type value,
        Attribute_Style style = Attribute_Style::double_if_needed
    )
    {
        return write_attribute(u8"tabindex", value, style);
    }

    /// @brief Writes `>` and finishes writing attributes.
    /// This function or `end_empty()` shall be called exactly once prior to destruction of this
    /// writer.
    Attribute_Writer& end()
    {
        m_writer.end_attributes();
        return *this;
    }

    /// @brief Writes `/>` and finishes writing attributes.
    /// This function or `end()` shall be called exactly once prior to destruction of this
    /// writer.
    Attribute_Writer& end_empty()
    {
        if (m_unsafe_slash) {
            m_writer.do_write(u8' ');
        }
        m_writer.end_empty_tag_attributes();
        return *this;
    }

    /// @brief Destructor.
    /// A call to `end()` or `end_empty()` shall have been made prior to destruction.
    ~Attribute_Writer() noexcept(false)
    {
        // This indicates that end() or end_empty() weren't called.
        COWEL_ASSERT(!m_writer.m_in_attributes);
    }
};

} // namespace cowel

#endif
