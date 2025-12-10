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

enum struct Primary_Kind : Default_Underlying {
    unit_literal,
    null_literal,
    bool_literal,
    int_literal,
    decimal_float_literal,
    infinity,
    unquoted_string,
    text,
    escape,
    comment,
    quoted_string,
    block,
    group,
};

/// @brief Returns `true` iff `kind` is a value.
/// That is, something that can be passed around within the COWEL's scripting sublanguage,
/// passed as arguments to directives, etc.
///
/// Notably, markup elements like `text` or `comment` are not values.
[[nodiscard]]
constexpr bool primary_kind_is_value(Primary_Kind kind)
{
    using enum Primary_Kind;
    switch (kind) {
    case unit_literal:
    case null_literal:
    case bool_literal:
    case int_literal:
    case decimal_float_literal:
    case infinity:
    case unquoted_string:
    case block:
    case quoted_string:
    case group: return true;

    case text:
    case escape:
    case comment: return false;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid kind.");
}

/// @brief Returns `true` iff `kind` can be spliced into markup.
[[nodiscard]]
constexpr bool primary_kind_is_spliceable(Primary_Kind kind)
{
    using enum Primary_Kind;
    switch (kind) {
    case unit_literal:
    case null_literal:
    case bool_literal:
    case int_literal:
    case decimal_float_literal:
    case infinity:
    case unquoted_string:
    case quoted_string:
    case block:
    case text:
    case escape:
    case comment: return true;

    case group: return false;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid kind.");
}

/// @brief Returns `true` iff `kind` is a value that can be spliced into markup.
[[nodiscard]]
constexpr bool primary_kind_is_spliceable_value(Primary_Kind kind)
{
    return primary_kind_is_value(kind) && primary_kind_is_spliceable(kind);
}

[[nodiscard]]
constexpr std::u8string_view primary_kind_display_name(Primary_Kind kind)
{
    using enum Primary_Kind;
    switch (kind) {
    case unit_literal: return u8"unit";
    case null_literal: return u8"null";
    case bool_literal: return u8"boolean literal";
    case int_literal: return u8"integer literal";
    case decimal_float_literal: return u8"floating-point literal";
    case infinity: return u8"infinity";
    case unquoted_string: return u8"unquoted string";
    case text: return u8"text";
    case escape: return u8"escape";
    case comment: return u8"comment";
    case quoted_string: return u8"quoted string";
    case block: return u8"block";
    case group: return u8"group";
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid kind.");
}

struct Primary {
public:
    [[nodiscard]]
    static Primary quoted_string(
        File_Source_Span source_span,
        std::u8string_view source,
        Pmr_Vector<Markup_Element>&& elements
    );

    [[nodiscard]]
    static Primary block(
        File_Source_Span source_span,
        std::u8string_view source,
        Pmr_Vector<Markup_Element>&& elements
    );

    [[nodiscard]]
    static Primary group(
        File_Source_Span source_span,
        std::u8string_view source,
        Pmr_Vector<Group_Member>&& members
    );

private:
    Primary_Kind m_kind;
    File_Source_Span m_source_span;
    std::u8string_view m_source;
    std::variant<std::size_t, Pmr_Vector<Markup_Element>, Pmr_Vector<Group_Member>> m_extra;

    [[nodiscard]]
    Primary(
        Primary_Kind kind,
        File_Source_Span source_span,
        std::u8string_view source,
        Pmr_Vector<Markup_Element>&& elements
    );

    [[nodiscard]]
    Primary(
        Primary_Kind kind,
        File_Source_Span source_span,
        std::u8string_view source,
        Pmr_Vector<Group_Member>&& members
    );

public:
    [[nodiscard]]
    Primary(Primary_Kind kind, File_Source_Span source_span, std::u8string_view source);

    [[nodiscard]]
    Primary(const Primary&);
    [[nodiscard]]
    Primary(Primary&&) noexcept;

    Primary& operator=(const Primary&);
    Primary& operator=(Primary&&) noexcept;

    ~Primary();

    [[nodiscard]]
    Primary_Kind get_kind() const
    {
        return m_kind;
    }

    [[nodiscard]]
    bool is_value() const
    {
        return primary_kind_is_value(m_kind);
    }
    [[nodiscard]]
    bool is_spliceable() const
    {
        return primary_kind_is_spliceable(m_kind);
    }
    [[nodiscard]]
    bool is_spliceable_value() const
    {
        return primary_kind_is_spliceable_value(m_kind);
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

    /// @brief Returns the source span covering the escaped characters.
    [[nodiscard]]
    File_Source_Span get_escaped_span() const
    {
        COWEL_ASSERT(m_kind == Primary_Kind::escape);
        return m_source_span.to_right(1);
    }

    /// @brief Returns the escaped characters.
    [[nodiscard]]
    std::u8string_view get_escaped() const
    {
        COWEL_ASSERT(m_kind == Primary_Kind::escape);
        COWEL_DEBUG_ASSERT(m_source.size() >= 2);
        return m_source.substr(1);
    }

    [[nodiscard]]
    std::size_t get_comment_suffix_length() const
    {
        COWEL_ASSERT(m_kind == Primary_Kind::comment);
        return std::get<std::size_t>(m_extra);
    }

    /// @brief Returns the suffix of the comment.
    /// That is, an empty string (if the comment ends with EOF),
    /// or a string containing the terminating LF/CRLF.
    [[nodiscard]]
    std::u8string_view get_comment_suffix() const
    {
        COWEL_ASSERT(m_kind == Primary_Kind::comment);
        return m_source.substr(m_source.length() - get_comment_suffix_length());
    }

    /// @brief Returns the text content of the comment, excluding the prefix and suffix.
    [[nodiscard]]
    std::u8string_view get_comment_text() const
    {
        COWEL_ASSERT(m_kind == Primary_Kind::comment);
        constexpr std::size_t prefix_length = 2; // \:
        return m_source.substr(
            prefix_length, m_source.length() - prefix_length - get_comment_suffix_length()
        );
    }

    [[nodiscard]]
    std::span<const Markup_Element> get_elements() const;

    [[nodiscard]]
    bool has_elements() const
    {
        COWEL_ASSERT(m_kind == Primary_Kind::block || m_kind == Primary_Kind::quoted_string);
        return !get_elements().empty();
    }

    [[nodiscard]]
    std::size_t get_elements_size() const
    {
        COWEL_ASSERT(m_kind == Primary_Kind::block || m_kind == Primary_Kind::quoted_string);
        return get_elements().size();
    }

    [[nodiscard]]
    std::span<const Group_Member> get_members() const;

    [[nodiscard]]
    bool has_members() const
    {
        COWEL_ASSERT(m_kind == Primary_Kind::group);
        return !get_members().empty();
    }

    [[nodiscard]]
    std::size_t get_members_size() const
    {
        COWEL_ASSERT(m_kind == Primary_Kind::group);
        return get_members().size();
    }

    void swap(Primary& other) noexcept;

private:
    void assert_validity() const;
};

enum struct Member_Kind : Default_Underlying {
    named,
    positional,
    ellipsis,
};

struct Directive final {
private:
    File_Source_Span m_source_span;
    std::u8string_view m_source;
    std::u8string_view m_name;
    bool m_has_ellipsis = false;

    std::optional<Primary> m_arguments;
    std::optional<Primary> m_content;

public:
    [[nodiscard]]
    Directive(
        const File_Source_Span& source_span,
        std::u8string_view source,
        std::u8string_view name,
        std::optional<Primary>&& args,
        std::optional<Primary>&& content
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

    /// @brief Returns the source code of this directive.
    /// This may include a leading backslash.
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

    /// @brief Returns the name of the directive,
    /// not including the leading backslash.
    [[nodiscard]]
    std::u8string_view get_name() const
    {
        return m_name;
    }

    [[nodiscard]]
    Primary* get_arguments()
    {
        return m_arguments ? &*m_arguments : nullptr;
    }
    [[nodiscard]]
    const Primary* get_arguments() const
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
    Primary* get_content()
    {
        return m_content ? &*m_content : nullptr;
    }
    [[nodiscard]]
    const Primary* get_content() const
    {
        return m_content ? &*m_content : nullptr;
    }

    [[nodiscard]]
    std::span<const ast::Markup_Element> get_content_span() const
    {
        if (m_content) {
            return m_content->get_elements();
        }
        return {};
    }
};

using Member_Value_Variant = std::variant<Directive, Primary>;

struct Member_Value : Member_Value_Variant {
    using std::variant<Directive, Primary>::variant;

    [[nodiscard]]
    bool is_directive() const
    {
        return std::holds_alternative<ast::Directive>(*this);
    }
    [[nodiscard]]
    bool is_primary() const
    {
        return std::holds_alternative<ast::Primary>(*this);
    }

    [[nodiscard]]
    const ast::Directive& as_directive() const
    {
        return std::get<ast::Directive>(*this);
    }
    [[nodiscard]]
    const ast::Directive* try_as_directive() const
    {
        return std::get_if<ast::Directive>(this);
    }

    [[nodiscard]]
    const ast::Primary& as_primary() const
    {
        return std::get<ast::Primary>(*this);
    }
    [[nodiscard]]
    const ast::Primary* try_as_primary() const
    {
        return std::get_if<ast::Primary>(this);
    }

    [[nodiscard]]
    bool is_spliceable_value() const noexcept
    {
        // FIXME: This doesn't seem correct;
        //        directives can return `void` or `group`,
        //        and aren't necessarily spliceable.
        return is_directive() || as_primary().is_spliceable_value();
    }

    [[nodiscard]]
    bool is_spliceable() const noexcept
    {
        // FIXME: This doesn't seem correct;
        //        directives can return `void` or `group`,
        //        and aren't necessarily spliceable.
        return is_directive() || as_primary().is_spliceable();
    }

    [[nodiscard]]
    bool is_value() const noexcept
    {
        return is_directive() || as_primary().is_value();
    }

    [[nodiscard]]
    File_Source_Span get_source_span() const
    {
        return std::visit(
            [&](const auto& v) -> File_Source_Span { return v.get_source_span(); }, *this
        );
    }

    [[nodiscard]]
    std::u8string_view get_source() const
    {
        return std::visit(
            [&](const auto& v) -> std::u8string_view { return v.get_source(); }, *this
        );
    }
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
        Member_Value&& value
    );
    [[nodiscard]]
    static Group_Member positional( //
        Member_Value&& value
    );

private:
    File_Source_Span m_source_span;
    std::u8string_view m_source;
    std::optional<Member_Value> m_value;
    File_Source_Span m_name_span;
    std::u8string_view m_name;
    Member_Kind m_kind;

    [[nodiscard]]
    Group_Member(
        const File_Source_Span& source_span,
        std::u8string_view source,
        const File_Source_Span& name_span,
        std::u8string_view name,
        std::optional<Member_Value>&& value,
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
        return m_value.has_value();
    }

    [[nodiscard]]
    const Member_Value& get_value() const
    {
        return m_value.value();
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

using Markup_Element_Variant = std::variant<Directive, Primary, Generated>;

template <typename T>
concept content_variant_alternative = []<typename... Ts>(std::variant<Ts...>*) {
    return one_of<T, Ts...>;
}(static_cast<Markup_Element_Variant*>(nullptr));

template <typename T>
concept user_written = content_variant_alternative<T> && !std::same_as<T, Generated>;

struct Markup_Element : Markup_Element_Variant {
    using Markup_Element_Variant::variant;

    [[nodiscard]]
    const ast::Directive& as_directive() const
    {
        return std::get<ast::Directive>(*this);
    }
    [[nodiscard]]
    const ast::Directive* try_as_directive() const
    {
        return std::get_if<ast::Directive>(this);
    }

    [[nodiscard]]
    const ast::Primary& as_primary() const
    {
        return std::get<ast::Primary>(*this);
    }
    [[nodiscard]]
    const ast::Primary* try_as_primary() const
    {
        return std::get_if<ast::Primary>(this);
    }

    [[nodiscard]]
    const ast::Generated& as_generated() const
    {
        return std::get<ast::Generated>(*this);
    }
    [[nodiscard]]
    const ast::Generated* try_as_generated() const
    {
        return std::get_if<ast::Generated>(this);
    }

    [[nodiscard]]
    File_Source_Span get_source_span() const
    {
        return std::visit(
            [&]<typename T>(const T& v) -> File_Source_Span {
                if constexpr (user_written<T>) {
                    return v.get_source_span();
                }
                else {
                    return { {}, File_Id::main };
                }
            },
            *this
        );
    }

    [[nodiscard]]
    std::u8string_view get_source() const
    {
        return std::visit(
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

static_assert(std::is_move_constructible_v<Markup_Element>);
static_assert(std::is_move_constructible_v<Markup_Element>);
static_assert(std::is_copy_assignable_v<Markup_Element>);
static_assert(std::is_move_assignable_v<Markup_Element>);

// Suppress false positive: https://github.com/llvm/llvm-project/issues/130745
// NOLINTBEGIN(readability-redundant-inline-specifier)
inline Primary::Primary(Primary&&) noexcept = default;
inline Primary::Primary(const Primary&) = default;
inline Primary& Primary::operator=(Primary&&) noexcept = default;
inline Primary& Primary::operator=(const Primary&) = default;
inline Primary::~Primary() = default;

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

inline std::span<const Markup_Element> Primary::get_elements() const
{
    COWEL_DEBUG_ASSERT(m_kind == Primary_Kind::block || m_kind == Primary_Kind::quoted_string);
    return std::get<Pmr_Vector<Markup_Element>>(m_extra);
}

inline std::span<const Group_Member> Primary::get_members() const
{
    COWEL_DEBUG_ASSERT(m_kind == Primary_Kind::group);
    return std::get<Pmr_Vector<Group_Member>>(m_extra);
}
// NOLINTEND(readability-redundant-inline-specifier)

} // namespace cowel::ast

#endif
