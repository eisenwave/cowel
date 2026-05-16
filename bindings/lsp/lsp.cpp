/// @file lsp.cpp
/// @brief Standalone LSP server for COWEL.
///
/// Reads JSON-RPC 2.0 messages (with LSP `Content-Length` framing) from stdin
/// and writes responses/notifications to stdout.
/// Intended to be compiled both natively and as a WASM node binary.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "cowel/util/from_chars.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/transparent_comparison.hpp"

#include "cowel/cowel.h"
#include "cowel/json.hpp"

#include "lsp.hpp"

namespace cowel {
namespace {

using namespace std::literals;

template <typename T>
using String_Map = std::unordered_map<
    std::u8string, //
    T, //
    Transparent_String_View_Hash8, //
    Transparent_String_View_Equals8>;

/// @brief All mutable server state bundled to avoid scattered globals.
struct Server_State {
    /// Documents currently open in the editor (URI → full content).
    String_Map<std::u8string> open_docs;
    /// Set to true after a "shutdown" request.
    bool shutdown_requested = false;
    /// Set to true after an LSP "exit" notification.
    bool should_exit = false;
    /// Accumulates LSP-framed output for the current message.
    /// Drained to stdout by the native main loop;
    /// read via lsp_get_output_ptr / lsp_get_output_length in WASM.
    std::u8string output;
};

Server_State server_state;

/// @brief Returns the hexadecimal digit value of @p c (0–15),
/// or -1 if @p c is not a valid hex digit.
[[nodiscard]]
constexpr int digit_value(const char8_t c) noexcept
{
    if (c >= u8'0' && c <= u8'9') {
        return c - u8'0';
    }
    if (c >= u8'a' && c <= u8'f') {
        return c - u8'a' + 10;
    }
    if (c >= u8'A' && c <= u8'F') {
        return c - u8'A' + 10;
    }
    return -1;
}

#ifndef COWEL_BUILD_WASM

struct Read_Message_Result {
    /// @brief The message body
    std::u8string body;
};

/// @brief Reads one LSP message from stdin.
/// Returns the body bytes, or `nullopt` on EOF or I/O error.
[[nodiscard]]
std::optional<std::u8string> read_message()
{
    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#headerPart
    int content_length = -1;
    std::u8string line;
    // Read headers one character at a time to handle CRLF without C++ streams.
    for (int ch; (ch = std::fgetc(stdin)) != EOF;) {
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            if (line.empty()) {
                break; // blank line terminates the header block
            }
            if (line.starts_with(lsp::header::content_length)) {
                const auto number_view
                    = std::u8string_view { line }.substr(lsp::header::content_length.size());
                const auto [ptr, ec] = from_characters(number_view, content_length);
                if (ec != std::errc {}) {
                    return {};
                }
            }
            line.clear();
            continue;
        }
        line.push_back(char8_t(ch));
    }
    if (content_length < 0 || std::feof(stdin) != 0 || std::ferror(stdin) != 0) {
        return std::nullopt;
    }
    std::u8string body(static_cast<std::size_t>(content_length), u8'\0');
    if (std::fread(body.data(), 1, static_cast<std::size_t>(content_length), stdin)
        != static_cast<std::size_t>(content_length)) {
        return std::nullopt;
    }
    return body;
}

#endif // !COWEL_BUILD_WASM

/// @brief Serialises @p msg as JSON, frames it with `Content-Length`, and
/// appends it to `server_state.output`.
void write_message(const json::Value& msg)
{
    std::pmr::u8string body;
    U8String_Consumer consumer { body };
    json::write_value(consumer, msg);
    const std::size_t n = body.size();
    server_state.output += u8"Content-Length: ";
    const std::string n_str = std::to_string(n);
    server_state.output.append(reinterpret_cast<const char8_t*>(n_str.data()), n_str.size());
    server_state.output += u8"\r\n\r\n";
    server_state.output.append(body.data(), n);
}

// ── File URI helpers ──────────────────────────────────────────────────────────

/// @brief Converts a `file://` URI to a filesystem path.
/// On Unix: `file:///a/b/c` → `/a/b/c`.
/// Percent-decodes byte sequences such as `%20` (space).
[[nodiscard]]
std::u8string uri_to_path(const std::u8string_view uri_in)
{
    constexpr auto scheme = u8"file://"sv;
    const std::u8string_view uri
        = uri_in.starts_with(scheme) ? uri_in.substr(scheme.length()) : uri_in;
    // Percent-decode only `%20` (space) and `%3A` (:) for practicality.
    std::u8string result;
    result.reserve(uri.size());
    for (std::size_t i = 0; i < uri.size(); ++i) {
        if (uri[i] == u8'%' && i + 2 < uri.size()) {
            const char8_t hi = uri[i + 1];
            const char8_t lo = uri[i + 2];
            const int hi_v = digit_value(hi);
            const int lo_v = digit_value(lo);
            if (hi_v >= 0 && lo_v >= 0) {
                result.push_back(char8_t((hi_v << 4) | lo_v));
                i += 2;
                continue;
            }
        }
        result.push_back(uri[i]);
    }
    return result;
}

/// @brief Converts a filesystem path to a `file://` URI.
/// Non-ASCII bytes and a small set of special characters are percent-encoded.
[[nodiscard]]
std::u8string path_to_uri(const std::u8string_view path)
{
    constexpr auto upper_hex = u8"0123456789ABCDEF"sv;
    std::u8string result = u8"file://"s;
    for (const char8_t c : path) {
        const auto uc = static_cast<unsigned char>(c);
        if (uc <= 0x20 || uc > 0x7e || uc == u8'#' || uc == u8'?' || uc == u8'%') {
            result.push_back(u8'%');
            result.push_back(upper_hex[(uc >> 4) & 0xFu]);
            result.push_back(upper_hex[uc & 0xFu]);
        }
        else {
            result.push_back(static_cast<char8_t>(uc));
        }
    }
    return result;
}

/// @brief Position information for one stack frame in a diagnostic.
struct Diagnostic_Location {
    std::u8string file_name;
    int file_id = COWEL_FILE_ID_MAIN;
    std::size_t begin = 0;
    std::size_t length = 0;
    std::size_t line = 0;
    std::size_t column = 0;
};

/// @brief A single diagnostic collected from the COWEL engine.
struct Diagnostic {
    cowel_severity severity {};
    std::u8string id;
    std::u8string message;
    std::vector<Diagnostic_Location> stack;
};

/// @brief A file loaded during a validation run.
struct Loaded_File {
    std::u8string path;
    std::u8string content;
};

/// @brief Per-validation context, passed as `data` into COWEL callbacks.
struct Validation_Context {
    /// Filesystem path of the main document.
    std::u8string main_path;
    /// Included files, indexed by `cowel_file_id` (>= 0).
    std::vector<Loaded_File> includes;
    /// Diagnostics collected by the `log` callback.
    std::vector<Diagnostic> diagnostics;

