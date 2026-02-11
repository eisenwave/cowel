#ifndef COWEL_VALUE_HPP
#define COWEL_VALUE_HPP

#include <string_view>

#include "cowel/util/assert.hpp"
#include "cowel/util/fixed_string.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/ast_fwd.hpp"
#include "cowel/big_int.hpp"
#include "cowel/content_status.hpp"
#include "cowel/expression_kind.hpp"
#include "cowel/fwd.hpp"
#include "cowel/gc.hpp"
#include "cowel/regexp.hpp"
#include "cowel/string_kind.hpp"
#include "cowel/type.hpp"

namespace cowel {

struct Value;

template <Comparison_Expression_Kind kind>
bool compare(const Value&, const Value&);

/// @brief A symbolic empty class indicating a `null` value or type in COWEL.
struct Null {
    friend bool operator==(Null, Null) = default;
};

/// @brief A symbolic empty class indicating a `void` value or type in COWEL.
struct Unit {
    friend bool operator==(Unit, Unit) = default;
};

using Short_String_Value = Fixed_String8<56>;

struct String_With_Meta {
    std::u8string_view data;
    String_Kind kind;
};

struct Block_And_Frame {
    const ast::Primary* block;
    Frame_Index frame;
};

struct Directive_And_Frame {
    const ast::Directive* directive;
    Frame_Index frame;
};

using Dynamic_String_Value = GC_Ref<char8_t>;

struct Group_Member_Value;

using Group_Value = GC_Ref<Group_Member_Value>;

/// @brief A value in the COWEL language.
/// This is a tagged union (similar to `std::variant`),
/// although it does not simply wrap `std::variant` for a variety of reasons:
/// - `std::variant` uses a `std::size_t` index,
///   which is excessively large for most amounts of alternatives
/// - its functionality is relatively expensive on debug builds and constant evaluation,
///   and debugging anything in `std::variant` is tricky
/// - `Value` is used in a huge amount of places,
///   so it's worth optimizing its layout somewhat
///
/// For values of basic type (`int`, `str`, etc.),
/// the type reference is to a static `Type` object.
/// For values of group type,
/// the type reference is either to a static object or to some group
/// stored in processing pass memory.
/// At least, the value shouldn't outlive its type reference.
struct Value {
private:
    enum Index : unsigned char {
        unit_index,
        null_index,
        boolean_index,
        integer_index,
        floating_index,
        static_string_index,
        short_string_index,
        dynamic_string_index,
        regex_index,
        block_index,
        directive_index,
        group_index,
    };

    union Union { // NOLINT(cppcoreguidelines-special-member-functions)
        Unit unit;
        Null null;
        bool boolean;
        Big_Int integer;
        Float floating;
        std::u8string_view static_string;
        Short_String_Value::array_type short_string;
        Dynamic_String_Value dynamic_string;
        Reg_Exp regex;
        Block_And_Frame block;
        Directive_And_Frame directive;
        Group_Value group;

        template <typename T>
        [[nodiscard]]
        static constexpr Union make(T&& other, Index other_index) noexcept;

        template <typename T>
        constexpr void assign(Index self_index, T&& other, Index other_index) noexcept;

        constexpr void destroy(Index index) noexcept;

        constexpr ~Union() { }
    };

public:
    /// @brief The only possible value for a `unit` type.
    static const Value unit;
    /// @brief The only possible value for a `null` type.
    /// That is, the value a `null` literal has.
    static const Value null;
    /// @brief The value of a `true` boolean literal.
    static const Value true_;
    /// @brief the value of a `false` boolean literal.
    static const Value false_;
    /// @brief the value of a `0` literal.
    static const Value zero_int;
    /// @brief the value of a `0f64` literal.
    static const Value zero_float;
    /// @brief the value of a `""` string literal.
    static const Value empty_string;
    /// @brief the value of a `"unit"` string literal.
    static const Value unit_string;
    /// @brief the value of a `"true"` string literal.
    static const Value true_string;
    /// @brief the value of a `"false"` string literal.
    static const Value false_string;

