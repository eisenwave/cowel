#ifndef COWEL_LSP_HPP
#define COWEL_LSP_HPP

#include <optional>
#include <span>
#include <string_view>

#include "cowel/fwd.hpp"
#include "cowel/json.hpp"

namespace cowel::json_rpc {

/// @brief A JSON-RPC error code.
/// @see https://www.jsonrpc.org/specification §5.1
/// @see
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#errorCodes
enum struct Error_Code { // NOLINT(performance-enum-size)
    /// Invalid JSON was received by the server.
    /// An error occurred on the server while parsing the JSON text.
    parse_error = -32'700,
    /// The JSON sent is not a valid Request object.
    invalid_request = -32'600,
    /// The method does not exist / is not available.
    method_not_found = -32601,
    /// Invalid method parameter(s).
    invalid_params = -32602,
    /// Internal JSON-RPC error.
    internal_error = -32603,
};

} // namespace cowel::json_rpc

namespace cowel::lsp {

using namespace std::literals;

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#headerPart
namespace header {

inline constexpr auto content_length = u8"Content-Length: "sv;
inline constexpr auto content_type = u8"Content-Type: "sv;

} // namespace header

namespace method {

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#initialize
inline constexpr auto initialize = u8"initialize"sv;
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#initialized
inline constexpr auto initialized = u8"initialized"sv;
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#shutdown
inline constexpr auto shutdown = u8"shutdown"sv;
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#exit
inline constexpr auto exit = u8"exit"sv;

namespace text_document {

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_publishDiagnostics
inline constexpr auto publish_diagnostics = u8"textDocument/publishDiagnostics"sv;
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_didOpen
inline constexpr auto did_open = u8"textDocument/didOpen"sv;
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_didChange
inline constexpr auto did_change = u8"textDocument/didChange"sv;
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_didClose
inline constexpr auto did_close = u8"textDocument/didClose"sv;

} // namespace text_document
} // namespace method

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#position
namespace position_encoding_kind {

/// Character offsets count UTF-8 code units (e.g bytes).
inline constexpr auto utf8 = u8"utf-8"sv;
/// Character offsets count UTF-16 code units.
inline constexpr auto utf16 = u8"utf-16"sv;
/// Character offsets count UTF-32 code units.
inline constexpr auto utf32 = u8"utf-32"sv;

} // namespace position_encoding_kind

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#position
struct Position {
    /// Zero-based line index.
    std::size_t line;
    /// Zero-based character offset within the line.
    std::size_t character;

    [[nodiscard]]
    json::Object to_json(std::pmr::memory_resource* const memory) const
    {
        json::Object result { memory };
        result.push_back({ json::String { u8"line"sv, memory }, json::Number(line) });
        result.push_back({ json::String { u8"character"sv, memory }, json::Number(character) });
        return result;
    }
};

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#range
struct Range {
    /// Inclusive start position.
    Position start;
    /// Exclusive end position.
    Position end;

    [[nodiscard]]
    json::Object to_json(std::pmr::memory_resource* const memory) const
    {
        json::Object result { memory };
        result.push_back({ json::String { u8"start"sv, memory }, start.to_json(memory) });
        result.push_back({ json::String { u8"end"sv, memory }, end.to_json(memory) });
        return result;
    }
};

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnosticSeverity
enum class Diagnostic_Severity : Default_Underlying {
    error = 1,
    warning = 2,
    information = 3,
    hint = 4,
};

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnostic
struct Diagnostic {
    /// The range at which the message applies.
    Range range;
    /// The diagnostic's severity.
    std::optional<Diagnostic_Severity> severity;
    /// The diagnostic's code, which might appear in the user interface.
    std::u8string_view code;
    /// A human-readable string describing the source of this diagnostic.
    std::u8string_view source;
    /// The diagnostic's message.
    std::u8string_view message;

    [[nodiscard]]
    json::Object to_json(std::pmr::memory_resource* const memory) const
    {
        json::Object result { memory };
        result.push_back({ json::String { u8"range"sv, memory }, range.to_json(memory) });
        if (severity) {
            result.push_back({ json::String { u8"severity"sv, memory }, json::Number(*severity) });
        }
        result.push_back({ json::String { u8"code"sv, memory }, json::String { code, memory } });
        result.push_back(
            { json::String { u8"source"sv, memory }, json::String { source, memory } }
        );
        result.push_back(
            { json::String { u8"message"sv, memory }, json::String { message, memory } }
        );
        return result;
    }
};

struct Publish_Diagnostics_Params {
    std::u8string_view uri;
    std::span<const Diagnostic> diagnostics;

    [[nodiscard]]
    json::Object to_json(std::pmr::memory_resource* const memory) const
    {
        json::Object result { memory };
        result.push_back({ json::String { u8"uri"sv, memory }, json::String { uri, memory } });
        json::Array diagnostics_json { memory };
        diagnostics_json.reserve(diagnostics.size());
        for (const Diagnostic& diagnostic : diagnostics) {
            diagnostics_json.push_back(diagnostic.to_json(memory));
        }
        result.push_back({ json::String { u8"diagnostics"sv, memory }, std::move(diagnostics_json) });
        return result;
    }
};

[[nodiscard]]
inline json::Value clone_json_rpc_id(
    const json::Value* const id, std::pmr::memory_resource* const memory
)
{
    if (id == nullptr) {
        return json::null;
    }
    if (const auto* const n = id->as_number()) {
        return *n;
    }
    if (const auto* const s = id->as_string()) {
        return json::String { *s, memory };
    }
    return json::null;
}

struct Text_Document_Sync_Options {
    bool open_close = true;
    int change = 1;