    /// Currently open in-editor documents (URI → content), for in-memory overrides.
    const String_Map<std::u8string>& open_docs;
};

/// @brief COWEL file-load callback.
/// Resolves @p rel_path relative to the file identified by @p relative_to,
/// preferring in-memory editor overrides when available.
cowel_file_result_u8 load_file_callback(
    const void* const data,
    const cowel_string_view_u8 rel_path,
    const cowel_file_id relative_to
) noexcept
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    auto& ctx = *static_cast<Validation_Context*>(const_cast<void*>(data));

    const std::string_view path_sv { reinterpret_cast<const char*>(rel_path.text),
                                     rel_path.length };

    // Resolve the base directory from which this include is relative.
    const std::u8string& base_path = (relative_to == COWEL_FILE_ID_MAIN)
        ? ctx.main_path
        : ctx.includes[std::size_t(relative_to)].path;

    const std::filesystem::path resolved
        = (std::filesystem::path { base_path }.parent_path() / std::string { path_sv })
              .lexically_normal();
    const std::u8string resolved_str = resolved.u8string();

    {
        // Check if this file is open in the editor (in-memory override).
        const std::u8string uri = path_to_uri(resolved_str);
        const auto it = ctx.open_docs.find(uri);
        if (it != ctx.open_docs.end()) {
            const int id = static_cast<int>(ctx.includes.size());
            ctx.includes.push_back({ resolved_str, it->second });
            const auto& entry = ctx.includes.back();
            return {
                COWEL_IO_OK,
                { entry.content.data(), entry.content.size() },
                id,
            };
        }
    }