    [[nodiscard]]
    static constexpr Value boolean(bool value) noexcept;
    [[nodiscard]]
    static constexpr Value integer(Int128 value) noexcept;
    [[nodiscard]]
    static constexpr Value integer(const Big_Int& value) noexcept;
    [[nodiscard]]
    static constexpr Value integer(Big_Int&& value) noexcept;
    [[nodiscard]]
    static constexpr Value floating(Float value) noexcept;

    /// @brief Dispatches to `short_string` or `dynamic_string_forced`
    /// depending on the length of the string.
    [[nodiscard]]
    static Value string(std::u8string_view value, String_Kind kind);
    /// @brief Creates a value of type `str` from a string with static storage duration.
    /// This performs no allocation and is very cheap,
    /// but should be used with great caution
    /// because it gives this string reference semantics.
    [[nodiscard]]
    static constexpr Value static_string(std::u8string_view value, String_Kind kind) noexcept;
    /// @brief Creates a value of type `str` from a string that fits into `Short_String_Value`.
    [[nodiscard]]
    static constexpr Value short_string(const Short_String_Value& value, String_Kind kind) noexcept;
    /// @brief Creates a value of type `str` with dynamic storage duration.
    /// Unlike for `static_string`, the contents of `value` are copied
    /// and kept alive using garbage collection.
    [[nodiscard]]
    static Value dynamic_string_forced(std::u8string_view value, String_Kind kind);

    [[nodiscard]]
    static Value regex(const Reg_Exp&);
    [[nodiscard]]
    static Value regex(Reg_Exp&&);

    [[nodiscard]]
    static Value block(const ast::Primary& block, Frame_Index frame);
    [[nodiscard]]
    static Value block(const ast::Directive& block, Frame_Index frame);

    static Value group(std::span<const Group_Member_Value> values);
    static Value group_move(std::span<Group_Member_Value> values);

    /// @brief Creates a value by copying each element of `values`,
    /// effectively treating it as a positional group member.
    static Value group_pack(std::span<const Value> values);
    /// @brief Creates a value by moving each element of `values`,
    /// effectively treating it as a positional group member.
    static Value group_pack_move(std::span<Value> values);

private:
    Index m_index;
    String_Kind m_string_kind;
    unsigned char m_short_string_length = 0;
    Union m_value;

    template <class T>
    [[nodiscard]]
    constexpr Value(
        T&& value,
        Index index,
        String_Kind string_kind = String_Kind::unknown,
        std::size_t short_string_length = 0
    ) noexcept;

public:
    [[nodiscard]]
    constexpr Value(const Value& other) noexcept
        : Value {
            other.m_value,
            other.m_index,
            other.m_string_kind,
            other.m_short_string_length,
        }
    {
        static_assert(std::is_nothrow_copy_constructible_v<Big_Int>);
        static_assert(std::is_nothrow_copy_constructible_v<Dynamic_String_Value>);
        static_assert(std::is_nothrow_copy_constructible_v<Group_Value>);
    }

    [[nodiscard]]
    constexpr Value(Value&& other) noexcept
        : Value {
            std::move(other.m_value),
            other.m_index,
            other.m_string_kind,
            other.m_short_string_length,
        }
    {
        static_assert(std::is_nothrow_move_constructible_v<Big_Int>);
        static_assert(std::is_nothrow_move_constructible_v<Dynamic_String_Value>);
        static_assert(std::is_nothrow_move_constructible_v<Group_Value>);
    }

    constexpr Value& operator=(const Value& other) noexcept;

    constexpr Value& operator=(Value&& other) noexcept;

    constexpr ~Value()
    {
        m_value.destroy(m_index);
    }

    /// @brief Returns the type of this value.
    [[nodiscard]]
    constexpr const Type& get_type() const noexcept
    {
        static constexpr auto types = [] {
            std::array<const Type*, 12> result;
            result[unit_index] = &Type::unit;
            result[null_index] = &Type::null;
            result[boolean_index] = &Type::boolean;
            result[integer_index] = &Type::integer;
            result[floating_index] = &Type::floating;
            result[static_string_index] = &Type::str;
            result[short_string_index] = &Type::str;
            result[dynamic_string_index] = &Type::str;
            result[regex_index] = &Type::regex;
            result[block_index] = &Type::block;
            result[directive_index] = &Type::block;
            result[group_index] = &Type::group;
            return result;
        }();
        return *types[m_index];
    }

