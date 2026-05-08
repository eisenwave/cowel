#include <memory_resource>
#include <optional>
#include <string_view>

#include <gtest/gtest.h>

#include "cowel/json.hpp"

namespace cowel::json {
namespace {

using namespace std::string_view_literals;

// Convenience: parse from a narrow string literal (all ASCII, so safe).
std::optional<Value> parse(std::string_view src, std::pmr::memory_resource* memory)
{
    const std::u8string_view u8src { reinterpret_cast<const char8_t*>(src.data()), src.size() };
    return load(u8src, memory);
}

// ── Invalid / empty input ────────────────────────────────────────────────────

TEST(Json, empty_source_fails)
{
    std::pmr::monotonic_buffer_resource memory;
    EXPECT_FALSE(parse("", &memory).has_value());
}

TEST(Json, whitespace_only_fails)
{
    std::pmr::monotonic_buffer_resource memory;
    EXPECT_FALSE(parse("   \t\n", &memory).has_value());
}

TEST(Json, invalid_token_fails)
{
    std::pmr::monotonic_buffer_resource memory;
    EXPECT_FALSE(parse("garbage", &memory).has_value());
}

TEST(Json, unterminated_string_fails)
{
    std::pmr::monotonic_buffer_resource memory;
    EXPECT_FALSE(parse(R"("unterminated)", &memory).has_value());
}

TEST(Json, unterminated_array_fails)
{
    std::pmr::monotonic_buffer_resource memory;
    EXPECT_FALSE(parse("[1, 2", &memory).has_value());
}

TEST(Json, unterminated_object_fails)
{
    std::pmr::monotonic_buffer_resource memory;
    EXPECT_FALSE(parse(R"({"key": 1)", &memory).has_value());
}

// ── Null ─────────────────────────────────────────────────────────────────────

TEST(Json, null_value)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse("null", &memory);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->as_null() != nullptr);
}

// ── Boolean ──────────────────────────────────────────────────────────────────

TEST(Json, boolean_true)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse("true", &memory);
    ASSERT_TRUE(result.has_value());
    const bool* const b = result->as_boolean();
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(*b, true);
}

TEST(Json, boolean_false)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse("false", &memory);
    ASSERT_TRUE(result.has_value());
    const bool* const b = result->as_boolean();
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(*b, false);
}

// ── Numbers ──────────────────────────────────────────────────────────────────

TEST(Json, integer_zero)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse("0", &memory);
    ASSERT_TRUE(result.has_value());
    const Number* const n = result->as_number();
    ASSERT_NE(n, nullptr);
    EXPECT_EQ(*n, 0.0);
}

TEST(Json, positive_integer)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse("42", &memory);
    ASSERT_TRUE(result.has_value());
    const Number* const n = result->as_number();
    ASSERT_NE(n, nullptr);
    EXPECT_EQ(*n, 42.0);
}

TEST(Json, floating_point)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse("3.14", &memory);
    ASSERT_TRUE(result.has_value());
    const Number* const n = result->as_number();
    ASSERT_NE(n, nullptr);
    EXPECT_DOUBLE_EQ(*n, 3.14);
}

TEST(Json, scientific_notation)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse("1.5e2", &memory);
    ASSERT_TRUE(result.has_value());
    const Number* const n = result->as_number();
    ASSERT_NE(n, nullptr);
    EXPECT_DOUBLE_EQ(*n, 150.0);
}

TEST(Json, negative_scientific_notation)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse("2.5E-1", &memory);
    ASSERT_TRUE(result.has_value());
    const Number* const n = result->as_number();
    ASSERT_NE(n, nullptr);
    EXPECT_DOUBLE_EQ(*n, 0.25);
}

// ── Strings ───────────────────────────────────────────────────────────────────

TEST(Json, empty_string)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse(R"("")", &memory);
    ASSERT_TRUE(result.has_value());
    const String* const s = result->as_string();
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(*s, u8""sv);
}

TEST(Json, simple_string)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse(R"("hello")", &memory);
    ASSERT_TRUE(result.has_value());
    const String* const s = result->as_string();
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(*s, u8"hello"sv);
}

TEST(Json, string_with_backslash_escape)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse(R"("line1\nline2\ttab\\back")", &memory);
    ASSERT_TRUE(result.has_value());
    const String* const s = result->as_string();
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(*s, u8"line1\nline2\ttab\\back"sv);
}

TEST(Json, string_with_unicode_escape)
{
    std::pmr::monotonic_buffer_resource memory;
    // \u0041 == 'A'
    const auto result = parse(R"("\u0041")", &memory);
    ASSERT_TRUE(result.has_value());
    const String* const s = result->as_string();
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(*s, u8"A"sv);
}

