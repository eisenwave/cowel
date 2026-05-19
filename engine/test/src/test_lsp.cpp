#include <memory_resource>
#include <optional>
#include <span>
#include <string>
#include <variant>

#include <gtest/gtest.h>

#include "cowel/json.hpp"
#include "cowel/lsp.hpp"
#include "cowel/util/string_or_char_consumer.hpp"

namespace cowel::lsp {
namespace {

using namespace std::literals;

template <typename T>
std::optional<T> from_json_str(const std::u8string_view json, Deserialize_Storage& storage)
{
    // The parsed json::Value is heap-allocated via shared_ptr so that:
    // - SSO string buffers (stored inline in json::String) remain at a stable
    //   heap address regardless of any Deserialize_Storage vector reallocation.
    // - The memory resource is also kept alive to cover non-SSO string data.
    auto mem = std::make_shared<std::pmr::monotonic_buffer_resource>();
    auto* const mem_ptr = mem.get();
    std::optional<json::Value> parsed = json::load(json, mem_ptr);
    if (!parsed) {
        return {};
    }
    storage.emplace(mem); // keep allocator alive for non-SSO strings
    auto val_owner = std::make_shared<json::Value>(std::move(*parsed));
    const json::Value& val = *val_owner; // stable heap reference
    storage.emplace(std::move(val_owner)); // transfer ownership into storage
    return from_json<T>(val, storage);
}

template <typename T>
std::u8string to_json_str(const T& value)
{
    std::pmr::monotonic_buffer_resource mem;
    const json::Value jval { lsp::to_json(value, &mem) };
    std::u8string result;
    U8String_Consumer consumer { result };
    json::write_value(consumer, jval);
    return result;
}

TEST(LspSerializer_Bool, from_true)
{
    Deserialize_Storage storage;
    const auto result = from_json_str<bool>(u8"true", storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, true);
}

TEST(LspSerializer_Bool, from_false)
{
    Deserialize_Storage storage;
    const auto result = from_json_str<bool>(u8"false", storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, false);
}

TEST(LspSerializer_Bool, from_non_bool_fails)
{
    Deserialize_Storage storage;
    EXPECT_FALSE(from_json_str<bool>(u8"1", storage).has_value());
    EXPECT_FALSE(from_json_str<bool>(u8"null", storage).has_value());
    EXPECT_FALSE(from_json_str<bool>(u8R"("true")", storage).has_value());
}

TEST(LspSerializer_Bool, to_json_true)
{
    EXPECT_EQ(to_json_str(true), u8"true");
}

TEST(LspSerializer_Bool, to_json_false)
{
    EXPECT_EQ(to_json_str(false), u8"false");
}

TEST(LspSerializer_Integer, from_integer)
{
    Deserialize_Storage storage;
    const auto result = from_json_str<Integer>(u8"42", storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST(LspSerializer_Integer, from_negative)
{
    Deserialize_Storage storage;
    const auto result = from_json_str<Integer>(u8"-1", storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, -1);
}

TEST(LspSerializer_Integer, from_fractional_fails)
{
    Deserialize_Storage storage;
    EXPECT_FALSE(from_json_str<Integer>(u8"1.5", storage).has_value());
}

TEST(LspSerializer_Integer, from_string_fails)
{
    Deserialize_Storage storage;
    EXPECT_FALSE(from_json_str<Integer>(u8R"("42")", storage).has_value());
}

TEST(LspSerializer_Integer, to_json)
{
    EXPECT_EQ(to_json_str<Integer>(0), u8"0");
    EXPECT_EQ(to_json_str<Integer>(100), u8"100");
    EXPECT_EQ(to_json_str<Integer>(-32700), u8"-32700");
}

TEST(LspSerializer_Number, from_number)
{
    Deserialize_Storage storage;
    const auto result = from_json_str<json::Number>(u8"3.14", storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 3.14);
}

TEST(LspSerializer_Number, from_non_number_fails)
{
    Deserialize_Storage storage;
    EXPECT_FALSE(from_json_str<json::Number>(u8"true", storage).has_value());
    EXPECT_FALSE(from_json_str<json::Number>(u8R"("3")", storage).has_value());
}

TEST(LspSerializer_Number, to_json)
{
    EXPECT_EQ(to_json_str<json::Number>(0.0), u8"0");
    EXPECT_EQ(to_json_str<json::Number>(1.0), u8"1");
}

TEST(LspSerializer_Null, from_null)
{
    Deserialize_Storage storage;
    const auto result = from_json_str<Null>(u8"null", storage);
    ASSERT_TRUE(result.has_value());
}

TEST(LspSerializer_Null, from_non_null_fails)
{
    Deserialize_Storage storage;
    EXPECT_FALSE(from_json_str<Null>(u8"false", storage).has_value());
    EXPECT_FALSE(from_json_str<Null>(u8"0", storage).has_value());
}

TEST(LspSerializer_Null, to_json)
{
    EXPECT_EQ(to_json_str(Null {}), u8"null");
}

TEST(LspSerializer_StringView, from_string)
{
    Deserialize_Storage storage;
    const auto result = from_json_str<std::u8string_view>(u8R"("hello")", storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, u8"hello"sv);
}

TEST(LspSerializer_StringView, from_empty_string)
{
    Deserialize_Storage storage;
    const auto result = from_json_str<std::u8string_view>(u8R"("")", storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, u8""sv);
}

TEST(LspSerializer_StringView, from_non_string_fails)
{
    Deserialize_Storage storage;
    EXPECT_FALSE(from_json_str<std::u8string_view>(u8"42", storage).has_value());
    EXPECT_FALSE(from_json_str<std::u8string_view>(u8"null", storage).has_value());
}

TEST(LspSerializer_StringView, to_json)
{
    EXPECT_EQ(to_json_str(u8"hello"sv), u8R"("hello")");
    EXPECT_EQ(to_json_str(u8""sv), u8R"("")");
}

TEST(LspSerializer_JsonValue, from_any_value)
{
    Deserialize_Storage storage;
    const auto result = from_json_str<json::Value>(u8"42", storage);
    ASSERT_TRUE(result.has_value());
    ASSERT_NE(result->as_number(), nullptr);
    EXPECT_DOUBLE_EQ(*result->as_number(), 42.0);
}

TEST(LspSerializer_JsonValue, from_object)
{
    Deserialize_Storage storage;
    const auto result = from_json_str<json::Value>(u8R"({"x":1})", storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->as_object(), nullptr);
}

TEST(LspSerializer_JsonValue, to_json_passthrough)
{
    EXPECT_EQ(to_json_str(json::Value { json::Number(7.0) }), u8"7");
    EXPECT_EQ(to_json_str(json::Value { true }), u8"true");
}

TEST(LspSerializer_ErrorCode, from_known_code)
{
    Deserialize_Storage storage;
    const auto result = from_json_str<Error_Code>(u8"-32700", storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, Error_Code::parse_error);
}

TEST(LspSerializer_ErrorCode, from_string_fails)
{
    Deserialize_Storage storage;
    EXPECT_FALSE(from_json_str<Error_Code>(u8R"("parse_error")", storage).has_value());
}

TEST(LspSerializer_ErrorCode, to_json)
{
    EXPECT_EQ(to_json_str(Error_Code::parse_error), u8"-32700");
    EXPECT_EQ(to_json_str(Error_Code::method_not_found), u8"-32601");
}

TEST(LspSerializer_DiagnosticSeverity, from_integer)
{
    Deserialize_Storage storage;
    EXPECT_EQ(from_json_str<Diagnostic_Severity>(u8"1", storage), Diagnostic_Severity::error);
    EXPECT_EQ(from_json_str<Diagnostic_Severity>(u8"2", storage), Diagnostic_Severity::warning);
    EXPECT_EQ(from_json_str<Diagnostic_Severity>(u8"3", storage), Diagnostic_Severity::information);
    EXPECT_EQ(from_json_str<Diagnostic_Severity>(u8"4", storage), Diagnostic_Severity::hint);
}

TEST(LspSerializer_DiagnosticSeverity, from_string_fails)
{
    Deserialize_Storage storage;
    EXPECT_FALSE(from_json_str<Diagnostic_Severity>(u8R"("error")", storage).has_value());
}

TEST(LspSerializer_DiagnosticSeverity, to_json)
{
    EXPECT_EQ(to_json_str(Diagnostic_Severity::error), u8"1");
    EXPECT_EQ(to_json_str(Diagnostic_Severity::warning), u8"2");
    EXPECT_EQ(to_json_str(Diagnostic_Severity::information), u8"3");
    EXPECT_EQ(to_json_str(Diagnostic_Severity::hint), u8"4");
}

TEST(LspSerializer_Span, from_empty_array)
{
    Deserialize_Storage storage;
    const auto result = from_json_str<std::span<const Integer>>(u8"[]", storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TEST(LspSerializer_Span, from_integer_array)
{
    Deserialize_Storage storage;
    const auto result = from_json_str<std::span<const Integer>>(u8"[1,2,3]", storage);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 3u);
    EXPECT_EQ((*result)[0], 1);
    EXPECT_EQ((*result)[1], 2);
    EXPECT_EQ((*result)[2], 3);
}

TEST(LspSerializer_Span, from_heterogeneous_array_fails)
{
    Deserialize_Storage storage;
    EXPECT_FALSE(from_json_str<std::span<const Integer>>(u8R"([1,"x"])", storage).has_value());
}

TEST(LspSerializer_Span, from_non_array_fails)
{
    Deserialize_Storage storage;
    EXPECT_FALSE(from_json_str<std::span<const Integer>>(u8"null", storage).has_value());
}

TEST(LspSerializer_Span, to_json_empty)
{
    const std::span<const Integer> empty {};
    EXPECT_EQ(to_json_str(empty), u8"[]");
}

TEST(LspSerializer_Span, to_json_integers)
{
    const std::array<Integer, 3> arr { 10, 20, 30 };
    EXPECT_EQ(to_json_str(std::span<const Integer>(arr)), u8"[10,20,30]");
}

TEST(LspSerializer_Variant, from_integer_alternative)
{
    Deserialize_Storage storage;
    using V = std::variant<Integer, std::u8string_view>;
    const auto result = from_json_str<V>(u8"7", storage);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<Integer>(*result));
    EXPECT_EQ(std::get<Integer>(*result), 7);
}

TEST(LspSerializer_Variant, from_string_alternative)
{
    Deserialize_Storage storage;
    using V = std::variant<Integer, std::u8string_view>;
    const auto result = from_json_str<V>(u8R"("hello")", storage);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<std::u8string_view>(*result));
    EXPECT_EQ(std::get<std::u8string_view>(*result), u8"hello"sv);
}

TEST(LspSerializer_Variant, to_json_integer_alternative)
{
    using V = std::variant<Integer, std::u8string_view>;
    EXPECT_EQ(to_json_str(V { Integer(99) }), u8"99");
}

TEST(LspSerializer_Variant, to_json_string_alternative)
{
    using V = std::variant<Integer, std::u8string_view>;
    EXPECT_EQ(to_json_str(V { u8"abc"sv }), u8R"("abc")");
}

TEST(LspPosition, deserialize)
{
    Deserialize_Storage storage;
    constexpr auto json = u8R"({"line":3,"character":7})";
    const auto result = from_json_str<Position>(json, storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, (Position { 3, 7 }));
}

TEST(LspPosition, deserialize_zero)
{
    Deserialize_Storage storage;
    constexpr auto json = u8R"({"line":0,"character":0})";
    const auto result = from_json_str<Position>(json, storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, (Position { 0, 0 }));
}

TEST(LspPosition, deserialize_missing_field_fails)
{
    Deserialize_Storage storage;
    EXPECT_FALSE(from_json_str<Position>(u8R"({"line":1})", storage).has_value());
    EXPECT_FALSE(from_json_str<Position>(u8R"({"character":1})", storage).has_value());
}

TEST(LspPosition, deserialize_non_object_fails)
{
    Deserialize_Storage storage;
    EXPECT_FALSE(from_json_str<Position>(u8"null", storage).has_value());
    EXPECT_FALSE(from_json_str<Position>(u8"[1,2]", storage).has_value());
}

TEST(LspPosition, serialize)
{
    const Position pos { 3, 7 };
    EXPECT_EQ(to_json_str(pos), u8R"({"line":3,"character":7})");
}

TEST(LspPosition, roundtrip)
{
    const Position original { 10, 42 };
    Deserialize_Storage storage;
    const auto result = from_json_str<Position>(to_json_str(original), storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, original);
}

TEST(LspRange, deserialize)
{
    Deserialize_Storage storage;
    constexpr auto json = u8R"({"start":{"line":1,"character":2},"end":{"line":3,"character":4}})";
    const auto result = from_json_str<Range>(json, storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, (Range { { 1, 2 }, { 3, 4 } }));
}

TEST(LspRange, deserialize_missing_start_fails)
{
    Deserialize_Storage storage;
    constexpr auto json = u8R"({"end":{"line":1,"character":0}})";
    EXPECT_FALSE(from_json_str<Range>(json, storage).has_value());
}

TEST(LspRange, serialize)
{
    const Range range { { 0, 0 }, { 1, 5 } };
    EXPECT_EQ(
        to_json_str(range), u8R"({"start":{"line":0,"character":0},"end":{"line":1,"character":5}})"
    );
}

TEST(LspRange, roundtrip)
{
    const Range original { { 2, 4 }, { 2, 8 } };
    Deserialize_Storage storage;
    const auto result = from_json_str<Range>(to_json_str(original), storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, original);
}

TEST(LspDiagnostic, deserialize_all_fields)
{
    Deserialize_Storage storage;
    constexpr auto json = u8R"({
        "range":{"start":{"line":0,"character":0},"end":{"line":0,"character":5}},
        "severity":1,
        "code":"E001",
        "source":"cowel",
        "message":"undeclared identifier"
    })";
    const auto result = from_json_str<Diagnostic>(json, storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->range, (Range { { 0, 0 }, { 0, 5 } }));
    EXPECT_EQ(result->severity, Diagnostic_Severity::error);
    EXPECT_EQ(result->code, u8"E001"sv);
    EXPECT_EQ(result->source, u8"cowel"sv);
    EXPECT_EQ(result->message, u8"undeclared identifier"sv);
}

TEST(LspDiagnostic, deserialize_optional_severity_absent)
{
    Deserialize_Storage storage;
    constexpr auto json = u8R"({
        "range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}},
        "code":"",
        "source":"",
        "message":"msg"
    })";
    const auto result = from_json_str<Diagnostic>(json, storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->severity.has_value());
}

