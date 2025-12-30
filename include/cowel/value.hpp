#ifndef COWEL_VALUE_HPP
#define COWEL_VALUE_HPP

#include <string_view>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/static_string.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/content_status.hpp"
#include "cowel/fwd.hpp"
#include "cowel/string_kind.hpp"
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
        block_index,
        directive_index,
    };

    union Union { // NOLINT(cppcoreguidelines-special-member-functions)
        Unit unit;
        Null null;
        bool boolean;
        Int_Storage integer;
        Float floating;
        std::u8string_view static_string;
        Short_String_Value short_string;
        std::pmr::vector<char8_t> dynamic_string;
        Block_And_Frame block;
        Directive_And_Frame directive;

        template <typename T>
        [[nodiscard]]
        static constexpr Union make(T&& other, Index other_index) noexcept
        {
            switch (other_index) {
            case unit_index: return { .unit = other.unit };
            case null_index: return { .null = other.null };
            case boolean_index: return { .boolean = other.boolean };
            case integer_index: return { .integer = other.integer };
            case floating_index: return { .floating = other.floating };
            case static_string_index: return { .static_string = other.static_string };
            case short_string_index: return { .short_string = other.short_string };
            case dynamic_string_index:
                return { .dynamic_string = std::forward<T>(other).dynamic_string };
            case block_index: return { .block = other.block };
            case directive_index: return { .directive = other.directive };
            }
        }

        template <typename T>
        constexpr void assign(Index self_index, T&& other, Index other_index) noexcept
        {
            switch (other_index) {
            case unit_index: unit = other.unit; break;
            case null_index: null = other.null; break;
            case boolean_index: boolean = other.boolean; break;
            case integer_index: integer = other.integer; break;
            case floating_index: floating = other.floating; break;
            case static_string_index: static_string = other.static_string; break;
            case short_string_index: short_string = other.short_string; break;
            case dynamic_string_index: {
                if (self_index == dynamic_string_index) {
                    dynamic_string = std::forward<T>(other).dynamic_string;
                }
                else {
                    destroy(self_index);
                    new (&dynamic_string) auto(std::forward<T>(other).dynamic_string);
                }
                break;
            }
            case block_index: block = other.block; break;
            case directive_index: directive = other.directive; break;
            }
        }

        constexpr void destroy(Index index) noexcept
        {
            static_assert(std::is_trivially_destructible_v<Integer>);
            static_assert(std::is_trivially_destructible_v<Block_And_Frame>);
            static_assert(std::is_trivially_destructible_v<Directive_And_Frame>);
            switch (index) {
            case unit_index:
            case null_index:
            case boolean_index:
            case integer_index:
            case floating_index:
            case static_string_index:
            case short_string_index:
            case block_index:
            case directive_index: break;
            case dynamic_string_index: dynamic_string.~vector();
            }
        }

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
    static constexpr Value boolean(bool value) noexcept
    {
        return Value {
            Union { .boolean = value },
            boolean_index,
            &Type::boolean,
        };
    }
    [[nodiscard]]
    static constexpr Value integer(Integer value) noexcept
    {
        return Value {
            Union { .integer = std::bit_cast<Int_Storage>(value) },
            integer_index,
            &Type::integer,
        };
    }
    [[nodiscard]]
    static constexpr Value floating(Float value) noexcept
    {
        return Value {
            Union { .floating = value },
            floating_index,
            &Type::floating,
        };
    }

    /// @brief Creates a value of type `str` from a string with static storage duration.
    /// This performs no allocation and is very cheap,
    /// but should be used with great caution
    /// because it gives this string reference semantics.
    [[nodiscard]]
    static constexpr Value static_string(std::u8string_view value, String_Kind kind) noexcept
    {
        return Value {
            Union { .static_string = value },
            static_string_index,
            &Type::str,
            kind,
        };
    }
    /// @brief Creates a value of type `str` from a string that fits into `Short_String_Value`.
    [[nodiscard]]
    static constexpr Value short_string(Short_String_Value value, String_Kind kind) noexcept
    {
        return Value {
            Union { .short_string = value },
            short_string_index,
            &Type::str,
            kind,
        };
    }
    /// @brief Creates a value of type `str` with dynamically sized contents.
    [[nodiscard]]
    static Value dynamic_string_forced(std::pmr::vector<char8_t>&& value, String_Kind kind) noexcept
    {
        return Value {
            Union { .dynamic_string = std::move(value) },
            dynamic_string_index,
            &Type::str,
            kind,
        };
    }
    /// @brief Creates a value of type `str` with dynamically sized contents.
    /// If `value` can be represented as `Short_String_Value`,
    /// creates a short string instead.
    [[nodiscard]]
    static Value dynamic_string(std::pmr::vector<char8_t>&& value, String_Kind kind) noexcept
    {
        if (value.size() <= Short_String_Value::max_size_v) {
            Value result = short_string({ value.data(), value.size() }, kind);
            // Clearing the original prevents inconsistent behavior
            // where the vector sometimes stays filled.
            value.clear();
            return result;
        }

        return Value {
            Union { .dynamic_string = std::move(value) },
            dynamic_string_index,
            &Type::str,
            kind,
        };
    }
    [[nodiscard]]
    static Value dynamic_string(
        std::u8string_view value,
        std::pmr::memory_resource* memory,
        String_Kind kind
    ) noexcept
    {
        if (value.size() <= Short_String_Value::max_size_v) {
            return short_string({ value.data(), value.size() }, kind);
        }
        std::pmr::vector<char8_t> storage { memory };
        storage.insert(storage.end(), value.data(), value.data() + value.size());
        return Value {
            Union { .dynamic_string = std::move(storage) },
            dynamic_string_index,
            &Type::str,
        };
    }

    [[nodiscard]]
    static Value block(const ast::Primary& block, Frame_Index frame);
    [[nodiscard]]
    static Value block(const ast::Directive& block, Frame_Index frame);

private:
    Index m_index;
    String_Kind m_string_kind;
    Union m_value;
    const Type* m_type;

    template <class T>
    [[nodiscard]]
    constexpr Value(
        T&& value,
        Index index,
        const Type* type,
        String_Kind string_kind = String_Kind::unknown
    ) noexcept
        : m_index { index }
        , m_string_kind { string_kind }
        , m_value { Union::make(std::forward<T>(value), index) }
        , m_type { type }
    {
        COWEL_ASSERT(type);
    }

public:
    [[nodiscard]]
    constexpr Value(const Value& other) noexcept
        : Value { other.m_value, other.m_index, other.m_type }
    {
    }

    [[nodiscard]]
    constexpr Value(Value&& other) noexcept
        : Value { std::move(other.m_value), other.m_index, other.m_type }
    {
    }

    constexpr Value& operator=(const Value& other)
    {
        if (this != &other) {
            m_value.assign(m_index, other.m_value, other.m_index);
            m_index = other.m_index;
            m_type = other.m_type;
        }
        return *this;
    }

    constexpr Value& operator=(Value&& other) noexcept
    {
        m_value.assign(m_index, std::move(other).m_value, other.m_index);
        m_index = other.m_index;
        m_type = other.m_type;
        return *this;
    }

    constexpr ~Value()
    {
        m_value.destroy(m_index);
    }

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
    constexpr bool as_boolean() const
    {
        COWEL_DEBUG_ASSERT(get_type_kind() == Type_Kind::boolean);
        return m_value.boolean;
    }
    [[nodiscard]]
    constexpr Integer as_integer() const
    {
        COWEL_DEBUG_ASSERT(get_type_kind() == Type_Kind::integer);
        return std::bit_cast<Integer>(m_value.integer);
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
        case short_string_index: return m_value.short_string;
        case dynamic_string_index: return as_u8string_view(m_value.dynamic_string);
        default: break;
        }
        COWEL_ASSERT_UNREACHABLE(u8"Value is not a string.");
    }
    [[nodiscard]]
    constexpr String_With_Meta as_string_with_meta() const
    {
        return { as_string(), m_string_kind };
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

inline constexpr Value Value::unit
    = { Value::Union { .unit = Unit {} }, Value::unit_index, &Type::unit };
inline constexpr Value Value::null
    = { Value::Union { .null = Null {} }, Value::null_index, &Type::null };
inline constexpr Value Value::true_ = Value::boolean(true);
inline constexpr Value Value::false_ = Value::boolean(false);
inline constexpr Value Value::zero_int = Value::integer(0);
inline constexpr Value Value::zero_float = Value::floating(0);
inline constexpr Value Value::empty_string = Value::static_string({}, String_Kind::ascii);
inline constexpr Value Value::unit_string = Value::static_string(u8"unit", String_Kind::ascii);
inline constexpr Value Value::true_string = Value::static_string(u8"true", String_Kind::ascii);
inline constexpr Value Value::false_string = Value::static_string(u8"false", String_Kind::ascii);

static_assert(sizeof(Value) <= 64, "Value should not be too large to be passed by value");

} // namespace cowel

#endif
