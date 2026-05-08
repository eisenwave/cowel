#ifndef COWEL_AST_HPP
#define COWEL_AST_HPP

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/fixed_string.hpp"
#include "cowel/util/meta.hpp"
#include "cowel/util/source_position.hpp"

#include "cowel/big_int.hpp"
#include "cowel/fwd.hpp"
#include "cowel/gc.hpp"
#include "cowel/memory_resources.hpp"
#include "cowel/string_kind.hpp"

#include "cowel/syntax/ast_fwd.hpp"
#include "cowel/syntax/expression_kind.hpp"

namespace cowel::ast {

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
    quoted_string,
    block,
    group,
    text,
    escape,
    comment,
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

enum struct Float_Literal_Status : Default_Underlying {
    /// @brief `value` holds the (possibly rounded) value.
    ok,
    /// @brief Floating-point overflow.
    /// `value` holds correctly signed infinity.
    float_overflow,
    /// @brief Floating-point underflow.
    /// `value` holds correctly signed zero.
    float_underflow,
};

struct Parsed_Float {
    Float value;
    Float_Literal_Status status;
};

/// @brief Represents a *primary-expression* or other terminal syntactical construct,
/// like *block-comment* in markup.
struct Primary {
private:
    using Extra_Variant = std::variant<
        std::monostate, // No extra.
        std::size_t, // Length of comment suffix.
        Fixed_String8<4>, // UTF-8 code units for escape sequences.
        Big_Int, // Parsed int value.
        Parsed_Float, // Parsed float value.
        Pmr_Vector<Markup_Element>, // Markup elements of block or quoted string.
        Pmr_Vector<Group_Member> // Members of a group.
        >;

    [[nodiscard]]
    static Primary integer(File_Source_Span source_span, std::u8string_view source);

    [[nodiscard]]
    static Primary floating(File_Source_Span source_span, std::u8string_view source);

public:
    [[nodiscard]]
    static Primary
    basic(Primary_Kind kind, File_Source_Span source_span, std::u8string_view source);

    /// @brief Constructs an escape primary with pre-encoded UTF-8 code units.
    /// @param code_point The code point from lexing, or `char32_t(-1)` for whitespace escapes.
    [[nodiscard]]
    static Primary
    escape(File_Source_Span source_span, std::u8string_view source, char32_t code_point);

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
    String_Kind m_string_kind;
    File_Source_Span m_source_span;
    std::u8string_view m_source;
    Extra_Variant m_extra;