TEST(LspDiagnostic, deserialize_severity_wrong_type_fails)
{
    Deserialize_Storage storage;
    constexpr auto json = u8R"({
        "range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}},
        "severity":"error",
        "code":"",
        "source":"",
        "message":"msg"
    })";
    EXPECT_FALSE(from_json_str<Diagnostic>(json, storage).has_value());
}

TEST(LspDiagnostic, deserialize_missing_required_field_fails)
{
    Deserialize_Storage storage;
    // Missing "range"
    constexpr auto json = u8R"({"code":"","source":"","message":"x"})";
    EXPECT_FALSE(from_json_str<Diagnostic>(json, storage).has_value());
}

TEST(LspDiagnostic, serialize_with_severity)
{
    const Diagnostic d {
        .range = { { 0, 0 }, { 0, 3 } },
        .severity = Diagnostic_Severity::warning,
        .code = u8"W01"sv,
        .source = u8"test"sv,
        .message = u8"warn"sv,
    };
    EXPECT_EQ(
        to_json_str(d),
        u8R"({"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":3}},"severity":2,"code":"W01","source":"test","message":"warn"})"
    );
}

TEST(LspDiagnostic, serialize_without_severity_omits_field)
{
    const Diagnostic d {
        .range = { { 1, 2 }, { 1, 5 } },
        .severity = std::nullopt,
        .code = u8""sv,
        .source = u8""sv,
        .message = u8"x"sv,
    };
    const std::u8string json = to_json_str(d);
    // "severity" key must not appear when the optional is empty
    EXPECT_EQ(json.find(u8"severity"), std::u8string::npos);
}

