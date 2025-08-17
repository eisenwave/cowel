#ifndef COWEL_AST_HPP
#define COWEL_AST_HPP

#include <concepts>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/meta.hpp"
#include "cowel/util/source_position.hpp"

#include "cowel/fwd.hpp"
#include "cowel/memory_resources.hpp"
#include "cowel/output_language.hpp"

namespace cowel::ast {
namespace detail {

using Suppress_Unused_Include_Source_Position_2 = Basic_File_Source_Span<void>;

}

template <typename T>
using Pmr_Vector = std::vector<T, Propagated_Polymorphic_Allocator<T>>;

struct Content_Sequence {
private:
    File_Source_Span m_source_span;
    std::u8string_view m_source;
    Pmr_Vector<Content> m_elements;

public:
    [[nodiscard]]
    explicit Content_Sequence(
        File_Source_Span source_span,
        std::u8string_view source,
        Pmr_Vector<Content>&& elements
    );
    [[nodiscard]]
    explicit Content_Sequence(File_Source_Span source_span, std::u8string_view source);
    [[nodiscard]]
    Content_Sequence(const Content_Sequence&);
    [[nodiscard]]
    Content_Sequence(Content_Sequence&&) noexcept;

    Content_Sequence& operator=(const Content_Sequence&);
    Content_Sequence& operator=(Content_Sequence&&) noexcept;

    ~Content_Sequence();

    [[nodiscard]]
    File_Source_Span get_source_span() const
    {
        return m_source_span;
    }

    [[nodiscard]]
    std::u8string_view get_source() const
    {
        return m_source;
    }

    [[nodiscard]]
    std::span<const Content> get_elements() const;

    [[nodiscard]]
    bool empty() const
    {
        return get_elements().empty();
    }

    [[nodiscard]]
    std::size_t size() const
    {
        return get_elements().size();
    }
};

struct Group final {
private:
    File_Source_Span m_source_span;
    std::u8string_view m_source;
    Pmr_Vector<Group_Member> m_members;

public:
    [[nodiscard]]
    explicit Group(File_Source_Span source_span, std::u8string_view source);
    [[nodiscard]]
    explicit Group(
        File_Source_Span source_span,
        std::u8string_view source,
        Pmr_Vector<Group_Member>&& members
    );

    [[nodiscard]]
    Group(const Group&);
    [[nodiscard]]
    Group(Group&&) noexcept;

    Group& operator=(const Group&);
    Group& operator=(Group&&) noexcept;

    ~Group();

    [[nodiscard]]
    File_Source_Span get_source_span() const
    {
        return m_source_span;
    }

    [[nodiscard]]
    std::u8string_view get_source() const
    {
        return m_source;
    }

    [[nodiscard]]
    std::span<const Group_Member> get_members() const;

    [[nodiscard]]
    bool empty() const
    {
        return get_members().empty();
    }

    [[nodiscard]]
    std::size_t size() const
    {
        return get_members().size();
    }
};

using Value_Variant = std::variant<Content_Sequence, Group>;

struct Value : Value_Variant {
    using Value_Variant::variant;

    [[nodiscard]]
    File_Source_Span get_source_span() const
    {
        if (const auto* const group = std::get_if<ast::Group>(this)) {
            return group->get_source_span();
        }
        return std::get<ast::Content_Sequence>(*this).get_source_span();
    }

