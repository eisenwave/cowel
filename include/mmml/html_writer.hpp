#ifndef BIT_MANIPULATION_BMD_HTML_WRITER_HPP
#define BIT_MANIPULATION_BMD_HTML_WRITER_HPP

#include "mmml/assert.hpp"
#include "mmml/fwd.hpp"
#include "mmml/parse.hpp"

namespace mmml {

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

private:
    Code_String& m_out;

    std::size_t m_depth = 0;
    bool m_in_attributes = false;

public:
    /// @brief Constructor.
    /// Writes nothing to the stream.
    /// `out.fail()` shall be true.
    /// @param out the output stream
    explicit HTML_Writer(Code_String& out);

    HTML_Writer(const HTML_Writer&) = delete;
    HTML_Writer& operator=(const HTML_Writer&) = delete;

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

    /// @brief Writes an empty tag such as `<br/>` or `<hr/>`.
    Self& write_empty_tag(std::string_view id);

    /// @brief Writes an HTML comment with the given contents.
    Self& write_comment(std::string_view comment);

    /// @brief Writes an opening tag such as `<div>`.
    Self& open_tag(std::string_view id);

    /// @brief Writes an incomplete opening tag such as `<div`.
    /// Returns an `Attribute_Writer` which must be used to write attributes (if any)
    /// and complete the opening tag.
    /// @param properties the tag properties
    /// @return `*this`
    [[nodiscard]]
    Attribute_Writer open_tag_with_attributes(std::string_view id);

    /// @brief Writes a closing tag, such as `</div>`.
    /// The most recent call to `open_tag` or `open_tag_with_attributes` shall have been made with
    /// the same arguments.
    Self& close_tag(std::string_view id);

    /// @brief Writes text between tags.
    /// Text characters such as `<` or `>` which interfere with HTML are converted to entities.
    void write_inner_text(std::string_view text);

    /// @brief Writes HTML content between tags.
    /// Unlike `write_inner_text`, does not escape any entities.
    ///
    /// WARNING: Improper use of this function can easily result in incorrect HTML output.
    void write_inner_html(std::string_view text);

private:
    Self& write_attribute(std::string_view key, std::string_view value);
    Self& end_attributes();
    Self& end_empty_tag_attributes();

    /// @brief Passes any text directly through to the writer, however, characters which interfere
    /// with HTML such as `<` or `>` are written as HTML entities instead.
    /// If the string contains no such entities, this function is equivalent to writing `text` to
    /// the writer directly.
    /// @param text the text to write
    void write_escaped_text(std::string_view text);
};

/// @brief RAII helper class which lets us write attributes more conveniently.
/// This class is not intended to be used directly, but with the help of `HTML_Writer`.
struct Attribute_Writer {
private:
    HTML_Writer& m_writer;

public:
    explicit Attribute_Writer(HTML_Writer& writer)
        : m_writer(writer)
    {
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
    Attribute_Writer& write_attribute(std::string_view key, std::string_view value = "")
    {
        m_writer.write_attribute(key, value);
        return *this;
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
        m_writer.end_empty_tag_attributes();
        return *this;
    }

    /// @brief Destructor.
    /// A call to `end()` or `end_empty()` shall have been made prior to destruction.
    ~Attribute_Writer() noexcept(false)
    {
        // This indicates that end() or end_empty() weren't called.
        MMML_ASSERT(m_writer.m_in_attributes);
    }
};

} // namespace mmml

#endif