TEST(LspPublishDiagnosticsParams, deserialize_empty_diagnostics)
{
    Deserialize_Storage storage;
    constexpr auto json = u8R"({"uri":"file:///a.cow","diagnostics":[]})";
    const auto result = from_json_str<Publish_Diagnostics_Params>(json, storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->uri, u8"file:///a.cow"sv);
    EXPECT_TRUE(result->diagnostics.empty());
}

TEST(LspPublishDiagnosticsParams, serialize_empty)
{
    const Publish_Diagnostics_Params params { u8"file:///x.cow"sv, {} };
    EXPECT_EQ(to_json_str(params), u8R"({"uri":"file:///x.cow","diagnostics":[]})");
}

TEST(LspTextDocumentSyncOptions, deserialize)
{
    Deserialize_Storage storage;
    constexpr auto json = u8R"({"openClose":true,"change":1})";
    const auto result = from_json_str<Text_Document_Sync_Options>(json, storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, (Text_Document_Sync_Options { true, 1 }));
}

TEST(LspTextDocumentSyncOptions, serialize)
{
    const Text_Document_Sync_Options opts { false, 2 };
    EXPECT_EQ(to_json_str(opts), u8R"({"openClose":false,"change":2})");
}

TEST(LspTextDocumentSyncOptions, roundtrip)
{
    const Text_Document_Sync_Options original { true, 0 };
    Deserialize_Storage storage;
    const auto result = from_json_str<Text_Document_Sync_Options>(to_json_str(original), storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, original);
}

// ── Server_Capabilities ───────────────────────────────────────────────────────

TEST(LspServerCapabilities, deserialize)
{
    Deserialize_Storage storage;
    constexpr auto json
        = u8R"({"positionEncoding":"utf-8","textDocumentSync":{"openClose":true,"change":1},"hoverProvider":true})";
    const auto result = from_json_str<Server_Capabilities>(json, storage);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->position_encoding.has_value());
    EXPECT_EQ(*result->position_encoding, lsp::position_encoding_kind::utf8);
    EXPECT_EQ(result->text_document_sync, (Text_Document_Sync_Options { true, 1 }));
    EXPECT_EQ(result->hover_provider, true);
}

