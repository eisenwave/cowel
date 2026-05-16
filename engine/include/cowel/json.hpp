#ifndef COWEL_JSON_HPP
#define COWEL_JSON_HPP

#include <algorithm>
#include <cmath>
#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "cowel/util/string_or_char_consumer.hpp"
#include "cowel/util/to_chars.hpp"

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
    if (it == end()) {
        return nullptr;
    }
    return &*it;
}

constexpr const Value* Object::find_value(std::u8string_view key) const noexcept
{
    const Member* const member = find(key);
    return member ? &member->value : nullptr;
}

template <typename T>
constexpr const T* Object::find_alternative(std::u8string_view key) const noexcept
{
    const Member* const member = find(key);
    return member ? std::get_if<T>(&member->value) : nullptr;
}

[[nodiscard]]
std::optional<json::Value> load(std::u8string_view source, std::pmr::memory_resource* memory);

/// @brief Returns the JSON escape sequence for `c`
/// if it has a named two-character form
/// (`\\\"`, `\\\\`, `\\b`, `\\f`, `\\n`, `\\r`, `\\t`),
/// the `\\u00XX` form for other control characters (0x00–0x1F),
/// or an empty view otherwise.
/// The returned view always points to a string literal.
[[nodiscard]]
constexpr std::u8string_view escape(const char8_t c) noexcept
{
    using namespace std::string_view_literals;
    switch (c) {
    case 0x00: return u8"\\u0000"sv;
    case 0x01: return u8"\\u0001"sv;
    case 0x02: return u8"\\u0002"sv;
    case 0x03: return u8"\\u0003"sv;
    case 0x04: return u8"\\u0004"sv;
    case 0x05: return u8"\\u0005"sv;
    case 0x06: return u8"\\u0006"sv;
    case 0x07: return u8"\\u0007"sv;
    case 0x08: return u8"\\b"sv;
    case 0x09: return u8"\\t"sv;
    case 0x0a: return u8"\\n"sv;
    case 0x0b: return u8"\\u000b"sv;
    case 0x0c: return u8"\\f"sv;
    case 0x0d: return u8"\\r"sv;
    case 0x0e: return u8"\\u000e"sv;
    case 0x0f: return u8"\\u000f"sv;
    case 0x10: return u8"\\u0010"sv;
    case 0x11: return u8"\\u0011"sv;
    case 0x12: return u8"\\u0012"sv;
    case 0x13: return u8"\\u0013"sv;
    case 0x14: return u8"\\u0014"sv;
    case 0x15: return u8"\\u0015"sv;
    case 0x16: return u8"\\u0016"sv;
    case 0x17: return u8"\\u0017"sv;
    case 0x18: return u8"\\u0018"sv;
    case 0x19: return u8"\\u0019"sv;
    case 0x1a: return u8"\\u001a"sv;
    case 0x1b: return u8"\\u001b"sv;
    case 0x1c: return u8"\\u001c"sv;
    case 0x1d: return u8"\\u001d"sv;
    case 0x1e: return u8"\\u001e"sv;
    case 0x1f: return u8"\\u001f"sv;
    case u8'"': return u8"\\\""sv;
    case u8'\\': return u8"\\\\"sv;
    default: return {};
    }
}

inline constexpr std::u8string_view null_string = u8"null";
inline constexpr std::u8string_view true_string = u8"true";
inline constexpr std::u8string_view false_string = u8"false";

[[nodiscard]]
constexpr std::u8string_view bool_string(const bool b)
{
    return b ? true_string : false_string;
}

template <string_or_char_consumer Out>
void write_string(Out& out, std::u8string_view s)
{
    out(u8'"');
    while (!s.empty()) {
        std::size_t safe = 0;
        while (safe < s.size() && s[safe] >= 0x20 && s[safe] != u8'"' && s[safe] != u8'\\') {
            ++safe;
        }
        if (safe > 0) {
            out(s.substr(0, safe));
            s.remove_prefix(safe);
            continue;
        }
        const char8_t c = s.front();
        s.remove_prefix(1);
        out(escape(c));
    }
    out(u8'"');
}

template <string_or_char_consumer Out>
void write_number(Out& out, const Number n)
{
    // Non-finite numbers have no JSON representation; emit null.
    if (!std::isfinite(n)) {
        out(null_string);
        return;
    }
    const auto chars = to_characters8(n);
    out(chars.as_string());
}

template <string_or_char_consumer Out>
void write_value(Out& out, const Value& v);

template <string_or_char_consumer Out>
void write_array(Out& out, const Array& a)
{
    out(u8'[');
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (i != 0) {
            out(u8',');
        }
        write_value(out, a[i]);
    }
    out(u8']');
}

template <string_or_char_consumer Out>
void write_object(Out& out, const Object& o)
{
    out(u8'{');
    for (std::size_t i = 0; i < o.size(); ++i) {
        if (i != 0) {
            out(u8',');
        }
        write_string(out, o[i].key);
        out(u8':');
        write_value(out, o[i].value);
    }
    out(u8'}');
}

/// @brief Appends the compact JSON serialization of `value` to `out`.
/// Numbers that are not finite (NaN, ±infinity) are serialized as JSON `null`.
template <string_or_char_consumer Out>
void write_value(Out& out, const Value& value)
{
    using namespace std::string_view_literals;
    std::visit(
        [&out](const auto& x) {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, Null>) {
                out(null_string);
            }
            else if constexpr (std::is_same_v<T, bool>) {
                out(bool_string(x));
            }
            else if constexpr (std::is_same_v<T, Number>) {
                write_number(out, x);
            }
            else if constexpr (std::is_same_v<T, String>) {
                write_string(out, x);
            }
            else if constexpr (std::is_same_v<T, Array>) {
                write_array(out, x);
            }
            else if constexpr (std::is_same_v<T, Object>) {
                write_object(out, x);
            }
        },
        static_cast<const Value_Variant&>(value)
    );
}

} // namespace cowel::json

#endif