    [[nodiscard]]
    std::u8string_view get_source() const
    {
        if (const auto* const group = std::get_if<ast::Group>(this)) {
            return group->get_source();
        }
        return std::get<ast::Content_Sequence>(*this).get_source();
    }
};

enum struct Member_Kind : Default_Underlying {
    named,
    positional,
    ellipsis,
};

struct [[nodiscard]]
Group_Member final {
public:
    [[nodiscard]]
    static Group_Member ellipsis( //
        File_Source_Span source_span,
        std::u8string_view source
    );
    [[nodiscard]]
    static Group_Member named( //
        const File_Source_Span& name_span,
        std::u8string_view name,
        Value&& value
    );
    [[nodiscard]]
    static Group_Member positional( //
        Value&& value
    );

private:
    File_Source_Span m_source_span;
    std::u8string_view m_source;
    Value m_value;
    File_Source_Span m_name_span;
    std::u8string_view m_name;
    Member_Kind m_kind;

    [[nodiscard]]
    Group_Member(
        const File_Source_Span& source_span,
        std::u8string_view source,
        const File_Source_Span& name_span,
        std::u8string_view name,
        Value&& value,
        Member_Kind type
    );

public:
    [[nodiscard]]
    Group_Member(Group_Member&&) noexcept;
    [[nodiscard]]
    Group_Member(const Group_Member&);

    Group_Member& operator=(Group_Member&&) noexcept;
    Group_Member& operator=(const Group_Member&);

    ~Group_Member();

    [[nodiscard]]
    Member_Kind get_kind() const
    {
        return m_kind;
    }

    [[nodiscard]]
    File_Source_Span get_source_span() const
    {
        return m_source_span;
    }

    [[nodiscard]]
    std::u8string_view get_source() const
    {
        return m_source;
    }

    [[nodiscard]]
    File_Source_Span get_name_span() const
    {
        COWEL_ASSERT(m_kind == Member_Kind::named);
        return m_name_span;
    }

    [[nodiscard]]
    std::u8string_view get_name() const
    {
        COWEL_ASSERT(m_kind == Member_Kind::named);
        return m_name;
    }

    [[nodiscard]]
    bool has_value() const
    {
        return m_kind != Member_Kind::ellipsis;
    }

    [[nodiscard]]
    const Value& get_value() const
    {
        COWEL_ASSERT(has_value());
        return m_value;
    }
};

struct Directive final {
private:
    File_Source_Span m_source_span;
    std::u8string_view m_source;
    std::u8string_view m_name;
    bool m_has_ellipsis = false;

    std::optional<Group> m_arguments;
    std::optional<Content_Sequence> m_content;

public:
    [[nodiscard]]
    Directive(
        const File_Source_Span& source_span,
        std::u8string_view source,
        std::u8string_view name,
        std::optional<Group>&& args,
        std::optional<Content_Sequence>&& content
    );

    [[nodiscard]]
    Directive(Directive&&) noexcept;
    [[nodiscard]]
    Directive(const Directive&);

    Directive& operator=(Directive&&) noexcept;
    Directive& operator=(const Directive&);

    ~Directive();

    [[nodiscard]]
    bool has_ellipsis() const
    {
        return m_has_ellipsis;
    }

    [[nodiscard]]
    File_Source_Span get_source_span() const
    {
        return m_source_span;
    }

    [[nodiscard]]
    std::u8string_view get_source() const
    {
        return m_source;
    }

    [[nodiscard]]
    File_Source_Span get_name_span() const
    {
        return m_source_span.with_length(m_name.length());
    }

    [[nodiscard]]
    std::u8string_view get_name() const
    {
        return m_name;
    }

    [[nodiscard]]
    Group* get_arguments()
    {
        return m_arguments ? &*m_arguments : nullptr;
    }
    [[nodiscard]]
    const Group* get_arguments() const
    {
        return m_arguments ? &*m_arguments : nullptr;
    }

    [[nodiscard]]
    std::span<const ast::Group_Member> get_argument_span() const
    {
        if (m_arguments) {
            return m_arguments->get_members();
        }
        return {};
    }

    [[nodiscard]]
    Content_Sequence* get_content()
    {
        return m_content ? &*m_content : nullptr;
    }
    [[nodiscard]]
    const Content_Sequence* get_content() const
    {
        return m_content ? &*m_content : nullptr;
    }

    [[nodiscard]]
    std::span<const ast::Content> get_content_span() const
    {
        if (m_content) {
            return m_content->get_elements();
        }
        return {};
    }
};

struct Text final {
private:
    File_Source_Span m_source_span;
    std::u8string_view m_source;

public:
    [[nodiscard]]
    Text(const File_Source_Span& source_span, std::u8string_view source);