    [[nodiscard]]
    json::Object to_json(std::pmr::memory_resource* const memory) const
    {
        json::Object result { memory };
        result.push_back({ json::String { u8"openClose"sv, memory }, open_close });
        result.push_back({ json::String { u8"change"sv, memory }, json::Number { change } });
        return result;
    }
};

struct Server_Capabilities {
    std::u8string_view position_encoding = position_encoding_kind::utf8;
    Text_Document_Sync_Options text_document_sync {};

    [[nodiscard]]
    json::Object to_json(std::pmr::memory_resource* const memory) const
    {
        json::Object result { memory };
        result.push_back(
            {
                json::String { u8"positionEncoding"sv, memory },
                json::String { position_encoding, memory },
            }
        );
        result.push_back(
            {
                json::String { u8"textDocumentSync"sv, memory },
                text_document_sync.to_json(memory),
            }
        );
        return result;
    }
};

struct Server_Info {
    std::u8string_view name;

    [[nodiscard]]
    json::Object to_json(std::pmr::memory_resource* const memory) const
    {
        json::Object result { memory };
        result.push_back({ json::String { u8"name"sv, memory }, json::String { name, memory } });
        return result;
    }
};

struct Initialize_Result {
    Server_Capabilities capabilities {};
    Server_Info server_info { .name = u8"cowel"sv };

    [[nodiscard]]
    json::Object to_json(std::pmr::memory_resource* const memory) const
    {
        json::Object result { memory };
        result.push_back({ json::String { u8"capabilities"sv, memory }, capabilities.to_json(memory) });
        result.push_back({ json::String { u8"serverInfo"sv, memory }, server_info.to_json(memory) });
        return result;
    }
};

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#requestMessage
struct Request_Message {
    std::u8string_view jsonrpc = u8"2.0"sv;
    /// The request id.
    std::u8string_view id;
    /// The method to be invoked.
    std::u8string_view method;
    /// The method's params.
    json::Value params;

    [[nodiscard]]
    json::Object to_json(std::pmr::memory_resource* const memory) const
    {
        json::Object result { memory };
        result.push_back(
            { json::String { u8"jsonrpc"sv, memory }, json::String { jsonrpc, memory } }
        );
        result.push_back(
            { json::String { u8"method"sv, memory }, json::String { method, memory } }
        );
        result.push_back({ json::String { u8"params"sv, memory }, params });
        return result;
    }
};

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#responseError
struct Response_Error {
    /// A number indicating the error type that occurred.
    json_rpc::Error_Code code;
    /// A string providing a short description of the error.
    std::u8string_view message;
    /// A primitive or structured value that contains additional information about the error.
    std::optional<json::Value> data = {};

    [[nodiscard]]
    json::Object to_json(std::pmr::memory_resource* const memory) const
    {
        json::Object result { memory };
        result.push_back({ json::String { u8"code"sv, memory }, json::Number(code) });
        result.push_back(
            { json::String { u8"message"sv, memory }, json::String { message, memory } }
        );
        if (data) {
            result.push_back({ json::String { u8"data"sv, memory }, *data });
        }
        return result;
    }
};

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#responseMessage
struct Response_Message {
    std::u8string_view jsonrpc = u8"2.0"sv;
    /// The request id, or `json::null` when unknown (e.g. parse errors).
    json::Value id = json::null;
    std::optional<json::Value> result;
    std::optional<Response_Error> error;

    [[nodiscard]]
    json::Object to_json(std::pmr::memory_resource* const memory) const
    {
        json::Object result { memory };
        result.push_back(
            { json::String { u8"jsonrpc"sv, memory }, json::String { jsonrpc, memory } }
        );
        result.push_back({ json::String { u8"id"sv, memory }, id });
        if (this->result) {
            result.push_back({ json::String { u8"result"sv, memory }, *this->result });
        }
        if (error) {
            result.push_back({ json::String { u8"error"sv, memory }, error->to_json(memory) });
        }
        return result;
    }
};

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#notificationMessage
struct Notification_Message {
    std::u8string_view jsonrpc = u8"2.0"sv;
    std::u8string_view method;
    std::optional<json::Value> params;

    [[nodiscard]]
    json::Object to_json(std::pmr::memory_resource* const memory) const
    {
        json::Object result { memory };
        result.push_back(
            { json::String { u8"jsonrpc"sv, memory }, json::String { jsonrpc, memory } }
        );
        result.push_back(
            { json::String { u8"method"sv, memory }, json::String { method, memory } }
        );
        if (params) {
            result.push_back({ json::String { u8"params"sv, memory }, *params });
        }
        return result;
    }
};

} // namespace cowel::lsp

#endif
