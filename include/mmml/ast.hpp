#ifndef MMML_AST_HPP
#define MMML_AST_HPP

#include <memory_resource>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include "mmml/util/meta.hpp"
#include "mmml/util/source_position.hpp"

#include "mmml/fwd.hpp"

namespace mmml::ast {

namespace detail {
struct Base {
    Local_Source_Span m_pos;

    // TODO: rename to get_source_span
    [[nodiscard]]
    Local_Source_Span get_source_position() const
    {
        return m_pos;
    }

    [[nodiscard]]
    std::string_view get_source(std::string_view source) const
    {
        MMML_ASSERT(m_pos.begin + m_pos.length <= source.size());
        return source.substr(m_pos.begin, m_pos.length);
    }
};

} // namespace detail

struct Argument final : detail::Base {
private:
    std::pmr::vector<Content> m_content;
    Local_Source_Span m_name;

public:
    [[nodiscard]]
    Argument(
        const Local_Source_Span& pos,
        const Local_Source_Span& name,
        std::pmr::vector<ast::Content>&& children
    );

    [[nodiscard]]
    Argument(const Local_Source_Span& pos, std::pmr::vector<ast::Content>&& children);

    ~Argument();

    [[nodiscard]]
    bool has_name() const
    {
        return !m_name.empty();
    }
    [[nodiscard]]
    Local_Source_Span get_name_span() const;
    [[nodiscard]]
    std::string_view get_name(std::string_view source) const
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

struct Directive final : detail::Base {
private:
    std::size_t m_name_length;

    std::pmr::vector<Argument> m_arguments;
    std::pmr::vector<Content> m_content;

public:
    [[nodiscard]]
    Directive(
        const Local_Source_Span& pos,
        std::size_t name_length,
        std::pmr::vector<Argument>&& args,
        std::pmr::vector<Content>&& block
    );

    ~Directive();

    [[nodiscard]]
    std::string_view get_name(std::string_view source) const
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

struct Text final : detail::Base {

    [[nodiscard]]
    Text(const Local_Source_Span& pos);

    [[nodiscard]]
    std::string_view get_text(std::string_view source) const
    {
        return source.substr(m_pos.begin, m_pos.length);
    }
};

/// @brief An escape sequence, such as `\\{`, `\\}`, or `\\\\`.
struct Escaped final : detail::Base {

    [[nodiscard]]
    Escaped(const Local_Source_Span& pos);

    /// @brief Returns the escaped character.
    [[nodiscard]]
    char get_char(std::string_view source) const
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
    std::string_view get_text(std::string_view source) const
    {
        return source.substr(m_pos.begin, m_pos.length);
    }
};

using Content_Variant = std::variant<Directive, Text, Escaped>;

struct Content : Content_Variant {
    using Content_Variant::variant;
};

inline Argument::~Argument() = default;
inline Directive::~Directive() = default;

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

[[nodiscard]]
inline Local_Source_Span get_source_span(const Content& node)
{
    return visit([]<typename T>(const T& v) -> const detail::Base& { return v; }, node)
        .get_source_position();
}

[[nodiscard]]
inline std::string_view get_source(const Content& node, std::string_view source)
{
    return visit([]<typename T>(const T& v) -> const detail::Base& { return v; }, node)
        .get_source(source);
}

template <bool constant>
struct Visitor_Impl {
    using Argument_Type = const_if_t<Argument, constant>;
    using Text_Type = const_if_t<Text, constant>;
    using Escaped_Type = const_if_t<Escaped, constant>;
    using Directive_Type = const_if_t<Directive, constant>;
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

    virtual void visit(Directive_Type& directive) = 0;
    virtual void visit(Text_Type& text) = 0;
    virtual void visit(Escaped_Type& text) = 0;
    virtual void visit(Argument_Type& argument) = 0;
};

using Visitor = Visitor_Impl<false>;
using Const_Visitor = Visitor_Impl<true>;

} // namespace mmml::ast

#endif