    // Load from the filesystem (not available in WASM library mode).
#ifdef COWEL_BUILD_WASM
    return { COWEL_IO_ERROR_NOT_FOUND, {}, -1 };
#else
    std::FILE* const f = std::fopen(reinterpret_cast<const char*>(resolved_str.c_str()), "rb");
    if (f == nullptr) {
        return { COWEL_IO_ERROR_NOT_FOUND, {}, -1 };
    }

    std::fseek(f, 0, SEEK_END);
    const long file_size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    std::u8string content;
    if (file_size > 0) {
        content.resize(static_cast<std::size_t>(file_size));
        std::fread(content.data(), 1, content.size(), f);
    }
    std::fclose(f);

    const int id = static_cast<int>(ctx.includes.size());
    ctx.includes.push_back({ resolved_str, std::move(content) });
    const auto& entry = ctx.includes.back();
    return {
        COWEL_IO_OK,
        { entry.content.data(), entry.content.size() },
        id,
    };
#endif // COWEL_BUILD_WASM
}

/// @brief COWEL diagnostic callback.
/// Converts @p d into a `Diagnostic` and appends it to the context.
void log_callback(
    const void* const data, //
    const cowel_diagnostic_u8* const d
) noexcept
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    auto& context = *static_cast<Validation_Context*>(const_cast<void*>(data));

    Diagnostic diagnostic;
    diagnostic.severity = d->severity;
    diagnostic.id.assign(d->id.text, d->id.length);
    diagnostic.message.assign(d->message.text, d->message.length);

    for (std::size_t i = 0; i < d->stack_size; ++i) {
        const auto& frame = d->stack[i];
        Diagnostic_Location location;
        location.file_name.assign(frame.file_name.text, frame.file_name.length);
        location.file_id = frame.file_id;
        location.begin = frame.begin;
        location.length = frame.length;
        location.line = frame.line;
        location.column = frame.column;
        diagnostic.stack.push_back(std::move(location));
    }

    context.diagnostics.push_back(std::move(diagnostic));
}

/// @brief Returns the LSP `DiagnosticSeverity` for `sev`,
/// or `lsp_sev_skip` to suppress the diagnostic entirely.
[[nodiscard]]
std::optional<lsp::Diagnostic_Severity> cowel_severity_to_lsp_severity(const cowel_severity sev)
{
    if (sev >= COWEL_SEVERITY_ERROR && sev < COWEL_SEVERITY_NONE) {
        return lsp::Diagnostic_Severity::error;
    }
    if (sev >= COWEL_SEVERITY_WARNING && sev < COWEL_SEVERITY_ERROR) {
        return lsp::Diagnostic_Severity::warning;
    }
    if (sev >= COWEL_SEVERITY_SOFT_WARNING && sev < COWEL_SEVERITY_WARNING) {
        return lsp::Diagnostic_Severity::information;
    }
    return {};
}

/// @brief Computes the LSP end position by scanning `loc.length` bytes
/// starting at `loc.begin` in @p bytes, counting newlines.
/// @returns `{ end_line, end_character }` in UTF-8 code units.
[[nodiscard]]
lsp::Position compute_end_position(const std::u8string_view bytes, const Diagnostic_Location& loc)
{
    const std::size_t from = std::min(loc.begin, bytes.size());
    const std::size_t to = std::min(loc.begin + loc.length, bytes.size());
    lsp::Position result { loc.line, loc.column };
    for (std::size_t i = from; i < to; ++i) {
        if (bytes[i] == u8'\n') {
            ++result.line;
            result.character = 0;
        }
        else {
            ++result.character;
        }
    }
    return result;
}

// ── Validation ────────────────────────────────────────────────────────────────