TEST(LspServerCapabilities, serialize)
{
    const Server_Capabilities caps {
        .position_encoding = lsp::position_encoding_kind::utf16,
        .text_document_sync = { false, 2 },
        .hover_provider = false,
    };
    EXPECT_EQ(
        to_json_str(caps),
        u8R"({"positionEncoding":"utf-16","textDocumentSync":{"openClose":false,"change":2},"hoverProvider":false})"
    );
}

TEST(LspServerCapabilities, serialize_no_position_encoding)
{
    const Server_Capabilities caps {
        .text_document_sync = { false, 2 },
        .hover_provider = true,
    };
    EXPECT_EQ(
        to_json_str(caps),
        u8R"({"textDocumentSync":{"openClose":false,"change":2},"hoverProvider":true})"
    );
}

TEST(LspServerInfo, deserialize)
{
    Deserialize_Storage storage;
    constexpr auto json = u8R"({"name":"cowel-lsp"})";
    const auto result = from_json_str<Server_Info>(json, storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->name, u8"cowel-lsp"sv);
}

TEST(LspServerInfo, deserialize_missing_name_fails)
{
    Deserialize_Storage storage;
    EXPECT_FALSE(from_json_str<Server_Info>(u8"{}", storage).has_value());
}

TEST(LspServerInfo, serialize)
{
    const Server_Info info { u8"my-server"sv };
    EXPECT_EQ(to_json_str(info), u8R"({"name":"my-server"})");
}

