#ifndef MMML_AST_HPP
#define MMML_AST_HPP

#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include "mmml/util/meta.hpp"
#include "mmml/util/source_position.hpp"

#include "mmml/fwd.hpp"

namespace mmml::ast {

struct Argument final {
private:
    Source_Span m_pos;
    std::pmr::vector<Content> m_content;
    Source_Span m_name;

public:
    [[nodiscard]]
    Argument(
        const Source_Span& pos,
        const Source_Span& name,
        std::pmr::vector<ast::Content>&& children
    );

    [[nodiscard]]
    Argument(const Source_Span& pos, std::pmr::vector<ast::Content>&& children);

    Argument(Argument&&) noexcept;
    Argument(const Argument&);

    Argument& operator=(Argument&&) noexcept;
    Argument& operator=(const Argument&);

    ~Argument();

    [[nodiscard]]
    Source_Span get_source_span() const
    {
        return m_pos;
    }

    [[nodiscard]]
    std::u8string_view get_source(std::u8string_view source) const
    {
        MMML_ASSERT(m_pos.begin + m_pos.length <= source.size());
        return source.substr(m_pos.begin, m_pos.length);
    }

    [[nodiscard]]
    bool has_name() const
    {
        return !m_name.empty();
    }
    [[nodiscard]]
    Source_Span get_name_span() const;
    [[nodiscard]]
    std::u8string_view get_name(std::u8string_view source) const
    {
        MMML_ASSERT(m_name.begin + m_name.length <= source.size());
        return source.substr(m_name.begin, m_name.length);
    }

    [[nodiscard]]
    std::span<Content> get_content() &;
    [[nodiscard]]
    std::span<const Content> get_content() const&;
    [[nodiscard]]
    std::pmr::vector<Content>&& get_content() &&;
};

struct Directive final {
private:
    Source_Span m_pos;
    std::size_t m_name_length;

    std::pmr::vector<Argument> m_arguments;
    std::pmr::vector<Content> m_content;

public:
    [[nodiscard]]
    Directive(
        const Source_Span& pos,
        std::size_t name_length,
        std::pmr::vector<Argument>&& args,
        std::pmr::vector<Content>&& block
    );

    Directive(Directive&&) noexcept;
    Directive(const Directive&);

    Directive& operator=(Directive&&) noexcept;
    Directive& operator=(const Directive&);

    ~Directive();

    [[nodiscard]]
    Source_Span get_source_span() const
    {
        return m_pos;
    }

    [[nodiscard]]
    std::u8string_view get_source(std::u8string_view source) const
    {
        MMML_ASSERT(m_pos.begin + m_pos.length <= source.size());
        return source.substr(m_pos.begin, m_pos.length);
    }

    [[nodiscard]]
    std::u8string_view get_name(std::u8string_view source) const
    {
        return source.substr(m_pos.begin + 1, m_name_length);
    }

    [[nodiscard]]
    std::span<Argument> get_arguments();
    [[nodiscard]]
    std::span<const Argument> get_arguments() const;
    [[nodiscard]]
    std::span<Content> get_content();
    [[nodiscard]]
    std::span<Content const> get_content() const;
};

struct Behaved_Content final {
private:
    Content_Behavior* m_behavior;
    std::pmr::vector<Content> m_content;

public:
    [[nodiscard]]
    Behaved_Content(Content_Behavior& behavior, std::pmr::vector<Content>&& block);

    Behaved_Content(Behaved_Content&&) noexcept;
    Behaved_Content(const Behaved_Content&);

    Behaved_Content& operator=(Behaved_Content&&) noexcept;
    Behaved_Content& operator=(const Behaved_Content&);

    ~Behaved_Content();

    [[nodiscard]]
    Content_Behavior& get_behavior() const
    {
        return *m_behavior;
    }

    [[nodiscard]]
    std::span<Content> get_content();
    [[nodiscard]]
    std::span<Content const> get_content() const;
};

struct Text final {
private:
    Source_Span m_pos;

public:
    [[nodiscard]]
    Text(const Source_Span& pos);

    [[nodiscard]]
    Source_Span get_source_span() const
    {
        return m_pos;
    }

    [[nodiscard]]
    std::u8string_view get_source(std::u8string_view source) const
    {
        MMML_ASSERT(m_pos.begin + m_pos.length <= source.size());
        return source.substr(m_pos.begin, m_pos.length);
    }

    [[nodiscard]]
    std::u8string_view get_text(std::u8string_view source) const
    {
        return source.substr(m_pos.begin, m_pos.length);
    }
};

/// @brief An escape sequence, such as `\\{`, `\\}`, or `\\\\`.
struct Escaped final {
private:
    Source_Span m_pos;

public:
    [[nodiscard]]
    Escaped(const Source_Span& pos);

    [[nodiscard]]
    Source_Span get_source_span() const
    {
        return m_pos;
    }

    [[nodiscard]]
    std::u8string_view get_source(std::u8string_view source) const
    {
        MMML_ASSERT(m_pos.begin + m_pos.length <= source.size());
        return source.substr(m_pos.begin, m_pos.length);
    }