/// @brief Runs COWEL on @p content (at @p uri) and returns a map from document
/// URI to its LSP `Diagnostic[]` JSON array.
[[nodiscard]]
String_Map<std::vector<lsp::Diagnostic>> validate_document(
    const std::u8string_view uri,
    const std::u8string_view content,
    const String_Map<std::u8string>& open_docs
)
{
    Validation_Context ctx {
        .main_path = uri_to_path(uri),
        .includes = {},
        .diagnostics = {},
        .open_docs = open_docs,
    };

    const cowel_options_u8 opts {
        .source = { content.data(), content.size() },
        .highlight_theme_json = {},
        .mode = COWEL_MODE_DOCUMENT,
        .min_log_severity = COWEL_SEVERITY_SOFT_WARNING,
        .preserved_variables = nullptr,
        .preserved_variables_size = 0,
        .consume_variables = nullptr,
        .consume_variables_data = nullptr,
        .alloc = nullptr,
        .alloc_data = nullptr,
        .free = nullptr,
        .free_data = nullptr,
        .load_file = load_file_callback,
        .load_file_data = &ctx,
        .log = log_callback,
        .log_data = &ctx,
        .highlighter = nullptr,
        .highlight_policy = COWEL_SYNTAX_HIGHLIGHT_POLICY_FALL_BACK,
        .preamble = {},
    };

    const cowel_gen_result_u8 gen = cowel_generate_html_u8(&opts);
    if (gen.output.text != nullptr) {
        cowel_free(static_cast<void*>(gen.output.text), gen.output.length, alignof(char8_t));
    }

    // Build the result map, pre-inserting an empty array for the main document
    // so it is always published (clears stale diagnostics after a fix).
    String_Map<std::vector<lsp::Diagnostic>> by_uri;
    by_uri.emplace(uri, std::vector<lsp::Diagnostic> {});

    for (const auto& diagnostic : ctx.diagnostics) {
        const std::optional<lsp::Diagnostic_Severity> severity
            = cowel_severity_to_lsp_severity(diagnostic.severity);
        if (!severity) {
            continue;
        }

        // Determine which document this diagnostic belongs to.
        std::u8string diagnostic_uri { uri };
        std::u8string_view diagnostic_bytes = content;

        if (!diagnostic.stack.empty()) {
            const auto& primary = diagnostic.stack[0];
            if (primary.file_id >= 0 && std::size_t(primary.file_id) < ctx.includes.size()) {
                const auto& include = ctx.includes[std::size_t(primary.file_id)];
                diagnostic_uri = path_to_uri(include.path);
                diagnostic_bytes = include.content;
            }
            else if (!primary.file_name.empty()) {
                diagnostic_uri = primary.file_name.starts_with(u8"file://"sv)
                    ? primary.file_name
                    : path_to_uri(primary.file_name);
            }
        }

        const auto range = [&] -> lsp::Range {
            lsp::Position start_pos {};
            lsp::Position end_pos {};
            if (!diagnostic.stack.empty()) {
                const auto& loc = diagnostic.stack[0];
                start_pos.line = loc.line;
                start_pos.character = loc.column;
                end_pos = compute_end_position(diagnostic_bytes, loc);
            }
            return { start_pos, end_pos };
        }();

        const lsp::Diagnostic lsp_diagnostic {
            .range = range,
            .severity = severity,
            .code = diagnostic.id,
            .source = u8"cowel"sv,
            .message = u8"message"sv,
        };
        auto emplace_result = by_uri.try_emplace(diagnostic_uri, std::vector<lsp::Diagnostic> {});
        emplace_result.first->second.push_back(lsp_diagnostic);
    }

    return by_uri;
}

/// @brief Runs COWEL on @p uri and publishes `textDocument/publishDiagnostics`
/// for the document and any included files.
void publish_diagnostics_for(const std::u8string_view uri, const std::u8string_view content)
{
    std::pmr::monotonic_buffer_resource mem;
    auto by_uri = validate_document(uri, content, server_state.open_docs);
    for (const auto& [document_uri, diagnostics] : by_uri) {
        write_message(
            lsp::Notification_Message {
                .method = lsp::method::text_document::publish_diagnostics,
                .params = lsp::Publish_Diagnostics_Params {
                    .uri = document_uri,
                    .diagnostics = diagnostics,
                }
                              .to_json(&mem),
            }
                .to_json(&mem)
        );
    }
}