    /// @brief Equivalent to `get_type().get_kind()`.
    [[nodiscard]]
    constexpr Type_Kind get_type_kind() const noexcept
    {
        static constexpr auto kinds = [] {
            std::array<Type_Kind, 12> result;
            result[unit_index] = Type_Kind::unit;
            result[null_index] = Type_Kind::null;
            result[boolean_index] = Type_Kind::boolean;
            result[integer_index] = Type_Kind::integer;
            result[floating_index] = Type_Kind::floating;
            result[static_string_index] = Type_Kind::str;
            result[short_string_index] = Type_Kind::str;
            result[dynamic_string_index] = Type_Kind::str;
            result[regex_index] = Type_Kind::regex;
            result[block_index] = Type_Kind::block;
            result[directive_index] = Type_Kind::block;
            result[group_index] = Type_Kind::group;
            return result;
        }();
        return kinds[m_index];
    }

    [[nodiscard]]
    constexpr String_Kind get_string_kind() const noexcept
    {
        COWEL_DEBUG_ASSERT(get_type_kind() == Type_Kind::str);
        return m_string_kind;
    }

    [[nodiscard]]
    friend constexpr bool operator==(const Value& x, const Value& y)
    {
        switch (const auto x_kind = x.get_type_kind()) {
        case Type_Kind::any:
        case Type_Kind::nothing:
        case Type_Kind::pack:
        case Type_Kind::union_:
        case Type_Kind::named:
        case Type_Kind::lazy: {
            break;
        }
        case Type_Kind::unit:
        case Type_Kind::null: {
            return x_kind == y.get_type_kind();
        }
        case Type_Kind::boolean: {
            return y.get_type_kind() == Type_Kind::boolean && x.as_boolean() == y.as_boolean();
        }
        case Type_Kind::integer: {
            return y.get_type_kind() == Type_Kind::integer && x.as_integer() == y.as_integer();
        }
        case Type_Kind::floating: {
            return y.get_type_kind() == Type_Kind::floating && x.as_float() == y.as_float();
        }
        case Type_Kind::str: {
            return y.get_type_kind() == Type_Kind::str && x.as_string() == y.as_string();
        }
        case Type_Kind::regex:
        case Type_Kind::block:
        case Type_Kind::group: {
            COWEL_ASSERT_UNREACHABLE(u8"Blocks and groups are not equality-comparable.");
        }
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid type of value.");
    }

    [[nodiscard]]
    constexpr bool is_unit() const noexcept
    {
        return get_type_kind() == Type_Kind::unit;
    }
    [[nodiscard]]
    constexpr bool is_null() const noexcept
    {
        return get_type_kind() == Type_Kind::null;
    }
    [[nodiscard]]
    constexpr bool is_bool() const noexcept
    {
        return get_type_kind() == Type_Kind::boolean;
    }

    [[nodiscard]]
    constexpr bool is_int() const noexcept
    {
        return get_type_kind() == Type_Kind::integer;
    }
    [[nodiscard]]
    constexpr bool is_float() const noexcept
    {
        return get_type_kind() == Type_Kind::floating;
    }
    [[nodiscard]]
    constexpr bool is_str() const noexcept
    {
        return get_type_kind() == Type_Kind::str;
    }
    [[nodiscard]]
    constexpr bool is_static_string() const noexcept
    {
        return m_index == static_string_index;
    }
    [[nodiscard]]
    constexpr bool is_regex() const noexcept
    {
        return get_type_kind() == Type_Kind::regex;
    }
    [[nodiscard]]
    constexpr bool is_block() const noexcept
    {
        return get_type_kind() == Type_Kind::block;
    }
    [[nodiscard]]
    constexpr bool is_group() const noexcept
    {
        return get_type_kind() == Type_Kind::group;
    }

    [[nodiscard]]
    constexpr bool as_boolean() const
    {
        COWEL_DEBUG_ASSERT(get_type_kind() == Type_Kind::boolean);
        return m_value.boolean;
    }

    [[nodiscard]]
    constexpr Big_Int& as_integer() &
    {
        COWEL_DEBUG_ASSERT(get_type_kind() == Type_Kind::integer);
        return m_value.integer;
    }
    [[nodiscard]]
    constexpr const Big_Int& as_integer() const&
    {
        COWEL_DEBUG_ASSERT(get_type_kind() == Type_Kind::integer);
        return m_value.integer;
    }
    [[nodiscard]]
    constexpr Big_Int&& as_integer() &&
    {
        COWEL_DEBUG_ASSERT(get_type_kind() == Type_Kind::integer);
        return std::move(m_value.integer);
    }
    [[nodiscard]]
    constexpr const Big_Int&& as_integer() const&&
    {
        COWEL_DEBUG_ASSERT(get_type_kind() == Type_Kind::integer);
        return std::move(m_value.integer);
    }

    [[nodiscard]]
    constexpr Float as_float() const
    {
        COWEL_DEBUG_ASSERT(get_type_kind() == Type_Kind::floating);
        return m_value.floating;
    }
    [[nodiscard]]
    constexpr std::u8string_view as_string() const
    {
        switch (m_index) {
        case static_string_index: return m_value.static_string;
        case short_string_index:
            return std::u8string_view { m_value.short_string.data(), m_short_string_length };
        case dynamic_string_index: return as_u8string_view(m_value.dynamic_string.as_span());
        default: break;
        }
        COWEL_ASSERT_UNREACHABLE(u8"Value is not a string.");
    }
    [[nodiscard]]
    constexpr String_With_Meta as_string_with_meta() const
    {
        return { as_string(), m_string_kind };
    }

    [[nodiscard]]
    constexpr Reg_Exp& as_regex() &
    {
        COWEL_DEBUG_ASSERT(get_type_kind() == Type_Kind::regex);
        return m_value.regex;
    }
    [[nodiscard]]
    constexpr const Reg_Exp& as_regex() const&
    {
        COWEL_DEBUG_ASSERT(get_type_kind() == Type_Kind::regex);
        return m_value.regex;
    }
    [[nodiscard]]
    constexpr Reg_Exp&& as_regex() &&
    {
        COWEL_DEBUG_ASSERT(get_type_kind() == Type_Kind::regex);
        return std::move(m_value.regex);
    }
    [[nodiscard]]
    constexpr const Reg_Exp&& as_regex() const&&
    {
        COWEL_DEBUG_ASSERT(get_type_kind() == Type_Kind::regex);
        return std::move(m_value.regex);
    }

    [[nodiscard]]
    // NOLINTNEXTLINE(readability-make-member-function-const)
    std::span<Group_Member_Value> get_group_members()
    {
        // The linter suppression is necessary because of a clang-tidy bug:
        // https://github.com/llvm/llvm-project/issues/174269
        COWEL_ASSERT(get_type_kind() == Type_Kind::group);
        return m_value.group.as_span();
    }
    [[nodiscard]]
    std::span<const Group_Member_Value> get_group_members() const
    {
        COWEL_ASSERT(get_type_kind() == Type_Kind::group);
        return m_value.group.as_span();
    }

    // TODO: There is currently no efficient way to move strings out of a Value.
    //       Maybe an rvalue reference overload for the functions above would make sense.

    [[nodiscard]]
    Processing_Status splice_block(Content_Policy& out, Context& context) const;

    template <Comparison_Expression_Kind kind>
    friend bool compare(const Value&, const Value&);
};

struct Group_Member_Value {
    /// @brief The name of the group member,
    /// or `Value::null` for positional members.
    Value name;
    /// @brief The value of the group member.
    Value value;
};

template <class T>
constexpr Value::Value(
    T&& value,
    Index index,
    String_Kind string_kind,
    std::size_t short_string_length
) noexcept
    : m_index { index }
    , m_string_kind { string_kind }
    , m_short_string_length { static_cast<unsigned char>(short_string_length) }
    , m_value { Union::make(std::forward<T>(value), index) }
{
    COWEL_ASSERT(short_string_length <= Short_String_Value::max_size_v);
}

template <typename T>
[[nodiscard]]
constexpr Value::Union Value::Union::make(T&& other, const Index other_index) noexcept
{
    // Note that accessing other.unit or other.null results in a GCC ICE;
    // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=123346
    switch (other_index) {
    case unit_index: return { .unit = Unit {} };
    case null_index: return { .null = Null {} };
    case boolean_index: return { .boolean = other.boolean };
    case integer_index: return { .integer = std::forward<T>(other).integer };
    case floating_index: return { .floating = other.floating };
    case static_string_index: return { .static_string = other.static_string };
    case short_string_index: return { .short_string = other.short_string };
    case dynamic_string_index: return { .dynamic_string = std::forward<T>(other).dynamic_string };
    case regex_index: return { .regex = std::forward<T>(other).regex };
    case block_index: return { .block = other.block };
    case directive_index: return { .directive = other.directive };
    case group_index: return { .group = std::forward<T>(other).group };
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid index.");
}

template <typename T>
constexpr void
Value::Union::assign(const Index self_index, T&& other, const Index other_index) noexcept
{
    switch (other_index) {
    case unit_index: unit = other.unit; break;
    case null_index: null = other.null; break;
    case boolean_index: boolean = other.boolean; break;
    case floating_index: floating = other.floating; break;
    case static_string_index: static_string = other.static_string; break;
    case short_string_index: short_string = other.short_string; break;
    case block_index: block = other.block; break;
    case directive_index: directive = other.directive; break;

    case integer_index: {
        if (self_index == integer_index) {
            integer = std::forward<T>(other).integer;
        }
        else {
            destroy(self_index);
            std::construct_at(&integer, std::forward<T>(other).integer);
        }
        break;
    }
    case dynamic_string_index: {
        if (self_index == dynamic_string_index) {
            dynamic_string = std::forward<T>(other).dynamic_string;
        }
        else {
            destroy(self_index);
            std::construct_at(&dynamic_string, std::forward<T>(other).dynamic_string);
        }
        break;
    }
    case regex_index: {
        if (self_index == regex_index) {
            regex = std::forward<T>(other).regex;
        }
        else {
            destroy(self_index);
            std::construct_at(&regex, std::forward<T>(other).regex);
        }
        break;
    }
    case group_index: {
        if (self_index == group_index) {
            group = std::forward<T>(other).group;
        }
        else {
            destroy(self_index);
            std::construct_at(&group, std::forward<T>(other).group);
        }
        break;
    };
    }
}

constexpr void Value::Union::destroy(const Index index) noexcept
{
    switch (index) {
    case unit_index:
    case null_index:
    case boolean_index:
    case floating_index:
    case static_string_index:
    case short_string_index:
    case block_index:
    case directive_index: {
        static_assert(std::is_trivially_destructible_v<Unit>);
        static_assert(std::is_trivially_destructible_v<Null>);
        static_assert(std::is_trivially_destructible_v<Short_String_Value>);
        static_assert(std::is_trivially_destructible_v<Block_And_Frame>);
        static_assert(std::is_trivially_destructible_v<Directive_And_Frame>);
        break;
    }
    case integer_index: {
        integer.~Big_Int();
        break;
    }
    case dynamic_string_index: {
        dynamic_string.~Dynamic_String_Value();
        break;
    }
    case regex_index: {
        regex.~Reg_Exp();
        break;
    }
    case group_index: {
        group.~Group_Value();
        break;
    }
    }
}

// The assignment operator needs to be defined out-of-line as a workaround to:
// https://github.com/llvm/llvm-project/issues/73232
constexpr Value& Value::operator=(const Value& other) noexcept
{
    static_assert(std::is_nothrow_copy_assignable_v<Big_Int>);
    static_assert(std::is_nothrow_copy_assignable_v<Dynamic_String_Value>);
    static_assert(std::is_nothrow_copy_assignable_v<Group_Value>);
    if (this != &other) {
        m_value.assign(m_index, other.m_value, other.m_index);
        m_index = other.m_index;
        m_string_kind = other.m_string_kind;
        m_short_string_length = other.m_short_string_length;
    }
    return *this;
}

constexpr Value& Value::operator=(Value&& other) noexcept
{
    static_assert(std::is_nothrow_move_assignable_v<Big_Int>);
    static_assert(std::is_nothrow_move_assignable_v<Dynamic_String_Value>);
    static_assert(std::is_nothrow_move_assignable_v<Group_Value>);
    m_value.assign(m_index, std::move(other).m_value, other.m_index);
    m_index = other.m_index;
    m_string_kind = other.m_string_kind;
    m_short_string_length = other.m_short_string_length;
    return *this;
}

constexpr Value Value::boolean(const bool value) noexcept
{
    return Value { Union { .boolean = value }, boolean_index };
}
constexpr Value Value::integer(const Int128 value) noexcept
{
    return Value { Union { .integer = Big_Int { value } }, integer_index };
}
constexpr Value Value::integer(const Big_Int& value) noexcept
{
    return Value { Union { .integer = value }, integer_index };
}
constexpr Value Value::integer(Big_Int&& value) noexcept
{
    return Value { Union { .integer = std::move(value) }, integer_index };
}
constexpr Value Value::floating(const Float value) noexcept
{
    return Value { Union { .floating = value }, floating_index };
}

constexpr Value
Value::static_string(const std::u8string_view value, const String_Kind kind) noexcept
{
    return Value { Union { .static_string = value }, static_string_index, kind };
}
constexpr Value
Value::short_string(const Short_String_Value& value, const String_Kind kind) noexcept
{
    return Value {
        Union { .short_string = value.as_array() },
        short_string_index,
        kind,
        value.size(),
    };
}

inline constexpr Value Value::unit = { Union { .unit = Unit {} }, unit_index };
inline constexpr Value Value::null = { Union { .null = Null {} }, null_index };
inline constexpr Value Value::true_ = Value::boolean(true);
inline constexpr Value Value::false_ = Value::boolean(false);
inline constexpr Value Value::zero_int = Value::integer(0_n);
inline constexpr Value Value::zero_float = Value::floating(0);
inline constexpr Value Value::empty_string = Value::static_string({}, String_Kind::ascii);
inline constexpr Value Value::unit_string = Value::static_string(u8"unit", String_Kind::ascii);
inline constexpr Value Value::true_string = Value::static_string(u8"true", String_Kind::ascii);
inline constexpr Value Value::false_string = Value::static_string(u8"false", String_Kind::ascii);

extern template bool compare<Comparison_Expression_Kind::eq>(const Value&, const Value&);
extern template bool compare<Comparison_Expression_Kind::ne>(const Value&, const Value&);
extern template bool compare<Comparison_Expression_Kind::lt>(const Value&, const Value&);
extern template bool compare<Comparison_Expression_Kind::gt>(const Value&, const Value&);
extern template bool compare<Comparison_Expression_Kind::le>(const Value&, const Value&);
extern template bool compare<Comparison_Expression_Kind::ge>(const Value&, const Value&);

inline bool compare_eq(const Value& x, const Value& y)
{
    return compare<Comparison_Expression_Kind::eq>(x, y);
}
inline bool compare_ne(const Value& x, const Value& y)
{
    return compare<Comparison_Expression_Kind::ne>(x, y);
}
inline bool compare_lt(const Value& x, const Value& y)
{
    return compare<Comparison_Expression_Kind::lt>(x, y);
}
inline bool compare_gt(const Value& x, const Value& y)
{
    return compare<Comparison_Expression_Kind::gt>(x, y);
}
inline bool compare_le(const Value& x, const Value& y)
{
    return compare<Comparison_Expression_Kind::le>(x, y);
}
inline bool compare_ge(const Value& x, const Value& y)
{
    return compare<Comparison_Expression_Kind::ge>(x, y);
}

} // namespace cowel

#endif