TEST(Json, string_with_quote_escape)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse(R"("say \"hi\"")", &memory);
    ASSERT_TRUE(result.has_value());
    const String* const s = result->as_string();
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(*s, u8R"(say "hi")"sv);
}

// ── Arrays ────────────────────────────────────────────────────────────────────

TEST(Json, empty_array)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse("[]", &memory);
    ASSERT_TRUE(result.has_value());
    const Array* const a = result->as_array();
    ASSERT_NE(a, nullptr);
    EXPECT_TRUE(a->empty());
}

TEST(Json, array_of_numbers)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse("[1, 2, 3]", &memory);
    ASSERT_TRUE(result.has_value());
    const Array* const a = result->as_array();
    ASSERT_NE(a, nullptr);
    ASSERT_EQ(a->size(), 3u);
    EXPECT_EQ(*(*a)[0].as_number(), 1.0);
    EXPECT_EQ(*(*a)[1].as_number(), 2.0);
    EXPECT_EQ(*(*a)[2].as_number(), 3.0);
}

TEST(Json, array_of_mixed_types)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse(R"([null, true, 42, "hi"])", &memory);
    ASSERT_TRUE(result.has_value());
    const Array* const a = result->as_array();
    ASSERT_NE(a, nullptr);
    ASSERT_EQ(a->size(), 4u);
    EXPECT_NE((*a)[0].as_null(), nullptr);
    EXPECT_EQ(*(*a)[1].as_boolean(), true);
    EXPECT_EQ(*(*a)[2].as_number(), 42.0);
    EXPECT_EQ(*(*a)[3].as_string(), u8"hi"sv);
}

TEST(Json, nested_array)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse("[[1, 2], [3, 4]]", &memory);
    ASSERT_TRUE(result.has_value());
    const Array* const outer = result->as_array();
    ASSERT_NE(outer, nullptr);
    ASSERT_EQ(outer->size(), 2u);

    const Array* const inner0 = (*outer)[0].as_array();
    ASSERT_NE(inner0, nullptr);
    ASSERT_EQ(inner0->size(), 2u);
    EXPECT_EQ(*(*inner0)[0].as_number(), 1.0);
    EXPECT_EQ(*(*inner0)[1].as_number(), 2.0);

    const Array* const inner1 = (*outer)[1].as_array();
    ASSERT_NE(inner1, nullptr);
    ASSERT_EQ(inner1->size(), 2u);
    EXPECT_EQ(*(*inner1)[0].as_number(), 3.0);
    EXPECT_EQ(*(*inner1)[1].as_number(), 4.0);
}

// ── Objects ───────────────────────────────────────────────────────────────────

TEST(Json, empty_object)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse("{}", &memory);
    ASSERT_TRUE(result.has_value());
    const Object* const o = result->as_object();
    ASSERT_NE(o, nullptr);
    EXPECT_TRUE(o->empty());
}

TEST(Json, object_single_string_member)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse(R"({"name": "Alice"})", &memory);
    ASSERT_TRUE(result.has_value());
    const Object* const o = result->as_object();
    ASSERT_NE(o, nullptr);
    const String* const s = o->find_string(u8"name");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(*s, u8"Alice"sv);
}

TEST(Json, object_multiple_members)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse(R"({"x": 1, "y": 2, "z": 3})", &memory);
    ASSERT_TRUE(result.has_value());
    const Object* const o = result->as_object();
    ASSERT_NE(o, nullptr);
    ASSERT_EQ(o->size(), 3u);

    const Number* const x = o->find_number(u8"x");
    const Number* const y = o->find_number(u8"y");
    const Number* const z = o->find_number(u8"z");
    ASSERT_NE(x, nullptr);
    ASSERT_NE(y, nullptr);
    ASSERT_NE(z, nullptr);
    EXPECT_EQ(*x, 1.0);
    EXPECT_EQ(*y, 2.0);
    EXPECT_EQ(*z, 3.0);
}

TEST(Json, object_bool_member)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse(R"({"active": true, "deleted": false})", &memory);
    ASSERT_TRUE(result.has_value());
    const Object* const o = result->as_object();
    ASSERT_NE(o, nullptr);

    const bool* const active = o->find_bool(u8"active");
    const bool* const deleted = o->find_bool(u8"deleted");
    ASSERT_NE(active, nullptr);
    ASSERT_NE(deleted, nullptr);
    EXPECT_EQ(*active, true);
    EXPECT_EQ(*deleted, false);
}

TEST(Json, object_null_member)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse(R"({"nothing": null})", &memory);
    ASSERT_TRUE(result.has_value());
    const Object* const o = result->as_object();
    ASSERT_NE(o, nullptr);
    const Null* const nothing = o->find_null(u8"nothing");
    EXPECT_NE(nothing, nullptr);
}