/// @brief Sends an empty `publishDiagnostics` for @p uri to clear any
/// previously published diagnostics.
void clear_diagnostics_for(const std::u8string_view uri)
{
    std::pmr::monotonic_buffer_resource mem;
    const auto params
        = lsp::Publish_Diagnostics_Params { .uri = uri, .diagnostics = {} }.to_json(&mem);
    write_message(
            lsp::Notification_Message {
                .method = lsp::method::text_document::publish_diagnostics,
                .params = params,
            }
                .to_json(&mem)
        );
}

/// @brief Handles the "initialize" request: sends server capabilities.
void handle_initialize(const json::Value* const id)
{
    std::pmr::monotonic_buffer_resource mem;
    write_message(
        lsp::Response_Message {
            .id = lsp::clone_json_rpc_id(id, &mem),
            .result = lsp::Initialize_Result {}.to_json(&mem),
            .error = {},
        }
            .to_json(&mem)
    );
}

/// @brief Handles "textDocument/didOpen":
/// records the document and publishes diagnostics.
void handle_did_open(const json::Object& params)
{
    const auto* const text_document = params.find_object(u8"textDocument"sv);
    if (text_document == nullptr) {
        return;
    }
    const auto* const uri_string = text_document->find_string(u8"uri"sv);
    const auto* const text_string = text_document->find_string(u8"text"sv);
    if (uri_string == nullptr || text_string == nullptr) {
        return;
    }

    const std::u8string uri { uri_string->data(), uri_string->size() };
    server_state.open_docs[uri] = std::u8string { text_string->data(), text_string->size() };
    publish_diagnostics_for(uri, server_state.open_docs[uri]);
}

/// @brief Handles "textDocument/didChange":
/// updates the document and re-publishes diagnostics.
void handle_did_change(const json::Object& params)
{
    const auto* const text_document = params.find_object(u8"textDocument"sv);
    if (text_document == nullptr) {
        return;
    }
    const auto* const uri_string = text_document->find_string(u8"uri"sv);
    if (uri_string == nullptr) {
        return;
    }

    const std::u8string uri { uri_string->data(), uri_string->size() };
    const auto* const changes = params.find_array(u8"contentChanges"sv);
    if (changes == nullptr || changes->empty()) {
        return;
    }

    // Full sync: take the last change's text.
    const auto* const last = changes->back().as_object();
    if (last == nullptr) {
        return;
    }
    const auto* const text_string = last->find_string(u8"text"sv);
    if (text_string == nullptr) {
        return;
    }

    server_state.open_docs[uri] = std::u8string { text_string->data(), text_string->size() };
    publish_diagnostics_for(uri, server_state.open_docs[uri]);
}

/// @brief Handles "textDocument/didClose":
/// removes the document from memory and clears its diagnostics.
void handle_did_close(const json::Object& params)
{
    const auto* const text_document = params.find_object(u8"textDocument"sv);
    if (text_document == nullptr) {
        return;
    }
    const auto* const uri_string = text_document->find_string(u8"uri"sv);
    if (uri_string == nullptr) {
        return;
    }

    const std::u8string uri { uri_string->data(), uri_string->size() };
    server_state.open_docs.erase(uri);
    clear_diagnostics_for(uri);
}

/// @brief Dispatches one parsed JSON-RPC request or notification object.
void dispatch(const json::Object& msg)
{
    std::pmr::monotonic_buffer_resource mem;
    const auto* const id = msg.find_value(u8"id"sv);
    const auto* const method_string = msg.find_string(u8"method"sv);
    const std::u8string_view method = method_string ? *method_string : u8""sv;

    if (method == lsp::method::initialize) {
        handle_initialize(id);
        return;
    }
    if (method == lsp::method::initialized) {
        return;
    }
    if (method == lsp::method::shutdown) {
        server_state.shutdown_requested = true;
        write_message(
            lsp::Response_Message {
                .id = lsp::clone_json_rpc_id(id, &mem),
                .result = json::null,
                .error = {},
            }
                .to_json(&mem)
        );
        return;
    }
    if (method == lsp::method::exit) {
        server_state.should_exit = true;
        return;
    }

    const auto* const params = msg.find_object(u8"params"sv);

    if (method == lsp::method::text_document::did_open) {
        if (params != nullptr) {
            handle_did_open(*params);
        }
        return;
    }
    if (method == lsp::method::text_document::did_change) {
        if (params != nullptr) {
            handle_did_change(*params);
        }
        return;
    }
    if (method == lsp::method::text_document::did_close) {
        if (params != nullptr) {
            handle_did_close(*params);
        }
        return;
    }
    if (id != nullptr) {
        write_message(
            lsp::Response_Message {
                .id = lsp::clone_json_rpc_id(id, &mem),
                .result = {},
                .error = lsp::Response_Error { json_rpc::Error_Code::method_not_found,
                                               u8"Method not found"sv },
            }
                .to_json(&mem)
        );
    }
}

