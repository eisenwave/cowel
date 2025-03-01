#ifndef MMML_CODE_STRING_HPP
#define MMML_CODE_STRING_HPP

#include <memory_resource>
#include <string_view>
#include <utility>
#include <vector>

#include "mmml/util/annotation_span.hpp"
#include "mmml/util/annotation_type.hpp"
#include "mmml/util/source_position.hpp"
#include "mmml/util/to_chars.hpp"

namespace mmml {

enum struct Sign_Policy : Default_Underlying {
    /// @brief Print only `-`, never `+`.
    negative_only,
    /// @brief Print `+` for positive numbers, including zero.
    always,
    /// @brief Print `+` only for non-zero numbers.
    nonzero
};

struct Annotated_String {
public:
    struct Length {
        std::size_t text_length;
        std::size_t span_count;
    };

    using iterator = Annotation_Span*;
    using const_iterator = const Annotation_Span*;

private:
    std::pmr::vector<char> m_text;
    std::pmr::vector<Annotation_Span> m_spans;

public:
    [[nodiscard]]
    explicit Annotated_String(std::pmr::memory_resource* memory = std::pmr::get_default_resource())
        : m_text(memory)
        , m_spans(memory)
    {
    }

    [[nodiscard]]
    Length get_length() const
    {
        return { .text_length = m_text.size(), .span_count = m_spans.size() };
    }

    [[nodiscard]]
    std::size_t get_text_length() const
    {
        return m_text.size();
    }

    [[nodiscard]]
    std::size_t get_span_count() const
    {
        return m_spans.size();
    }

    [[nodiscard]]
    std::string_view get_text() const
    {
        return { m_text.data(), m_text.size() };
    }

    [[nodiscard]]
    std::string_view get_text(const Annotation_Span& span) const
    {
        return get_text().substr(span.begin, span.length);
    }

    void resize(Length length)
    {
        m_text.resize(length.text_length);
        m_spans.resize(length.span_count);
    }

    void clear() noexcept
    {
        m_text.clear();
        m_spans.clear();
    }

    /// @brief Appends a raw range of text to the string.
    /// This is typically useful for e.g. whitespace between pieces of code.
    void append(std::string_view text)
    {
        m_text.insert(m_text.end(), text.begin(), text.end());
    }

    /// @brief Appends a raw character of text to the string.
    /// This is typically useful for e.g. whitespace between pieces of code.
    void append(char c)
    {
        m_text.push_back(c);
    }

    /// @brief Appends a raw character of text multiple times to the string.
    /// This is typically useful for e.g. whitespace between pieces of code.
    void append(std::size_t amount, char c)
    {
        m_text.insert(m_text.end(), amount, c);
    }

    void append(std::string_view text, Annotation_Type type)
    {
        MMML_ASSERT(!text.empty());
        m_spans.push_back({ .begin = m_text.size(), .length = text.size(), .type = type });
        m_text.insert(m_text.end(), text.begin(), text.end());
    }

    void append(char c, Annotation_Type type)
    {
        m_spans.push_back({ .begin = m_text.size(), .length = 1, .type = type });
        m_text.push_back(c);
    }

    template <character_convertible Integer>
    void append_integer(Integer x, Sign_Policy signs = Sign_Policy::negative_only)
    {
        const bool plus
            = (signs == Sign_Policy::always && x >= 0) || (signs == Sign_Policy::nonzero && x > 0);
        const Characters chars = to_characters(x);
        append_digits(chars.as_string(), plus);
    }

    template <character_convertible Integer>
    void
    append_integer(Integer x, Annotation_Type type, Sign_Policy signs = Sign_Policy::negative_only)
    {
        const bool plus
            = (signs == Sign_Policy::always && x >= 0) || (signs == Sign_Policy::nonzero && x > 0);
        const Characters chars = to_characters(x);
        append_digits(chars.as_string(), plus, &type);
    }

private:
    // using std::optional would obviously be more idiomatic, but we can avoid
    // #include <optional> for this file by using a pointer
    void append_digits(std::string_view digits, bool plus, const Annotation_Type* type = nullptr)
    {
        const std::size_t begin = m_text.size();
        std::size_t prefix_length = 0;
        if (plus) {
            m_text.push_back('+');
            prefix_length = 1;
        }
        append(digits);
        if (type) {
            m_spans.push_back(
                { .begin = begin, .length = digits.length() + prefix_length, .type = *type }
            );
        }
    }

public:
    struct Scoped_Builder;

    /// @brief Starts building a single code span out of multiple parts which will be fused
    /// together.
    /// For example:
    /// ```
    /// string.build(Code_Span_Type::identifier)
    ///     .append("m_")
    ///     .append(name);
    /// ```
    /// @param type the type of the appended span as a whole
    Scoped_Builder build(Annotation_Type type) &;

    [[nodiscard]]
    iterator begin()
    {
        return m_spans.data();
    }

    [[nodiscard]]
    iterator end()
    {
        return m_spans.data() + std::ptrdiff_t(m_spans.size());
    }

    [[nodiscard]]
    const_iterator begin() const
    {
        return m_spans.data();
    }

    [[nodiscard]]
    const_iterator end() const
    {
        return m_spans.data() + std::ptrdiff_t(m_spans.size());
    }

    [[nodiscard]]
    const_iterator cbegin() const
    {
        return begin();
    }

    [[nodiscard]]
    const_iterator cend() const
    {
        return end();
    }
};

struct [[nodiscard]] Annotated_String::Scoped_Builder {
private:
    Annotated_String& self;
    std::size_t initial_size;
    Annotation_Type type;

public:
    Scoped_Builder(Annotated_String& self, Annotation_Type type)
        : self { self }
        , initial_size { self.m_text.size() }
        , type { type }
    {
    }

    ~Scoped_Builder() noexcept(false)
    {
        MMML_ASSERT(self.m_text.size() >= initial_size);
        std::size_t length = self.m_text.size() - initial_size;
        if (length != 0) {
            self.m_spans.push_back(
                { .begin = initial_size, .length = self.m_text.size() - initial_size, .type = type }
            );
        }
    }

    Scoped_Builder(const Scoped_Builder&) = delete;
    Scoped_Builder& operator=(const Scoped_Builder&) = delete;

    Scoped_Builder& append(char c)
    {
        self.append(c);
        return *this;
    }

    Scoped_Builder& append(std::size_t n, char c)
    {
        self.append(n, c);
        return *this;
    }

    Scoped_Builder& append(std::string_view text)
    {
        self.append(text);
        return *this;
    }

    template <typename Integer>
    Scoped_Builder& append_integer(Integer x, Sign_Policy signs = Sign_Policy::negative_only)
    {
        self.append_integer(x, signs);
        return *this;
    }
};

inline Annotated_String::Scoped_Builder Annotated_String::build(Annotation_Type type) &
{
    return { *this, type };
}

} // namespace mmml

#endif