    [[nodiscard]]
    File_Source_Span get_source_span() const
    {
        return m_source_span;
    }

    [[nodiscard]]
    std::u8string_view get_source() const
    {
        return m_source;
    }
};

struct Comment final {
private:
    File_Source_Span m_source_span;
    std::u8string_view m_source;
    std::size_t m_suffix_length;

public:
    [[nodiscard]]
    Comment(const File_Source_Span& source_span, std::u8string_view source);

    [[nodiscard]]
    File_Source_Span get_source_span() const
    {
        return m_source_span;
    }

    /// @brief Returns a string containing all the source characters comprising the comment,
    /// including the `\:` prefix and the LF/CRLF suffix, if any.
    [[nodiscard]]
    std::u8string_view get_source() const
    {
        return m_source;
    }

    [[nodiscard]]
    std::size_t get_suffix_length() const
    {
        return m_suffix_length;
    }

    /// @brief Returns the suffix of the comment.
    /// That is, an empty string (if the comment ends with EOF),
    /// or a string containing the terminating LF/CRLF.
    [[nodiscard]]
    std::u8string_view get_suffix() const
    {
        return m_source.substr(m_source.length() - m_suffix_length);
    }

    /// @brief Returns the text content of the comment, excluding the prefix and suffix.
    [[nodiscard]]
    std::u8string_view get_text() const
    {
        constexpr std::size_t prefix_length = 2; // \:
        return m_source.substr(prefix_length, m_source.length() - prefix_length - m_suffix_length);
    }
};

/// @brief An escape sequence, such as `\\{`, `\\}`, or `\\\\`.
struct Escaped final {
private:
    File_Source_Span m_source_span;
    std::u8string_view m_source;

public:
    [[nodiscard]]
    Escaped(const File_Source_Span& source_span, std::u8string_view source);

    [[nodiscard]]
    File_Source_Span get_source_span() const
    {
        return m_source_span;
    }

    /// @brief Returns a two-character substring of the `source`,
    /// where the first character is the escaping backslash,
    /// and the second character is the escaped character.
    [[nodiscard]]
    std::u8string_view get_source() const
    {
        return m_source;
    }

    /// @brief Returns the source span covering the escaped characters.
    [[nodiscard]]
    File_Source_Span get_escaped_span() const
    {
        return m_source_span.to_right(1);
    }

    /// @brief Returns the escaped characters.
    [[nodiscard]]
    std::u8string_view get_escaped() const
    {
        COWEL_DEBUG_ASSERT(m_source.size() >= 2);
        return m_source.substr(1);
    }
};

struct Generated final {
private:
    Pmr_Vector<char8_t> m_data;
    Output_Language m_type;

public:
    [[nodiscard]]
    explicit Generated(Pmr_Vector<char8_t>&& data, Output_Language type)
        : m_data { std::move(data) }
        , m_type { type }
    {
    }

    [[nodiscard]]
    constexpr Output_Language get_type() const
    {
        return m_type;
    }

    [[nodiscard]]
    constexpr std::span<char8_t> as_span()
    {
        return m_data;
    }

    [[nodiscard]]
    constexpr std::span<const char8_t> as_span() const
    {
        return m_data;
    }

    [[nodiscard]]
    constexpr std::u8string_view as_string() const
    {
        return { m_data.data(), m_data.size() };
    }

    [[nodiscard]]
    constexpr std::size_t size() const
    {
        return m_data.size();
    }

    [[nodiscard]]
    constexpr bool emtpy() const
    {
        return m_data.empty();
    }
};

using Content_Variant = std::variant<Directive, Text, Comment, Escaped, Generated>;

template <typename T>
concept content_variant_alternative = []<typename... Ts>(std::variant<Ts...>*) {
    return one_of<T, Ts...>;
}(static_cast<Content_Variant*>(nullptr));

template <typename T>
concept user_written = content_variant_alternative<T> && !std::same_as<T, Generated>;

struct Content : Content_Variant {
    using Content_Variant::variant;

