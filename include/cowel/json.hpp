#ifndef COWEL_JSON_HPP
#define COWEL_JSON_HPP

#include <memory_resource>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace cowel::json {

struct Value;
struct Member;

struct Null {
    [[nodiscard]]
    friend constexpr bool operator==(Null, Null)
        = default;
};
inline constexpr Null null;

using String = std::pmr::u8string;
using Number = double;

struct Array : std::pmr::vector<Value> {
    explicit constexpr Array(
        std::pmr::memory_resource* memory = std::pmr::get_default_resource()
    ) noexcept;

    [[nodiscard]]
    friend constexpr bool operator==(const Array&, const Array&)
        = default;
};

struct Object : std::pmr::vector<Member> {
    explicit constexpr Object(
        std::pmr::memory_resource* memory = std::pmr::get_default_resource()
    ) noexcept;

    [[nodiscard]]
    friend constexpr bool operator==(const Object&, const Object&)
        = default;

    [[nodiscard]]
    constexpr const Member* find(std::u8string_view key) const noexcept;
    [[nodiscard]]
    constexpr Member* find(std::u8string_view key) noexcept
    {
        return const_cast<Member*>(std::as_const(*this).find(key)); // NOLINT
    }

    [[nodiscard]]
    constexpr const Value* find_value(std::u8string_view key) const noexcept;
    [[nodiscard]]
    constexpr Value* find_value(std::u8string_view key) noexcept
    {
        return const_cast<Value*>(std::as_const(*this).find_value(key)); // NOLINT
    }

    [[nodiscard]]
    constexpr const Null* find_null(std::u8string_view key) const noexcept
    {
        return find_alternative<Null>(key);
    }
    [[nodiscard]]
    constexpr Null* find_null(std::u8string_view key) noexcept
    {
        return find_alternative<Null>(key);
    }

    [[nodiscard]]
    constexpr const bool* find_bool(std::u8string_view key) const noexcept
    {
        return find_alternative<bool>(key);
    }
    [[nodiscard]]
    constexpr bool* find_bool(std::u8string_view key) noexcept
    {
        return find_alternative<bool>(key);
    }

    [[nodiscard]]
    constexpr const Number* find_number(std::u8string_view key) const noexcept
    {
        return find_alternative<Number>(key);
    }
    [[nodiscard]]
    constexpr Number* find_number(std::u8string_view key) noexcept
    {
        return find_alternative<Number>(key);
    }

    [[nodiscard]]
    constexpr const String* find_string(std::u8string_view key) const noexcept
    {
        return find_alternative<String>(key);
    }
    [[nodiscard]]
    constexpr String* find_string(std::u8string_view key) noexcept
    {
        return find_alternative<String>(key);
    }

    [[nodiscard]]
    constexpr const Object* find_object(std::u8string_view key) const noexcept
    {
        return find_alternative<Object>(key);
    }
    [[nodiscard]]
    constexpr Object* find_object(std::u8string_view key) noexcept
    {
        return find_alternative<Object>(key);
    }

    [[nodiscard]]
    constexpr const Array* find_array(std::u8string_view key) const noexcept
    {
        return find_alternative<Array>(key);
    }
    [[nodiscard]]
    constexpr Array* find_array(std::u8string_view key) noexcept
    {
        return find_alternative<Array>(key);
    }

private:
    template <typename T>
    [[nodiscard]]
    constexpr const T* find_alternative(std::u8string_view key) const noexcept;

    template <typename T>
    [[nodiscard]]
    constexpr T* find_alternative(std::u8string_view key) noexcept
    {
        return const_cast<T*>(std::as_const(*this).find_alternative<T>(key)); // NOLINT
    }
};

using Value_Variant = std::variant<Null, bool, Number, String, Array, Object>;

struct Value : Value_Variant {
    using Value_Variant::variant;

    [[nodiscard]]
    bool operator==(const Value&) const
        = default;

    [[nodiscard]]
    const Null* as_null() const noexcept
    {
        return std::get_if<Null>(this);
    }
    [[nodiscard]]
    const bool* as_boolean() const noexcept
    {
        return std::get_if<bool>(this);
    }
    [[nodiscard]]
    const Number* as_number() const noexcept
    {
        return std::get_if<Number>(this);
    }
    [[nodiscard]]
    const String* as_string() const noexcept
    {
        return std::get_if<String>(this);
    }
    [[nodiscard]]
    const Object* as_object() const noexcept
    {
        return std::get_if<Object>(this);
    }
    [[nodiscard]]
    const Array* as_array() const noexcept
    {
        return std::get_if<Array>(this);
    }
};

struct Member {
    String key;
    Value value;

    [[nodiscard]]
    friend constexpr bool operator==(const Member&, const Member&)
        = default;
};

constexpr Array::Array(std::pmr::memory_resource* memory) noexcept
    : std::pmr::vector<Value> { memory }
{
}

constexpr Object::Object(std::pmr::memory_resource* memory) noexcept
    : std::pmr::vector<Member> { memory }
{
}

constexpr const Member* Object::find(std::u8string_view key) const noexcept
{
    const auto it = std::ranges::find(*this, key, &Member::key);
    return &*it;
}

constexpr const Value* Object::find_value(std::u8string_view key) const noexcept
{
    const auto it = std::ranges::find(*this, key, &Member::key);
    return &it->value;
}

template <typename T>
constexpr const T* Object::find_alternative(std::u8string_view key) const noexcept
{
    const Member* const member = find(key);
    return member ? std::get_if<T>(&member->value) : nullptr;
}

[[nodiscard]]
std::optional<json::Value> load(std::u8string_view source, std::pmr::memory_resource* memory);

} // namespace cowel::json

#endif