TEST(LspInitializeResult, serialize)
{
    const Initialize_Result res {
        .capabilities = {
            .position_encoding = lsp::position_encoding_kind::utf8,
            .text_document_sync = { true, 1 },
            .hover_provider = false,
        },
        .server_info = { u8"cowel-lsp"sv },
    };
    EXPECT_EQ(
        to_json_str(res),
        u8R"({"capabilities":{"positionEncoding":"utf-8","textDocumentSync":{"openClose":true,"change":1},"hoverProvider":false},"serverInfo":{"name":"cowel-lsp"}})"
    );
}

TEST(LspInitializeResult, roundtrip)
{
    const Initialize_Result original {
        .capabilities = {
            .position_encoding = lsp::position_encoding_kind::utf32,
            .text_document_sync = { false, 0 },
        },
        .server_info = { u8"srv"sv },
    };
    Deserialize_Storage storage;
    const auto result = from_json_str<Initialize_Result>(to_json_str(original), storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->capabilities, original.capabilities);
    EXPECT_EQ(result->server_info, original.server_info);
}

TEST(LspGeneralClientCapabilities, deserialize_with_encodings)
{
    Deserialize_Storage storage;
    constexpr auto json = u8R"({"positionEncodings":["utf-8","utf-16"]})";
    const auto result = from_json_str<General_Client_Capabilities>(json, storage);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->position_encodings.has_value());
    ASSERT_EQ(result->position_encodings->size(), 2u);
    EXPECT_EQ((*result->position_encodings)[0], lsp::position_encoding_kind::utf8);
    EXPECT_EQ((*result->position_encodings)[1], lsp::position_encoding_kind::utf16);
}