/// @brief Parses and dispatches one JSON-RPC message body.
void process_message(const std::u8string_view body)
{
    std::pmr::monotonic_buffer_resource parse_mem;
    const auto parsed = json::load(body, &parse_mem);
    if (!parsed.has_value()) {
        std::pmr::monotonic_buffer_resource err_mem;
        write_message(
            lsp::Response_Message {
                .id = json::null,
                .result = {},
                .error
                = lsp::Response_Error { json_rpc::Error_Code::parse_error, u8"Parse error"sv },
            }
                .to_json(&err_mem)
        );
        return;
    }
    const auto* const obj = parsed->as_object();
    if (obj == nullptr) {
        std::pmr::monotonic_buffer_resource err_mem;
        write_message(
            lsp::Response_Message {
                .id = json::null,
                .result = {},
                .error = lsp::Response_Error { json_rpc::Error_Code::invalid_request,
                                               u8"Invalid Request"sv },
            }
                .to_json(&err_mem)
        );
        return;
    }
    dispatch(*obj);
}

} // namespace

// ── WASM API ──────────────────────────────────────────────────────────────────
// These are intentional module entry points exported via COWEL_EXPORT.
// No header declares them, so suppress -Wmissing-declarations for this block.
ULIGHT_DIAGNOSTIC_PUSH()
#pragma GCC diagnostic ignored "-Wmissing-declarations"

extern "C" {

/// @brief Processes one JSON-RPC message body (bytes only, no Content-Length
/// header). Clears any previous output, dispatches the message, and buffers
/// any response(s) for retrieval via `lsp_get_output_ptr`/`lsp_get_output_length`.
COWEL_EXPORT
void cowel_lsp_process_message(const char8_t* const json, const std::size_t length) noexcept
{
    server_state.output.clear();
    process_message({ json, length });
}

/// @brief Returns a pointer into WASM memory holding the accumulated output
/// bytes. Valid until the next call to `lsp_process_message`.
COWEL_EXPORT
const char* cowel_lsp_get_output_ptr() noexcept
{
    return reinterpret_cast<const char*>(server_state.output.data());
}

/// @brief Returns the number of accumulated output bytes.
COWEL_EXPORT
std::size_t cowel_lsp_get_output_length() noexcept
{
    return server_state.output.size();
}

/// @brief Returns true if the LSP client sent an "exit" notification.
/// The caller should terminate the process after this returns true.
COWEL_EXPORT
bool cowel_lsp_should_exit() noexcept
{
    return server_state.should_exit;
}

} // extern "C"

ULIGHT_DIAGNOSTIC_POP()

#ifndef COWEL_BUILD_WASM
namespace {

/// @brief LSP server entry point.
/// Reads LSP-framed messages from stdin until EOF or an "exit" notification,
/// dispatching each to the appropriate handler.
int main()
{
    while (true) {
        server_state.output.clear();
        const std::optional<std::u8string> body = read_message();
        if (!body.has_value()) {
            break; // EOF or I/O error
        }
        process_message(*body);
        std::fwrite(
            reinterpret_cast<const char*>(server_state.output.data()), 1,
            server_state.output.size(), stdout
        );
        std::fflush(stdout);
        if (server_state.should_exit) {
            return server_state.shutdown_requested ? 0 : 1;
        }
    }
    return 0;
}

} // namespace
#endif // !COWEL_BUILD_WASM

} // namespace cowel

#ifndef COWEL_BUILD_WASM
int main()
{
    return cowel::main();
}
#endif
