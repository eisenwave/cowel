#ifndef COWEL_VALUE_HPP
#define COWEL_VALUE_HPP

#include <string_view>
#include <variant>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/static_string.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/content_status.hpp"
#include "cowel/fwd.hpp"
#include "cowel/type.hpp"

namespace cowel {

/// @brief A symbolic empty class indicating a `null` value or type in COWEL.
struct Null {
    friend bool operator==(Null, Null) = default;
};

/// @brief A symbolic empty class indicating a `void` value or type in COWEL.
struct Unit {
    friend bool operator==(Unit, Unit) = default;
};

struct Int_Storage {
    alignas(std::size_t) unsigned char bytes[sizeof(Integer)];
};

using Short_String_Value = Static_String8<40>;

struct Block_And_Frame {
    const ast::Primary* block;
    Frame_Index frame;
};

struct Directive_And_Frame {
    const ast::Directive* directive;
    Frame_Index frame;
};

using Value_Variant = std::variant<
    Unit, //
    Null,
    bool,
    Int_Storage,
    Float,
    std::u8string_view,
    Short_String_Value,
    std::pmr::vector<char8_t>,
    Block_And_Frame,
    Directive_And_Frame>;

/// @brief A value in the COWEL language.
/// In short, this combines a `Value_Variant` with a (non-owning) reference
/// to a type for that value.
///
/// For values of basic type (`int`, `str`, etc.),
/// the type reference is to a static `Type` object.
/// For values of group type,
/// the type reference is either to a static object or to some group
/// stored in processing pass memory.
/// At least, the value shouldn't outlive its type reference.
struct Value {
private:
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

    [[nodiscard]]
    static constexpr Value boolean(bool value) noexcept
    {
        return Value { value, &Type::boolean };
    }
    [[nodiscard]]
    static constexpr Value integer(Integer value) noexcept
    {
        return Value { std::bit_cast<Int_Storage>(value), &Type::integer };
    }
    [[nodiscard]]
    static constexpr Value floating(Float value) noexcept
    {
        return Value { value, &Type::floating };
    }

    /// @brief Creates a value of type `str` from a string with static storage duration.
    /// This performs no allocation and is very cheap,
    /// but should be used with great caution
    /// because it gives this string reference semantics.
    [[nodiscard]]
    static constexpr Value static_string(std::u8string_view value) noexcept
    {
        return Value { value, &Type::str };
    }
    /// @brief Creates a value of type `str` from a string that fits into `Short_String_Value`.
    [[nodiscard]]
    static constexpr Value short_string(Short_String_Value value) noexcept
    {
        return Value { value, &Type::str };
    }
    /// @brief Creates a value of type `str` with dynamically sized contents.
    [[nodiscard]]
    static Value dynamic_string_forced(std::pmr::vector<char8_t>&& value) noexcept
    {
        return Value { std::move(value), &Type::str };
    }
    /// @brief Creates a value of type `str` with dynamically sized contents.
    /// If `value` can be represented as `Short_String_Value`,
    /// creates a short string instead.
    [[nodiscard]]
    static Value dynamic_string(std::pmr::vector<char8_t>&& value) noexcept
    {
        if (value.size() <= Short_String_Value::max_size_v) {
            Value result = short_string({ value.data(), value.size() });
            // Clearing the original prevents inconsistent behavior
            // where the vector sometimes stays filled.
            value.clear();
            return result;
        }
        return Value { std::move(value), &Type::str };
    }

    [[nodiscard]]
    static Value block(const ast::Primary& block, Frame_Index frame);
    [[nodiscard]]
    static Value block(const ast::Directive& block, Frame_Index frame);

private:
    Value_Variant m_value;
    const Type* m_type;

    [[nodiscard]]
    constexpr Value(Value_Variant&& value, const Type* type) noexcept
        : m_value { std::move(value) }
        , m_type { type }
    {
        COWEL_ASSERT(type);
    }

public:
    /// @brief Returns the type of this value.
    [[nodiscard]]
    constexpr const Type& get_type() const noexcept
    {
        return *m_type;
    }

    /// @brief Equivalent to `get_type().get_kind()`.
    [[nodiscard]]
    constexpr Type_Kind get_type_kind() const noexcept
    {
        return m_type->get_kind();
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
        case Type_Kind::block:
        case Type_Kind::group: {
            COWEL_ASSERT_UNREACHABLE(u8"Blocks and groups are not equality-comparable.");
        }
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid type of value.");
    }

    [[nodiscard]]
    constexpr bool is_any() const noexcept
    {
        return get_type_kind() == Type_Kind::any;
    }
    [[nodiscard]]
    constexpr bool is_nothing() const noexcept
    {
        return get_type_kind() == Type_Kind::nothing;
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
        return std::holds_alternative<std::u8string_view>(m_value);
    }

    [[nodiscard]]
    constexpr bool as_boolean() const
    {
        COWEL_DEBUG_ASSERT(get_type_kind() == Type_Kind::boolean);
        return std::get<bool>(m_value);
    }
    [[nodiscard]]
    constexpr Integer as_integer() const
    {
        COWEL_DEBUG_ASSERT(get_type_kind() == Type_Kind::integer);
        return std::bit_cast<Integer>(std::get<Int_Storage>(m_value));
    }
    [[nodiscard]]
    constexpr Float as_float() const
    {
        COWEL_DEBUG_ASSERT(get_type_kind() == Type_Kind::floating);
        return std::get<Float>(m_value);
    }
    [[nodiscard]]
    constexpr std::u8string_view as_string() const
    {
        COWEL_DEBUG_ASSERT(get_type_kind() == Type_Kind::str);
        if (const auto* const static_string = std::get_if<std::u8string_view>(&m_value)) {
            return *static_string;
        }
        if (const auto* const short_string = std::get_if<Short_String_Value>(&m_value)) {
            return short_string->as_string();
        }
        return as_u8string_view(std::get<std::pmr::vector<char8_t>>(m_value));
    }
    constexpr void extract_string(std::pmr::vector<char8_t>& out) const
    {
        const std::u8string_view str = as_string();
        out.insert(out.end(), str.begin(), str.end());
    }
    // TODO: There is currently no efficient way to move strings out of a Value.
    //       Maybe an rvalue reference overload for the functions above would make sense.

    [[nodiscard]]
    Processing_Status splice_block(Content_Policy& out, Context& context) const;
};

inline constexpr Value Value::unit { Unit {}, &Type::unit };
inline constexpr Value Value::null { Null {}, &Type::null };
inline constexpr Value Value::true_ = Value::boolean(true);
inline constexpr Value Value::false_ = Value::boolean(false);
inline constexpr Value Value::zero_int = Value::integer(0);
inline constexpr Value Value::zero_float = Value::floating(0);
inline constexpr Value Value::empty_string = Value::static_string(std::u8string_view {});

static_assert(sizeof(Value) <= 64, "Value should not be too large to be passed by value");

} // namespace cowel

#endif