TEST(LspGeneralClientCapabilities, deserialize_missing_encodings)
{
    Deserialize_Storage storage;
    const auto result = from_json_str<General_Client_Capabilities>(u8"{}", storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->position_encodings.has_value());
}

TEST(LspClientCapabilities, deserialize_with_general)
{
    Deserialize_Storage storage;
    constexpr auto json = u8R"({"general":{"positionEncodings":["utf-8"]}})";
    const auto result = from_json_str<Client_Capabilities>(json, storage);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->general.has_value());
    ASSERT_TRUE(result->general->position_encodings.has_value());
    ASSERT_EQ(result->general->position_encodings->size(), 1u);
    EXPECT_EQ((*result->general->position_encodings)[0], lsp::position_encoding_kind::utf8);
}

TEST(LspClientCapabilities, deserialize_without_general)
{
    Deserialize_Storage storage;
    const auto result = from_json_str<Client_Capabilities>(u8"{}", storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->general.has_value());
}

TEST(LspInitializeParams, deserialize)
{
    Deserialize_Storage storage;
    constexpr auto json
        = u8R"({"capabilities":{"general":{"positionEncodings":["utf-8","utf-16"]}}})";
    const auto result = from_json_str<Initialize_Params>(json, storage);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->capabilities.has_value());
    ASSERT_TRUE(result->capabilities->general.has_value());
    ASSERT_TRUE(result->capabilities->general->position_encodings.has_value());
    EXPECT_EQ(result->capabilities->general->position_encodings->size(), 2u);
}

TEST(LspInitializeParams, deserialize_empty_capabilities)
{
    Deserialize_Storage storage;
    const auto result = from_json_str<Initialize_Params>(u8R"({"capabilities":{}})", storage);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->capabilities.has_value());
    EXPECT_FALSE(result->capabilities->general.has_value());
}

TEST(LspInitializeParams, deserialize_extra_fields_tolerated)
{
    // InitializeParams in the wild includes many other fields; they must be ignored.
    Deserialize_Storage storage;
    constexpr auto json
        = u8R"({"processId":42,"rootUri":"file:///foo","capabilities":{},"clientInfo":{"name":"test"}})";
    const auto result = from_json_str<Initialize_Params>(json, storage);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->capabilities.has_value());
}