    /// @brief Returns the escaped character.
    [[nodiscard]]
    char8_t get_char(std::u8string_view source) const
    {
        return source[get_char_index()];
    }

    /// @brief Returns the index of the escaped character in the source file.
    [[nodiscard]]
    std::size_t get_char_index() const
    {
        return m_pos.begin + 1;
    }

    /// @brief Returns a two-character substring of the `source`,
    /// where the first character is the escaping backslash,
    /// and the second character is the escaped character.
    [[nodiscard]]
    std::u8string_view get_text(std::u8string_view source) const
    {
        return source.substr(m_pos.begin, m_pos.length);
    }
};

using Content_Variant = std::variant<Directive, Behaved_Content, Text, Escaped>;

struct Content : Content_Variant {
    using Content_Variant::variant;
};

static_assert(std::is_move_constructible_v<Content>);
static_assert(std::is_move_constructible_v<Content>);
static_assert(std::is_copy_assignable_v<Content>);
static_assert(std::is_move_assignable_v<Content>);

// Suppress false positive: https://github.com/llvm/llvm-project/issues/130745
// NOLINTBEGIN(readability-redundant-inline-specifier)
inline Argument::Argument(Argument&&) noexcept = default;
inline Argument::Argument(const Argument&) = default;
inline Argument& Argument::operator=(Argument&&) noexcept = default;
inline Argument& Argument::operator=(const Argument&) = default;
inline Argument::~Argument() = default;

inline Directive::Directive(Directive&&) noexcept = default;
inline Directive::Directive(const Directive&) = default;
inline Directive& Directive::operator=(Directive&&) noexcept = default;
inline Directive& Directive::operator=(const Directive&) = default;
inline Directive::~Directive() = default;

inline Behaved_Content::Behaved_Content(Behaved_Content&&) noexcept = default;
inline Behaved_Content::Behaved_Content(const Behaved_Content&) = default;
inline Behaved_Content& Behaved_Content::operator=(Behaved_Content&&) noexcept = default;
inline Behaved_Content& Behaved_Content::operator=(const Behaved_Content&) = default;
inline Behaved_Content::~Behaved_Content() = default;
// NOLINTEND(readability-redundant-inline-specifier)

inline std::span<Content> Argument::get_content() &
{
    return m_content;
}
inline std::span<const Content> Argument::get_content() const&
{
    return m_content;
}
inline std::pmr::vector<Content>&& Argument::get_content() &&
{
    return std::move(m_content);
}

inline std::span<Argument> Directive::get_arguments()
{
    return m_arguments;
}
inline std::span<const Argument> Directive::get_arguments() const
{
    return m_arguments;
}

inline std::span<Content> Directive::get_content()
{
    return m_content;
}
inline std::span<Content const> Directive::get_content() const
{
    return m_content;
}

inline std::span<Content> Behaved_Content::get_content()
{
    return m_content;
}
inline std::span<Content const> Behaved_Content::get_content() const
{
    return m_content;
}

[[nodiscard]]
inline Source_Span get_source_span(const Content& node)
{
    return visit(
        []<typename T>(const T& v) -> Source_Span {
            if constexpr (!std::is_same_v<T, Behaved_Content>) {
                return v.get_source_span();
            }
            else {
                return {};
            }
        },
        node
    );
}

[[nodiscard]]
inline std::u8string_view get_source(const Content& node, std::u8string_view source)
{
    return visit(
        [source]<typename T>(const T& v) -> std::u8string_view {
            if constexpr (!std::is_same_v<T, Behaved_Content>) {
                return v.get_source(source);
            }
            else {
                return {};
            }
        },
        node
    );
}

template <bool constant>
struct Visitor_Impl {
    using Argument_Type = const_if_t<Argument, constant>;
    using Text_Type = const_if_t<Text, constant>;
    using Escaped_Type = const_if_t<Escaped, constant>;
    using Directive_Type = const_if_t<Directive, constant>;
    using Behaved_Content_Type = const_if_t<Behaved_Content, constant>;
    using Content_Type = const_if_t<Content, constant>;

    void visit_arguments(Directive_Type& directive)
    {
        for (Argument_Type& arg : directive.get_arguments()) {
            this->visit(arg);
        }
    }

    void visit_children(Directive_Type& directive)
    {
        visit_arguments();
        visit_content_sequence(directive.get_content());
    }

    void visit_children(Argument_Type& argument)
    {
        visit_content_sequence(argument.get_content());
    }

    void visit_content(Content_Type& content)
    {
        std::visit([&](auto& c) { this->visit(c); }, content);
    }

    void visit_content_sequence(std::span<Content_Type> content)
    {
        for (Content_Type& c : content) {
            this->visit_content(c);
        }
    }

    virtual void visit(Argument_Type& argument) = 0;
    virtual void visit(Directive_Type& directive) = 0;
    virtual void visit(Behaved_Content_Type& behaved_content) = 0;
    virtual void visit(Text_Type& text) = 0;
    virtual void visit(Escaped_Type& text) = 0;
};

using Visitor = Visitor_Impl<false>;
using Const_Visitor = Visitor_Impl<true>;

} // namespace mmml::ast

#endif
