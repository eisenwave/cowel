#ifndef MMML_JSON_HPP
#define MMML_JSON_HPP

#include <memory_resource>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace mmml::json {

struct Value;
struct Member;
struct Null {
    [[nodiscard]]
    friend constexpr bool operator==(Null, Null)
        = default;
};
inline constexpr Null null;

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
    constexpr const bool* find_bool(std::u8string_view key) const noexcept
    {
        return find_alternative<bool>(key);
    }

    [[nodiscard]]
    constexpr const double* find_number(std::u8string_view key) const noexcept;

private:
    template <typename T>
    [[nodiscard]]
    constexpr const T* find_alternative(std::u8string_view key) const noexcept;
};

using Value_Variant = std::variant<Null, bool, double, std::pmr::u8string, Array, Object>;

struct Value : Value_Variant {
    using Value_Variant::variant;

    [[nodiscard]]
    bool operator==(const Value&) const
        = default;
};

struct Member {
    std::pmr::u8string key;
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

/* constexpr const bool* Object::find_bool(std::u8string_view key) const noexcept
{
    const Member* const member = find(key);
    return member ? std::get_if<bool>(&member->value) : nullptr;
} */

constexpr const double* Object::find_number(std::u8string_view key) const noexcept
{
    const Member* const member = find(key);
    return member ? std::get_if<double>(&member->value) : nullptr;
}

[[nodiscard]]
inline std::optional<json::Value>
load(std::u8string_view source, std::pmr::memory_resource* memory);

} // namespace mmml::json

#endif