TEST(LspInitializeParams, deserialize_absent_capabilities)
{
    Deserialize_Storage storage;
    const auto result = from_json_str<Initialize_Params>(u8"{}", storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->capabilities.has_value());
}

TEST(LspRequestMessage, deserialize_integer_id)
{
    Deserialize_Storage storage;
    constexpr auto json = u8R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"a":1}})";
    const auto result = from_json_str<Request_Message>(json, storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->jsonrpc, u8"2.0"sv);
    ASSERT_TRUE(std::holds_alternative<Integer>(result->id));
    EXPECT_EQ(std::get<Integer>(result->id), 1);
    EXPECT_EQ(result->method, u8"initialize"sv);
    ASSERT_NE(result->params, nullptr);
}

TEST(LspRequestMessage, deserialize_string_id)
{
    Deserialize_Storage storage;
    constexpr auto json = u8R"({"jsonrpc":"2.0","id":"req-1","method":"shutdown"})";
    const auto result = from_json_str<Request_Message>(json, storage);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<std::u8string_view>(result->id));
    EXPECT_EQ(std::get<std::u8string_view>(result->id), u8"req-1"sv);
    // params absent → nullptr
    EXPECT_EQ(result->params, nullptr);
}

TEST(LspRequestMessage, deserialize_missing_method_fails)
{
    Deserialize_Storage storage;
    constexpr auto json = u8R"({"jsonrpc":"2.0","id":1})";
    EXPECT_FALSE(from_json_str<Request_Message>(json, storage).has_value());
}

TEST(LspRequestMessage, serialize_without_params)
{
    const Request_Message msg {
        .jsonrpc = u8"2.0"sv,
        .id = Integer(3),
        .method = u8"shutdown"sv,
        .params = nullptr,
    };
    const std::u8string json = to_json_str(msg);
    // params pointer is null → omitted
    EXPECT_EQ(json.find(u8"params"), std::u8string::npos);
    EXPECT_NE(json.find(u8"shutdown"), std::u8string::npos);
}

TEST(LspResponseError, deserialize_no_data)
{
    Deserialize_Storage storage;
    constexpr auto json = u8R"({"code":-32600,"message":"Invalid Request"})";
    const auto result = from_json_str<Response_Error>(json, storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->code, Error_Code::invalid_request);
    EXPECT_EQ(result->message, u8"Invalid Request"sv);
    EXPECT_FALSE(result->data.has_value());
}

TEST(LspResponseError, deserialize_with_data)
{
    Deserialize_Storage storage;
    constexpr auto json = u8R"({"code":-32603,"message":"err","data":42})";
    const auto result = from_json_str<Response_Error>(json, storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->code, Error_Code::internal_error);
    ASSERT_TRUE(result->data.has_value());
    ASSERT_NE(result->data->as_number(), nullptr);
    EXPECT_DOUBLE_EQ(*result->data->as_number(), 42.0);
}

TEST(LspResponseError, serialize_without_data)
{
    const Response_Error err {
        .code = Error_Code::method_not_found,
        .message = u8"not found"sv,
        .data = {},
    };
    const std::u8string json = to_json_str(err);
    EXPECT_EQ(json.find(u8"\"data\""), std::u8string::npos);
    EXPECT_NE(json.find(u8"-32601"), std::u8string::npos);
}

TEST(LspResponseError, serialize_with_data)
{
    const Response_Error err {
        .code = Error_Code::internal_error,
        .message = u8"oops"sv,
        .data = json::Value { true },
    };
    EXPECT_EQ(to_json_str(err), u8R"({"code":-32603,"message":"oops","data":true})");
}

TEST(LspResponseMessage, deserialize_success)
{
    Deserialize_Storage storage;
    constexpr auto json = u8R"({"jsonrpc":"2.0","id":1,"result":null})";
    const auto result = from_json_str<Response_Message>(json, storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->jsonrpc, u8"2.0"sv);
    ASSERT_TRUE(std::holds_alternative<Integer>(result->id));
    EXPECT_EQ(std::get<Integer>(result->id), 1);
    ASSERT_TRUE(result->result.has_value());
    EXPECT_NE(result->result->as_null(), nullptr);
    EXPECT_FALSE(result->error.has_value());
}