    [[nodiscard]]
    Primary(
        Primary_Kind kind,
        File_Source_Span source_span,
        std::u8string_view source,
        Extra_Variant&& extra,
        String_Kind string_kind = String_Kind::unknown
    );

public:
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
    String_Kind get_string_kind() const
    {
        return m_string_kind;
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
    const File_Source_Span& get_source_span() const
    {
        return m_source_span;
    }

    [[nodiscard]]
    std::u8string_view get_source() const
    {
        return m_source;
    }

    [[nodiscard]]
    bool get_bool_value() const
    {
        COWEL_ASSERT(m_kind == Primary_Kind::bool_literal);
        return m_source == u8"true";
    }

    [[nodiscard]]
    const Big_Int& get_int_value() const
    {
        COWEL_ASSERT(m_kind == Primary_Kind::int_literal);
        return std::get<Big_Int>(m_extra);
    }

    [[nodiscard]]
    Parsed_Float get_float_value() const
    {
        return std::get<Parsed_Float>(m_extra);
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

    /// @brief Returns the pre-encoded UTF-8 code units corresponding to this escape.
    /// For LF and CRLF escapes, this is empty.
    [[nodiscard]]
    std::u8string_view get_escaped_code_units() const
    {
        COWEL_ASSERT(m_kind == Primary_Kind::escape);
        return std::get<Fixed_String8<4>>(m_extra).as_string();
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

static_assert(std::is_copy_constructible_v<Primary>);
static_assert(std::is_move_constructible_v<Primary>);
static_assert(std::is_copy_assignable_v<Primary>);
static_assert(std::is_move_assignable_v<Primary>);

enum struct Member_Kind : Default_Underlying {
    named,
    positional,
    ellipsis,
};

/// @brief Represents a *directive-splice* or *directive-call*.
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
    const File_Source_Span& get_source_span() const
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
    std::span<const ast::Group_Member> get_argument_span() const;

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
    std::span<const ast::Markup_Element> get_content_span() const;
};

static_assert(std::is_copy_constructible_v<Directive>);
static_assert(std::is_move_constructible_v<Directive>);
static_assert(std::is_copy_assignable_v<Directive>);
static_assert(std::is_move_assignable_v<Directive>);

/// @brief Represents a *unary-expression*.
struct Unary_Expression {
private:
    GC_Ref<Expression> m_operand;
    Unary_Expression_Kind m_kind;
    File_Source_Span m_source_span;
    std::u8string_view m_source;

public:
    [[nodiscard]]
    Unary_Expression(const Unary_Expression&);
    [[nodiscard]]
    Unary_Expression(Unary_Expression&&) noexcept;
    Unary_Expression(
        GC_Ref<Expression> operand,
        Unary_Expression_Kind kind,
        File_Source_Span source_span,
        std::u8string_view source
    );

    Unary_Expression& operator=(const Unary_Expression&);
    Unary_Expression& operator=(Unary_Expression&&) noexcept;
    ~Unary_Expression();

    [[nodiscard]]
    const Expression& get_operand() const
    {
        return *m_operand;
    }

    [[nodiscard]]
    Unary_Expression_Kind get_kind() const
    {
        return m_kind;
    }

    [[nodiscard]]
    const File_Source_Span& get_source_span() const
    {
        return m_source_span;
    }

    [[nodiscard]]
    std::u8string_view get_source() const
    {
        return m_source;
    }
};

static_assert(std::is_copy_constructible_v<Unary_Expression>);
static_assert(std::is_move_constructible_v<Unary_Expression>);
static_assert(std::is_copy_assignable_v<Unary_Expression>);
static_assert(std::is_move_assignable_v<Unary_Expression>);

/// @brief Represents a *binary-expression*.
struct Binary_Expression {
private:
    GC_Ref<Expression> m_lhs;
    GC_Ref<Expression> m_rhs;
    Binary_Expression_Kind m_kind;
    File_Source_Span m_source_span;
    std::u8string_view m_source;

public:
    [[nodiscard]]
    Binary_Expression(const Binary_Expression&);
    [[nodiscard]]
    Binary_Expression(Binary_Expression&&) noexcept;
    Binary_Expression(
        GC_Ref<Expression> lhs,
        GC_Ref<Expression> rhs,
        Binary_Expression_Kind kind,
        File_Source_Span source_span,
        std::u8string_view source
    );

    Binary_Expression& operator=(const Binary_Expression&);
    Binary_Expression& operator=(Binary_Expression&&) noexcept;
    ~Binary_Expression();

    [[nodiscard]]
    const Expression& get_lhs() const
    {
        return *m_lhs;
    }
    [[nodiscard]]
    const Expression& get_rhs() const
    {
        return *m_rhs;
    }

    [[nodiscard]]
    Binary_Expression_Kind get_kind() const
    {
        return m_kind;
    }

    [[nodiscard]]
    const File_Source_Span& get_source_span() const
    {
        return m_source_span;
    }

    [[nodiscard]]
    std::u8string_view get_source() const
    {
        return m_source;
    }
};

static_assert(std::is_copy_constructible_v<Binary_Expression>);
static_assert(std::is_move_constructible_v<Binary_Expression>);
static_assert(std::is_copy_assignable_v<Binary_Expression>);
static_assert(std::is_move_assignable_v<Binary_Expression>);

using Expression_Variant = std::variant<Directive, Primary, Unary_Expression, Binary_Expression>;

/// @brief Represents an *expression* or *expression-splice*.
struct Expression : Expression_Variant {
private:
    File_Source_Span m_source_span;
    std::u8string_view m_source;

public:
    // Because we also need the constructors to copy the `m_source_span` member,
    // we cannot simply inherit all the `Expression_Variant` constructors.
    // We need additional members for the source and source span because
    // an expression-splice does not have the same source as the expression nested within.

    [[nodiscard]]
    Expression(const Directive& value)
        : Expression_Variant { value }
        , m_source_span { value.get_source_span() }
        , m_source { value.get_source() }
    {
    }
    [[nodiscard]]
    Expression(Directive&& value)
        : Expression_Variant { std::move(value) }
        , m_source_span { std::get<Directive>(*this).get_source_span() }
        , m_source { std::get<Directive>(*this).get_source() }
    {
    }

    [[nodiscard]]
    Expression(const Primary& value)
        : Expression_Variant { value }
        , m_source_span { value.get_source_span() }
        , m_source { value.get_source() }
    {
    }
    [[nodiscard]]
    Expression(Primary&& value)
        : Expression_Variant { std::move(value) }
        , m_source_span { std::get<Primary>(*this).get_source_span() }
        , m_source { std::get<Primary>(*this).get_source() }
    {
    }

    [[nodiscard]]
    Expression(const Unary_Expression& value)
        : Expression_Variant { value }
        , m_source_span { value.get_source_span() }
        , m_source { value.get_source() }
    {
    }
    [[nodiscard]]
    Expression(Unary_Expression&& value)
        : Expression_Variant { std::move(value) }
        , m_source_span { std::get<Unary_Expression>(*this).get_source_span() }
        , m_source { std::get<Unary_Expression>(*this).get_source() }
    {
    }

    [[nodiscard]]
    Expression(const Binary_Expression& value)
        : Expression_Variant { value }
        , m_source_span { value.get_source_span() }
        , m_source { value.get_source() }
    {
    }
    [[nodiscard]]
    Expression(Binary_Expression&& value)
        : Expression_Variant { std::move(value) }
        , m_source_span { std::get<Binary_Expression>(*this).get_source_span() }
        , m_source { std::get<Binary_Expression>(*this).get_source() }
    {
    }

    [[nodiscard]]
    Expression(
        Expression&& value,
        const File_Source_Span source_span,
        const std::u8string_view source
    )
        : Expression_Variant { std::move(value) }
        , m_source_span { source_span }
        , m_source { source }
    {
        COWEL_ASSERT(m_source_span.length == m_source.length());
    }

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
    bool is_unary() const
    {
        return std::holds_alternative<ast::Unary_Expression>(*this);
    }
    [[nodiscard]]
    bool is_binary() const
    {
        return std::holds_alternative<ast::Binary_Expression>(*this);
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
    const ast::Unary_Expression& as_unary() const
    {
        return std::get<ast::Unary_Expression>(*this);
    }
    [[nodiscard]]
    const ast::Unary_Expression* try_as_unary() const
    {
        return std::get_if<ast::Unary_Expression>(this);
    }

    [[nodiscard]]
    const ast::Binary_Expression& as_binary() const
    {
        return std::get<ast::Binary_Expression>(*this);
    }
    [[nodiscard]]
    const ast::Binary_Expression* try_as_binary() const
    {
        return std::get_if<ast::Binary_Expression>(this);
    }

    [[nodiscard]]
    bool is_value() const noexcept
    {
        return is_directive() //
            || (is_primary() && as_primary().is_value())
            || (is_unary() && as_unary().get_operand().is_value())
            || (is_binary() && as_binary().get_lhs().is_value() && as_binary().get_rhs().is_value()
            );
    }

    [[nodiscard]]
    const File_Source_Span& get_source_span() const
    {
        return m_source_span;
    }

    [[nodiscard]]
    std::u8string_view get_source() const
    {
        return m_source;
    }
};

static_assert(std::is_copy_constructible_v<Expression>);
static_assert(std::is_move_constructible_v<Expression>);
static_assert(std::is_copy_assignable_v<Expression>);
static_assert(std::is_move_assignable_v<Expression>);

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
        Primary&& name,
        Expression&& value
    );
    [[nodiscard]]
    static Group_Member positional( //
        Expression&& value
    );

private:
    File_Source_Span m_source_span;
    std::u8string_view m_source;
    GC_Ref<Primary> m_name;
    GC_Ref<Expression> m_value;
    Member_Kind m_kind;

    [[nodiscard]]
    Group_Member(
        const File_Source_Span& source_span,
        std::u8string_view source,
        GC_Ref<Primary> name,
        GC_Ref<Expression> value,
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
    const File_Source_Span& get_source_span() const
    {
        return m_source_span;
    }

    [[nodiscard]]
    std::u8string_view get_source() const
    {
        return m_source;
    }

    [[nodiscard]]
    bool has_name() const
    {
        return bool(m_name);
    }

    [[nodiscard]]
    const Primary& get_name() const
    {
        COWEL_ASSERT(m_kind == Member_Kind::named);
        return *m_name;
    }
    [[nodiscard]]
    GC_Ref<const Primary> get_name_ref() const
    {
        return m_name;
    }

    [[nodiscard]]
    File_Source_Span get_name_span() const
    {
        return get_name().get_source_span();
    }

    [[nodiscard]]
    bool has_value() const
    {
        return bool(m_value);
    }

    [[nodiscard]]
    const Expression& get_value() const
    {
        return *m_value;
    }
    [[nodiscard]]
    GC_Ref<const Expression> get_value_ref() const
    {
        return m_value;
    }

    [[nodiscard]]
    File_Source_Span get_value_span() const
    {
        return get_value().get_source_span();
    }
};

static_assert(std::is_copy_constructible_v<Group_Member>);
static_assert(std::is_move_constructible_v<Group_Member>);
static_assert(std::is_copy_assignable_v<Group_Member>);
static_assert(std::is_move_assignable_v<Group_Member>);

using Markup_Element_Variant = std::variant<Directive, Primary, Expression>;

template <typename T>
concept content_variant_alternative = []<typename... Ts>(std::variant<Ts...>*) {
    return one_of<T, Ts...>;
}(static_cast<Markup_Element_Variant*>(nullptr));

struct Markup_Element : Markup_Element_Variant {
    using Markup_Element_Variant::variant;

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
    bool is_expression() const
    {
        return std::holds_alternative<ast::Expression>(*this);
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
    const ast::Expression& as_expression() const
    {
        const auto* const result = try_as_expression();
        COWEL_ASSERT(result);
        return *result;
    }

    [[nodiscard]]
    const ast::Expression* try_as_expression() const
    {
        return std::get_if<ast::Expression>(this);
    }

    [[nodiscard]]
    File_Source_Span get_source_span() const
    {
        return std::visit(
            [&]<typename T>(const T& v) -> File_Source_Span { return v.get_source_span(); }, *this
        );
    }

    [[nodiscard]]
    std::u8string_view get_source() const
    {
        return std::visit(
            []<typename T>(const T& v) -> std::u8string_view { return v.get_source(); }, *this
        );
    }
};

static_assert(std::is_copy_constructible_v<Markup_Element>);
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

inline Unary_Expression::Unary_Expression(const Unary_Expression&) = default;
inline Unary_Expression::Unary_Expression(Unary_Expression&&) noexcept = default;
inline Unary_Expression& Unary_Expression::operator=(const Unary_Expression&) = default;
inline Unary_Expression& Unary_Expression::operator=(Unary_Expression&&) noexcept = default;
inline Unary_Expression::~Unary_Expression() = default;

inline Binary_Expression::Binary_Expression(const Binary_Expression&) = default;
inline Binary_Expression::Binary_Expression(Binary_Expression&&) noexcept = default;
inline Binary_Expression& Binary_Expression::operator=(const Binary_Expression&) = default;
inline Binary_Expression& Binary_Expression::operator=(Binary_Expression&&) noexcept = default;
inline Binary_Expression::~Binary_Expression() = default;

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

[[nodiscard]]
inline std::span<const ast::Group_Member> Directive::get_argument_span() const
{
    if (m_arguments) {
        return m_arguments->get_members();
    }
    return {};
}

[[nodiscard]]
inline std::span<const ast::Markup_Element> Directive::get_content_span() const
{
    if (m_content) {
        return m_content->get_elements();
    }
    return {};
}
// NOLINTEND(readability-redundant-inline-specifier)

} // namespace cowel::ast

#endif
