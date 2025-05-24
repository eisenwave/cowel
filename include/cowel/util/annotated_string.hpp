#ifndef COWEL_ANNOTATED_STRING_HPP
#define COWEL_ANNOTATED_STRING_HPP

#include <cstddef>
#include <memory_resource>
#include <string_view>
#include <utility>
#include <vector>

#include "cowel/util/annotation_span.hpp"
#include "cowel/util/assert.hpp"
#include "cowel/util/to_chars.hpp"

#include "cowel/fwd.hpp"

namespace cowel {

enum struct Sign_Policy : Default_Underlying {
    /// @brief Print only `-`, never `+`.
    negative_only,
    /// @brief Print `+` for positive numbers, including zero.
    always,
    /// @brief Print `+` only for non-zero numbers.
    nonzero
};

struct Annotated_String_Length {
    std::size_t text_length;
    std::size_t span_count;
};

template <typename Char, typename T>
struct Basic_Annotated_String {
public:
    using span_type = Annotation_Span<T>;
    using iterator = span_type*;
    using const_iterator = const span_type*;
    using char_type = Char;
    using string_view_type = std::basic_string_view<Char>;

private:
    std::pmr::vector<Char> m_text;
    std::pmr::vector<span_type> m_spans;

public:
    [[nodiscard]]
    constexpr explicit Basic_Annotated_String(
        std::pmr::memory_resource* memory = std::pmr::get_default_resource()
    )
        : m_text { memory }
        , m_spans { memory }
    {
    }

    [[nodiscard]]
    constexpr Annotated_String_Length get_length() const
    {
        return { .text_length = m_text.size(), .span_count = m_spans.size() };
    }

    [[nodiscard]]
    constexpr std::size_t get_text_length() const
    {
        return m_text.size();
    }

    [[nodiscard]]
    constexpr std::pmr::memory_resource* get_memory() const
    {
        return m_text.get_allocator().resource();
    }

    [[nodiscard]]
    constexpr std::size_t get_span_count() const
    {
        return m_spans.size();
    }

    [[nodiscard]]
    constexpr string_view_type get_text() const
    {
        return { m_text.data(), m_text.size() };
    }

    [[nodiscard]]
    constexpr string_view_type get_text(const span_type& span) const
    {
        return get_text().substr(span.begin, span.length);
    }

    constexpr void resize(Annotated_String_Length length)
    {
        m_text.resize(length.text_length);
        m_spans.resize(length.span_count);
    }

    constexpr void clear() noexcept
    {
        m_text.clear();
        m_spans.clear();
    }

    /// @brief Appends a raw range of text to the string.
    /// This is typically useful for e.g. whitespace between pieces of code.
    constexpr void append(string_view_type text)
    {
        m_text.insert(m_text.end(), text.begin(), text.end());
    }

    /// @brief Appends a raw character of text to the string.
    /// This is typically useful for e.g. whitespace between pieces of code.
    constexpr void append(char_type c)
    {
        m_text.push_back(c);
    }

    /// @brief Appends a raw character of text multiple times to the string.
    /// This is typically useful for e.g. whitespace between pieces of code.
    constexpr void append(std::size_t amount, char_type c)
    {
        m_text.insert(m_text.end(), amount, c);
    }

    constexpr void append(string_view_type text, T value)
    {
        COWEL_ASSERT(!text.empty());
        m_spans.push_back({ .begin = m_text.size(), .length = text.size(), .value = value });
        m_text.insert(m_text.end(), text.begin(), text.end());
    }

    constexpr void append(char_type c, T value)
    {
        m_spans.push_back({ .begin = m_text.size(), .length = 1, .value = value });
        m_text.push_back(c);
    }

    template <character_convertible Integer>
    constexpr void append_integer(Integer x, Sign_Policy signs = Sign_Policy::negative_only)
    {
        const bool plus
            = (signs == Sign_Policy::always && x >= 0) || (signs == Sign_Policy::nonzero && x > 0);
        const Basic_Characters chars = to_characters<char_type>(x);
        append_digits(chars.as_string(), plus);
    }

    template <character_convertible Integer>
    constexpr void
    append_integer(Integer x, T value, Sign_Policy signs = Sign_Policy::negative_only)
    {
        const bool plus
            = (signs == Sign_Policy::always && x >= 0) || (signs == Sign_Policy::nonzero && x > 0);
        const Basic_Characters chars = to_characters<char_type>(x);
        append_digits(chars.as_string(), plus, &value);
    }

private:
    // using std::optional would obviously be more idiomatic, but we can avoid
    // #include <optional> for this file by using a pointer
    constexpr void append_digits(string_view_type digits, bool plus, const T* value = nullptr)
    {
        const std::size_t begin = m_text.size();
        std::size_t prefix_length = 0;
        if (plus) {
            m_text.push_back('+');
            prefix_length = 1;
        }
        append(digits);
        if (value) {
            m_spans.push_back(
                { .begin = begin, .length = digits.length() + prefix_length, .value = *value }
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
    constexpr Scoped_Builder build(Diagnostic_Highlight type) &
    {
        return { *this, type };
    }

    [[nodiscard]]
    constexpr iterator begin()
    {
        return m_spans.data();
    }

    [[nodiscard]]
    constexpr iterator end()
    {
        return m_spans.data() + std::ptrdiff_t(m_spans.size());
    }

    [[nodiscard]]
    constexpr const_iterator begin() const
    {
        return m_spans.data();
    }

    [[nodiscard]]
    constexpr const_iterator end() const
    {
        return m_spans.data() + std::ptrdiff_t(m_spans.size());
    }

    [[nodiscard]]
    constexpr const_iterator cbegin() const
    {
        return begin();
    }

    [[nodiscard]]
    constexpr const_iterator cend() const
    {
        return end();
    }
};

template <typename Char, typename T>
struct [[nodiscard]] Basic_Annotated_String<Char, T>::Scoped_Builder {
private:
    using owner_type = Basic_Annotated_String<Char, T>;
    using char_type = owner_type::char_type;
    using string_view_type = owner_type::string_view_type;

    owner_type& m_self;
    std::size_t m_initial_size;
    T m_value;

public:
    constexpr Scoped_Builder(owner_type& self, T value)
        : m_self { self }
        , m_initial_size { self.m_text.size() }
        , m_value { std::move(value) }
    {
    }

    constexpr ~Scoped_Builder() noexcept(false)
    {
        COWEL_ASSERT(m_self.m_text.size() >= m_initial_size);
        const std::size_t length = m_self.m_text.size() - m_initial_size;
        if (length != 0) {
            m_self.m_spans.push_back({ .begin = m_initial_size,
                                       .length = m_self.m_text.size() - m_initial_size,
                                       .value = m_value });
        }
    }

    Scoped_Builder(const Scoped_Builder&) = delete;
    Scoped_Builder& operator=(const Scoped_Builder&) = delete;

    constexpr Scoped_Builder& append(char_type c)
    {
        m_self.append(c);
        return *this;
    }

    constexpr Scoped_Builder& append(std::size_t n, char_type c)
    {
        m_self.append(n, c);
        return *this;
    }

    constexpr Scoped_Builder& append(string_view_type text)
    {
        m_self.append(text);
        return *this;
    }

    template <character_convertible Integer>
    constexpr Scoped_Builder&
    append_integer(Integer x, Sign_Policy signs = Sign_Policy::negative_only)
    {
        m_self.append_integer(x, signs);
        return *this;
    }
};

} // namespace cowel

#endif