    [[nodiscard]]
    File_Source_Span get_source_span() const
    {
        return visit(
            [&]<typename T>(const T& v) -> File_Source_Span {
                if constexpr (one_of<T, Text, Escaped, Directive>) {
                    return v.get_source_span();
                }
                else {
                    return { {}, File_Id { 0 } };
                }
            },
            *this
        );
    }

    [[nodiscard]]
    std::u8string_view get_source() const
    {
        return visit(
            []<typename T>(const T& v) -> std::u8string_view {
                if constexpr (user_written<T>) {
                    return v.get_source();
                }
                else {
                    return {};
                }
            },
            *this
        );
    }
};

static_assert(std::is_move_constructible_v<Content>);
static_assert(std::is_move_constructible_v<Content>);
static_assert(std::is_copy_assignable_v<Content>);
static_assert(std::is_move_assignable_v<Content>);

// Suppress false positive: https://github.com/llvm/llvm-project/issues/130745
// NOLINTBEGIN(readability-redundant-inline-specifier)
inline Group_Member::Group_Member(Group_Member&&) noexcept = default;
inline Group_Member::Group_Member(const Group_Member&) = default;
inline Group_Member& Group_Member::operator=(Group_Member&&) noexcept = default;
inline Group_Member& Group_Member::operator=(const Group_Member&) = default;
inline Group_Member::~Group_Member() = default;

inline Directive::Directive(Directive&&) noexcept = default;
inline Directive::Directive(const Directive&) = default;
inline Directive& Directive::operator=(Directive&&) noexcept = default;
inline Directive& Directive::operator=(const Directive&) = default;
inline Directive::~Directive() = default;

inline Group::Group(const Group&) = default;
inline Group::Group(Group&&) noexcept = default;
inline Group& Group::operator=(const Group&) = default;
inline Group& Group::operator=(Group&&) noexcept = default;
inline Group::~Group() = default;

inline Content_Sequence::Content_Sequence(const Content_Sequence&) = default;
inline Content_Sequence::Content_Sequence(Content_Sequence&&) noexcept = default;
inline Content_Sequence& Content_Sequence::operator=(const Content_Sequence&) = default;
inline Content_Sequence& Content_Sequence::operator=(Content_Sequence&&) noexcept = default;
inline Content_Sequence::~Content_Sequence() = default;

inline std::span<const Content> Content_Sequence::get_elements() const
{
    return m_elements;
}

inline std::span<const ast::Group_Member> Group::get_members() const
{
    return m_members;
}
// NOLINTEND(readability-redundant-inline-specifier)

template <bool constant>
struct Visitor_Impl {
    using Group_Type = const_if_t<Group, constant>;
    using Group_Member_Type = const_if_t<Group_Member, constant>;
    using Text_Type = const_if_t<Text, constant>;
    using Comment_Type = const_if_t<Comment, constant>;
    using Escaped_Type = const_if_t<Escaped, constant>;
    using Directive_Type = const_if_t<Directive, constant>;
    using Generated_Type = const_if_t<Generated, constant>;
    using Content_Type = const_if_t<Content, constant>;

    void visit_arguments(Directive_Type& directive)
    {
        if (Group_Type* const args = directive.get_arguments()) {
            visit_group_members(args->get_members());
        }
    }

    void visit_group_members(std::span<Group_Member_Type> members)
    {
        for (Group_Member_Type& arg : members) {
            this->visit(arg);
        }
    }

    void visit_children(Directive_Type& directive)
    {
        visit_arguments();
        visit_content_sequence(directive.get_content());
    }

    void visit_children(Group_Member_Type& argument)
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

    virtual void visit(Group_Member_Type& argument) = 0;
    virtual void visit(Directive_Type& directive) = 0;
    virtual void visit(Generated_Type& generated) = 0;
    virtual void visit(Text_Type& text) = 0;
    virtual void visit(Comment_Type& text) = 0;
    virtual void visit(Escaped_Type& text) = 0;
};

using Visitor = Visitor_Impl<false>;
using Const_Visitor = Visitor_Impl<true>;

} // namespace cowel::ast

#endif
