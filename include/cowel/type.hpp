#ifndef COWEL_TYPE_HPP
#define COWEL_TYPE_HPP

#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cowel/util/assert.hpp"

#include "cowel/fwd.hpp"

namespace cowel {

/// @brief The kind of a type.
enum struct Type_Kind : Default_Underlying {
    /// @brief The top type, i.e. the union of all types.
    any,
    /// @brief The bottom type, i.e. an empty type set.
    /// This is the type of return expressions, unions containing `nothing`, etc.
    nothing,

    /// @brief A unit type for directives that don't return anything.
    /// Produces nothing when spliced, and does not indicate an error.
    unit,
    /// @brief A unit type indicating errors, absence of values, etc.
    /// Produces `null` when spliced.
    null,

    /// @brief A type representing `true` or `false`.
    boolean,
    /// @brief A type which holds integer values.
    /// While this is intended to eventually hold any integer (i.e. "big int"),
    /// this is currently limited (see `Integer`).
    integer,
    /// @brief A type which holds binary64 floating-point numbers.
    floating,
    /// @brief A UTF-8 string of characters.
    str,
    /// @brief A regular expression.
    regex,
    /// @brief A block of markup.
    /// Always lazily evaluated, acting a bit like a C++ lambda with no parameters.
    block,

    /// @brief A group (similar to `struct`), i.e. a product type,
    /// but can hold both named and unnamed members and packs thereof.
    group,
    /// @brief A pack of other types.
    /// May only appear within a group.
    pack,
    /// @brief A named member.
    /// May only appear within a group.
    named,
    /// @brief A lazily evaluated value.
    lazy,
    /// @brief A union of other types, i.e. a sum type.
    union_,
};

[[nodiscard]]
constexpr bool type_kind_is_basic(Type_Kind kind)
{
    return kind <= Type_Kind::block;
}
[[nodiscard]]
constexpr bool type_kind_is_compound(Type_Kind kind)
{
    return kind > Type_Kind::block;
}

/// @brief Returns `true` iff a `Value` can hold values of type `kind`.
[[nodiscard]]
constexpr bool type_kind_is_value_holdable(Type_Kind kind)
{
    using enum Type_Kind;
    switch (kind) {
    case any:
    case nothing:
    case union_:
    case pack:
    case named: return false;
    default: return true;
    }
}

/// @brief Returns `true` iff `kind` can be spliced into markup.
[[nodiscard]]
constexpr bool type_kind_is_spliceable(Type_Kind kind)
{
    using enum Type_Kind;
    switch (kind) {
    case unit:
    case null:
    case boolean:
    case integer:
    case floating:
    case str:
    case block: return true;

    case any:
    case nothing:
    case regex:
    case group:
    case pack:
    case named:
    case lazy:
    case union_: return false;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid kind.");
}

[[nodiscard]]
constexpr std::u8string_view type_kind_display_name(Type_Kind kind)
{
    using enum Type_Kind;
    switch (kind) {
    case any: return u8"any";
    case nothing: return u8"nothing";
    case unit: return u8"unit";
    case null: return u8"null";
    case boolean: return u8"bool";
    case integer: return u8"int";
    case floating: return u8"float";
    case regex: return u8"regex";
    case str: return u8"str";
    case block: return u8"block";
    case group: return u8"group";
    case pack: return u8"pack";
    case named: return u8"named";
    case lazy: return u8"lazy";
    case union_: return u8"union";
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid kind");
}

/// @brief A type in the COWEL type system.
struct Type {
    /// @brief The `any` type, i.e. the top type.
    /// Dynamic.
    static const Type any;
    /// @brief The `nothing` type, i.e. the bottom type.
    static const Type nothing;
    /// @brief The `unit` type.
    static const Type unit;
    /// @brief The `null` type.
    static const Type null;
    /// @brief The `bool` type.
    static const Type boolean;
    /// @brief The `int` type.
    static const Type integer;
    /// @brief The `float` type.
    static const Type floating;
    /// @brief The `str` type.
    static const Type str;
    /// @brief The `regex` type.
    static const Type regex;
    /// @brief The `block` type.
    static const Type block;
    static const Type empty_group;
    /// @brief The dynamic group type, i.e. a group of anything.
    /// No value with this dynamic type can be created,
    /// but values of group type are considered to have this type for the purpose of analysis.
    static const Type group;

    static constexpr Type basic(Type_Kind kind)
    {
        COWEL_ASSERT(type_kind_is_basic(kind));
        const auto dynamic_flag = kind == Type_Kind::any ? Flags::dynamic : Flags {};
        return { kind, {}, Flags::canonical | Flags::legal | dynamic_flag };
    }

    static constexpr Type pack_of(const Type* const element)
    {
        return {
            Type_Kind::pack,
            { element, 1 },
            pack_is_canonical(*element) ? Flags::canonical : Flags {},
        };
    }
    static constexpr Type canonical_pack_of(const Type* const element)
    {
        // Cannot form canonical packs of non-canonical elements.
        COWEL_ASSERT(element->is_canonical());
        if (element->m_kind == Type_Kind::nothing) {
            return Type::nothing;
        }
        if (element->m_kind == Type_Kind::pack) {
            COWEL_DEBUG_ASSERT(element->m_members.size() == 1);
            return canonical_pack_of(&element->m_members.front());
        }
        COWEL_DEBUG_ASSERT(pack_is_canonical(*element));
        return { Type_Kind ::pack, { element, 1 }, Flags::canonical };
    }

    static constexpr Type named(const Type* const element)
    {
        return {
            Type_Kind::named,
            { element, 1 },
            named_is_canonical(*element) ? Flags ::canonical : Flags {},
        };
    }
    static constexpr Type canonical_named(const Type* const element)
    {
        // Cannot form canonical named from non-canonical.
        COWEL_ASSERT(element->is_canonical());
        if (element->m_kind == Type_Kind::nothing) {
            return Type::nothing;
        }
        COWEL_DEBUG_ASSERT(named_is_canonical(*element));
        return { Type_Kind::named, { element, 1 }, Flags::canonical };
    }

    static constexpr Type lazy(const Type* const element)
    {
        return {
            Type_Kind::lazy,
            { element, 1 },
            lazy_is_canonical(*element) ? Flags::canonical : Flags {},
        };
    }
    static constexpr Type canonical_lazy(const Type* const element)
    {
        // Cannot form canonical lazy from non-canonical.
        COWEL_ASSERT(element->is_canonical());
        return { Type_Kind::lazy, { element, 1 }, Flags::canonical };
    }

    /// @brief Forms a group type from a given list of members,
    /// which may not be canonical.
    static constexpr Type group_of(const std::span<const Type> members)
    {
        return {
            Type_Kind::group,
            members,
            group_is_canonical(members) ? Flags::canonical : Flags {},
        };
    }
    static constexpr Type canonical_group_of(std::vector<Type>& members)
    {
        for (auto& m : members) {
            COWEL_ASSERT(m.is_canonical());
            if (m == Type::nothing) {
                return Type::nothing;
            }
        }
        COWEL_DEBUG_ASSERT(group_is_canonical(members));
        return { Type_Kind::group, members, Flags::canonical };
    }

    /// @brief Forms a union type from a given list of alternatives,
    /// which may not be canonical.
    static constexpr Type union_of(const std::span<const Type> alternatives)
    {
        return {
            Type_Kind::union_,
            alternatives,
            union_is_canonical(alternatives) ? Flags::canonical : Flags {},
        };
    }
    /// @brief Forms a union type from the given `alternatives`.
    /// The union is canonicalized as needed.
    /// This implies that the result may not actually be a union,
    /// such as when a single-alternative union is canonicalized to that alternative.
    static constexpr Type canonical_union_of(std::vector<Type>& alternatives)
    {
        for (auto& a : alternatives) {
            COWEL_ASSERT(a.is_canonical());
        }
        // Nested unions are flattened, recursively.
        while (std::ranges::contains(alternatives, Type_Kind::union_, &Type::m_kind)) {
            for (auto it = alternatives.begin(); it != alternatives.end();) {
                if (it->m_kind == Type_Kind::union_) {
                    Type nested_union = *it;
                    it = alternatives.erase(it);
                    it = alternatives.insert(
                        it, std::make_move_iterator(nested_union.m_members.begin()),
                        std::make_move_iterator(nested_union.m_members.end())
                    );
                    continue;
                }
                ++it;
            }
        }
        // A union containing any is any.
        if (std::ranges::contains(alternatives, Type::any)) {
            return Type::any;
        }
        // nothing types are removed.
        std::erase(alternatives, Type::nothing);
        // Alternatives are brought into a canonical order.
        std::ranges::sort(alternatives);
        // Duplicate alternatives are removed.
        alternatives.erase(std::ranges::unique(alternatives).begin(), alternatives.end());

        // Empty unions are canonicalized to nothing.
        if (alternatives.empty()) {
            return Type::nothing;
        }
        // Single-alternative unions are canonicalized to that alternative.
        if (alternatives.size() == 1) {
            return alternatives.front();
        }

        COWEL_DEBUG_ASSERT(union_is_canonical(alternatives));
        return { Type_Kind::union_, alternatives, Flags::canonical };
    }

    enum struct Flags : unsigned char {
        canonical = 1 << 0,
        non_canonical = 1 << 1,
        legal = 1 << 2,
        illegal = 1 << 3,
        dynamic = 1 << 4,
    };

    friend constexpr Flags operator&(Flags x, Flags y)
    {
        return Flags(int(x) & int(y));
    }
    friend constexpr Flags operator|(Flags x, Flags y)
    {
        return Flags(int(x) | int(y));
    }
    friend constexpr Flags operator~(Flags x)
    {
        return Flags(~int(x));
    }
    friend constexpr Flags& operator|=(Flags& x, Flags y)
    {
        return x = x | y;
    }

private:
    Type_Kind m_kind;
    Flags m_flags;
    std::span<const Type> m_members;

    [[nodiscard]]
    constexpr Type(
        const Type_Kind kind,
        const std::span<const Type> members,
        const Flags flags
    ) noexcept
        : m_kind { kind }
        , m_flags { flags }
        , m_members { members }
    {
    }

public:
    [[nodiscard]]
    constexpr Type_Kind get_kind() const
    {
        return m_kind;
    }

    [[nodiscard]]
    friend constexpr bool operator==(const Type& x, const Type& y)
    {
        return x.m_kind == y.m_kind && std::ranges::equal(x.m_members, y.m_members);
    }
    friend constexpr std::strong_ordering operator<=>(const Type& x, const Type& y)
    {
        const std::strong_ordering kind_compare = x.m_kind <=> y.m_kind;
        if (kind_compare != 0) {
            return kind_compare;
        }
        return std::lexicographical_compare_three_way(
            x.m_members.begin(), x.m_members.end(), //
            y.m_members.begin(), y.m_members.end()
        );
    }

    /// @brief Returns `true` if this type is equivalent to `other`.
    [[nodiscard]]
    constexpr bool equivalent_to(const Type& other) const
    {
        COWEL_ASSERT(is_canonical());
        COWEL_ASSERT(other.is_canonical());
        return *this == other;
    }

    /// @brief Returns `true` if this type is analytically convertible to `other`.
    /// That is, if this type is equivalent to `other` or if an expression of this type
    /// could be stored in a variable of type `other` without any change to that value.
    ///
    /// For example, `int` is analytically convertible to `int | null`,
    /// and `nothing` is analytically convertible to any other type.
    [[nodiscard]]
    constexpr bool analytically_convertible_to(const Type& other) const
    {
        COWEL_ASSERT(is_canonical());
        COWEL_ASSERT(other.is_canonical());

        if (other.m_kind == Type_Kind::any || *this == other) {
            return true;
        }

        switch (m_kind) {
        case Type_Kind::nothing: {
            return true;
        }
        case Type_Kind::pack: {
            return other.m_kind == Type_Kind::pack
                && m_members.front().analytically_convertible_to(other.m_members.front());
        }
        case Type_Kind::named: {
            return other.m_kind == Type_Kind::named
                && m_members.front().analytically_convertible_to(other.m_members.front());
        }
        case Type_Kind::union_: {
            return std::ranges::all_of(m_members, [&](const Type& type) {
                return type.analytically_convertible_to(other);
            });
        }
        case Type_Kind::group: {
            if (other.m_kind != Type_Kind::group) {
                break;
            }
            if (is_dynamic() || other.is_dynamic()) {
                return true;
            }
            if (m_members.size() != other.m_members.size()) {
                break;
            }
            const bool all_members_convertible = [&] {
                for (std::size_t i = 0; i < m_members.size(); ++i) {
                    if (!m_members[i].analytically_convertible_to(other.m_members[i])) {
                        return false;
                    }
                }
                return true;
            }();
            if (all_members_convertible) {
                return true;
            }
            break;
        }
        default: break;
        }

        switch (other.m_kind) {
        case Type_Kind::any: {
            return true;
        }
        case Type_Kind::lazy: {
            COWEL_ASSERT(other.m_members.size() == 1);
            return analytically_convertible_to(other.m_members.front());
        }
        case Type_Kind::union_: {
            return std::ranges::any_of(other.m_members, [&](const Type& type) {
                return analytically_convertible_to(type);
            });
        }
        default: break;
        }

        return false;
    }

    [[nodiscard]]
    std::u8string get_display_name() const;

    [[nodiscard]]
    bool is_dynamic() const
    {
        return (m_flags & Flags::dynamic) != Flags {};
    }

    [[nodiscard]]
    constexpr bool is_basic() const
    {
        return type_kind_is_basic(m_kind);
    }

    [[nodiscard]]
    constexpr bool is_canonical() const
    {
        if ((m_flags & Flags::canonical) != Flags {}) {
            return true;
        }
        if ((m_flags & Flags::non_canonical) != Flags {}) {
            return false;
        }

        const bool result = [&] {
            switch (m_kind) {
            case Type_Kind::pack: {
                COWEL_ASSERT(m_members.size() == 1);
                return pack_is_canonical(m_members.front());
            }
            case Type_Kind::named: {
                COWEL_ASSERT(m_members.size() == 1);
                return named_is_canonical(m_members.front());
            }
            case Type_Kind::lazy: {
                COWEL_ASSERT(m_members.size() == 1);
                return lazy_is_canonical(m_members.front());
            }
            case Type_Kind::group: return group_is_canonical(m_members);
            case Type_Kind::union_: return union_is_canonical(m_members);
            default: {
                COWEL_DEBUG_ASSERT(is_basic());
                return true;
            }
            }
            COWEL_ASSERT_UNREACHABLE(u8"Invalid type kind.");
        }();

        return result;
    }

    [[nodiscard]]
    constexpr std::span<const Type> get_members() const
    {
        return m_members;
    }

private:
    [[nodiscard]]
    static constexpr bool pack_is_canonical(const Type& element)
    {
        return element.m_kind != Type_Kind::pack //
            && element.m_kind != Type_Kind::nothing //
            && element.is_canonical();
    }

    [[nodiscard]]
    static constexpr bool named_is_canonical(const Type& element)
    {
        return element.m_kind != Type_Kind::named //
            && element.m_kind != Type_Kind::nothing //
            && element.is_canonical();
    }

    [[nodiscard]]
    static constexpr bool lazy_is_canonical(const Type& element)
    {
        return element.is_canonical();
    }

    [[nodiscard]]
    static constexpr bool group_is_canonical(const std::span<const Type> members)
    {
        return std::ranges::none_of(members, [](const Type& type) {
            return type == Type::nothing || !type.is_canonical();
        });
    }

    [[nodiscard]]
    static constexpr bool union_is_canonical(const std::span<const Type> alternatives)
    {
        if (alternatives.size() <= 1) {
            return false;
        }
        constexpr auto non_canonical_in_union = [](const Type& type) -> bool {
            return type.m_kind == Type_Kind::union_ //
                || type.m_kind == Type_Kind::any //
                || type.m_kind == Type_Kind::nothing //
                || !type.is_canonical();
        };
        if (std::ranges::any_of(alternatives, non_canonical_in_union)) {
            return false;
        }
        if (!std::ranges::is_sorted(alternatives)) {
            return false;
        }
        if (std::ranges::adjacent_find(alternatives) != alternatives.end()) {
            return true;
        }
        return true;
    }
};

inline constexpr Type Type::any = Type::basic(Type_Kind::any);
inline constexpr Type Type::nothing = Type::basic(Type_Kind::nothing);
inline constexpr Type Type::unit = Type::basic(Type_Kind::unit);
inline constexpr Type Type::null = Type::basic(Type_Kind::null);
inline constexpr Type Type::boolean = Type::basic(Type_Kind::boolean);
inline constexpr Type Type::integer = Type::basic(Type_Kind::integer);
inline constexpr Type Type::floating = Type::basic(Type_Kind::floating);
inline constexpr Type Type::str = Type::basic(Type_Kind::str);
inline constexpr Type Type::regex = Type::basic(Type_Kind::regex);
inline constexpr Type Type::block = Type::basic(Type_Kind::block);
inline constexpr Type Type::group
    = { Type_Kind::group, {}, Flags::canonical | Flags::legal | Flags::dynamic };
inline constexpr Type Type::empty_group = Type::group_of({});

} // namespace cowel

#endif