TEST(LspResponseMessage, deserialize_null_id)
{
    Deserialize_Storage storage;
    constexpr auto json = u8R"({"jsonrpc":"2.0","id":null,"result":null})";
    const auto result = from_json_str<Response_Message>(json, storage);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<Null>(result->id));
}

TEST(LspResponseMessage, serialize_error)
{
    const Response_Message msg {
        .jsonrpc = u8"2.0"sv,
        .id = Integer(2),
        .result = {},
        .error = Response_Error { Error_Code::parse_error, u8"bad json"sv, {} },
    };
    const std::u8string json = to_json_str(msg);
    EXPECT_EQ(json.find(u8"result"), std::u8string::npos);
    EXPECT_NE(json.find(u8"-32700"), std::u8string::npos);
}

TEST(LspNotificationMessage, deserialize_without_params)
{
    Deserialize_Storage storage;
    constexpr auto json = u8R"({"jsonrpc":"2.0","method":"initialized"})";
    const auto result = from_json_str<Notification_Message>(json, storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->jsonrpc, u8"2.0"sv);
    EXPECT_EQ(result->method, u8"initialized"sv);
    EXPECT_FALSE(result->params.has_value());
}

TEST(LspNotificationMessage, deserialize_with_params)
{
    Deserialize_Storage storage;
    constexpr auto json = u8R"({"jsonrpc":"2.0","method":"exit","params":{}})";
    const auto result = from_json_str<Notification_Message>(json, storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->method, u8"exit"sv);
    ASSERT_TRUE(result->params.has_value());
    EXPECT_NE(result->params->as_object(), nullptr);
}

TEST(LspNotificationMessage, deserialize_missing_method_fails)
{
    Deserialize_Storage storage;
    constexpr auto json = u8R"({"jsonrpc":"2.0"})";
    EXPECT_FALSE(from_json_str<Notification_Message>(json, storage).has_value());
}

TEST(LspNotificationMessage, serialize_without_params)
{
    const Notification_Message msg {
        .jsonrpc = u8"2.0"sv,
        .method = u8"initialized"sv,
        .params = {},
    };
    const std::u8string json = to_json_str(msg);
    EXPECT_EQ(json.find(u8"params"), std::u8string::npos);
    EXPECT_NE(json.find(u8"initialized"), std::u8string::npos);
}

TEST(LspNotificationMessage, serialize_with_params)
{
    const Notification_Message msg {
        .jsonrpc = u8"2.0"sv,
        .method = u8"exit"sv,
        .params = json::Value { json::Null {} },
    };
    EXPECT_EQ(to_json_str(msg), u8R"({"jsonrpc":"2.0","method":"exit","params":null})");
}

TEST(LspNotificationMessage, roundtrip)
{
    const Notification_Message original {
        .jsonrpc = u8"2.0"sv,
        .method = u8"initialized"sv,
        .params = {},
    };
    Deserialize_Storage storage;
    const auto result = from_json_str<Notification_Message>(to_json_str(original), storage);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->jsonrpc, original.jsonrpc);
    EXPECT_EQ(result->method, original.method);
    EXPECT_EQ(result->params, original.params);
}

TEST(LspToResponseId, integer_id)
{
    const Request_Message::Id req_id = Integer(42);
    const Response_Message::Id resp_id = to_response_id(req_id);
    ASSERT_TRUE(std::holds_alternative<Integer>(resp_id));
    EXPECT_EQ(std::get<Integer>(resp_id), 42);
}

TEST(LspToResponseId, string_id)
{
    const Request_Message::Id req_id = u8"my-id"sv;
    const Response_Message::Id resp_id = to_response_id(req_id);
    ASSERT_TRUE(std::holds_alternative<std::u8string_view>(resp_id));
    EXPECT_EQ(std::get<std::u8string_view>(resp_id), u8"my-id"sv);
}

} // namespace
} // namespace cowel::lsp