TEST(Json, nested_object)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse(R"({"outer": {"inner": 99}})", &memory);
    ASSERT_TRUE(result.has_value());
    const Object* const outer = result->as_object();
    ASSERT_NE(outer, nullptr);

    const Object* const inner = outer->find_object(u8"outer");
    ASSERT_NE(inner, nullptr);
    const Number* const val = inner->find_number(u8"inner");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 99.0);
}

TEST(Json, object_with_array_member)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse(R"({"items": [10, 20, 30]})", &memory);
    ASSERT_TRUE(result.has_value());
    const Object* const o = result->as_object();
    ASSERT_NE(o, nullptr);

    const Array* const items = o->find_array(u8"items");
    ASSERT_NE(items, nullptr);
    ASSERT_EQ(items->size(), 3u);
    EXPECT_EQ(*(*items)[0].as_number(), 10.0);
    EXPECT_EQ(*(*items)[1].as_number(), 20.0);
    EXPECT_EQ(*(*items)[2].as_number(), 30.0);
}

TEST(Json, object_missing_key_returns_null_pointer)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse(R"({"a": 1})", &memory);
    ASSERT_TRUE(result.has_value());
    const Object* const o = result->as_object();
    ASSERT_NE(o, nullptr);
    // Querying for a type that is present under the wrong key returns nullptr.
    EXPECT_EQ(o->find_string(u8"a"), nullptr);
    // Querying for a key that does not exist returns nullptr.
    EXPECT_EQ(o->find_number(u8"missing"), nullptr);
}

// ── Comments (allowed by the parser) ─────────────────────────────────────────

TEST(Json, line_comment_ignored)
{
    std::pmr::monotonic_buffer_resource memory;
    // The parser is configured with allow_comments = true.
    const auto result = parse("42 // this is a comment\n", &memory);
    ASSERT_TRUE(result.has_value());
    const Number* const n = result->as_number();
    ASSERT_NE(n, nullptr);
    EXPECT_EQ(*n, 42.0);
}

TEST(Json, block_comment_ignored)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse("/* comment */ true", &memory);
    ASSERT_TRUE(result.has_value());
    const bool* const b = result->as_boolean();
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(*b, true);
}

TEST(Json, comment_inside_object)
{
    std::pmr::monotonic_buffer_resource memory;
    const auto result = parse(
        "{\n"
        "  // version number\n"
        "  \"version\": 2\n"
        "}",
        &memory
    );
    ASSERT_TRUE(result.has_value());
    const Object* const o = result->as_object();
    ASSERT_NE(o, nullptr);
    const Number* const ver = o->find_number(u8"version");
    ASSERT_NE(ver, nullptr);
    EXPECT_EQ(*ver, 2.0);
}

// ── Complex / realistic sample ────────────────────────────────────────────────

TEST(Json, realistic_config_object)
{
    std::pmr::monotonic_buffer_resource memory;
    constexpr std::string_view src = R"({
    "name": "cowel",
    "version": 1,
    "enabled": true,
    "tags": ["markup", "html", "compiler"],
    "meta": {
        "author": "eisenwave",
        "stable": false
    }
})";
    const auto result = parse(src, &memory);
    ASSERT_TRUE(result.has_value());

    const Object* const root = result->as_object();
    ASSERT_NE(root, nullptr);

    // Scalar members.
    const String* const name = root->find_string(u8"name");
    ASSERT_NE(name, nullptr);
    EXPECT_EQ(*name, u8"cowel"sv);

    const Number* const version = root->find_number(u8"version");
    ASSERT_NE(version, nullptr);
    EXPECT_EQ(*version, 1.0);

    const bool* const enabled = root->find_bool(u8"enabled");
    ASSERT_NE(enabled, nullptr);
    EXPECT_EQ(*enabled, true);

    // Array member.
    const Array* const tags = root->find_array(u8"tags");
    ASSERT_NE(tags, nullptr);
    ASSERT_EQ(tags->size(), 3u);
    EXPECT_EQ(*(*tags)[0].as_string(), u8"markup"sv);
    EXPECT_EQ(*(*tags)[1].as_string(), u8"html"sv);
    EXPECT_EQ(*(*tags)[2].as_string(), u8"compiler"sv);

    // Nested object member.
    const Object* const meta = root->find_object(u8"meta");
    ASSERT_NE(meta, nullptr);

    const String* const author = meta->find_string(u8"author");
    ASSERT_NE(author, nullptr);
    EXPECT_EQ(*author, u8"eisenwave"sv);

    const bool* const stable = meta->find_bool(u8"stable");
    ASSERT_NE(stable, nullptr);
    EXPECT_EQ(*stable, false);
}

} // namespace
} // namespace cowel::json
