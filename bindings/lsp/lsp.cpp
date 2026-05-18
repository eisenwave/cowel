/// @file lsp.cpp
/// @brief Standalone LSP server for COWEL.
///
/// Compiled as a WASM binary;
/// exposes a C API consumed by the TypeScript LSP runner in `bindings/node/`.

#include <algorithm>
#include <deque>
#include <filesystem>
#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "cowel/util/strings.hpp"
#include "cowel/util/transparent_comparison.hpp"

#include "cowel/cowel.h"
#include "cowel/json.hpp"
#include "cowel/settings.hpp"

#include "cowel/lsp.hpp"

#ifndef COWEL_BUILD_WASM
#error "lsp.cpp must only be compiled as part of the WASM build"
#endif

extern "C" {

/// Called from the WASM module when a file needs to be loaded from the filesystem.
/// On success:
/// allocates WASM memory via `cowel_alloc`,
/// writes the file contents there,
/// stores the allocation address in `*out_ptr` and the byte count in `*out_len`.
/// Leaves the output parameters unchanged on failure.
/// @returns `true` iff loading the file succeeded.
COWEL_WASM_IMPORT("env", "cowel_lsp_host_read_file")
bool cowel_lsp_host_read_file(
    const char8_t* path_ptr,
    std::size_t path_len,
    void** out_ptr,
    std::size_t* out_len
);

/// Called from the WASM module to expand a glob pattern within a directory.
/// On success, allocates WASM memory via `cowel_alloc` and writes a sequence of
/// null-terminated absolute UTF-8 file paths into it,
/// storing the allocation address in `*out_ptr` and the total byte count in `*out_len`.
/// Leaves the output parameters unchanged on failure or when no files match.
/// @returns `true` iff at least one file was matched.
COWEL_WASM_IMPORT("env", "cowel_lsp_host_glob")
bool cowel_lsp_host_glob(
    const char8_t* dir_ptr,
    std::size_t dir_len,
    const char8_t* pattern_ptr,
    std::size_t pattern_len,
    void** out_ptr,
    std::size_t* out_len
);

/// Called from the assertion handler when an internal assertion fails.
/// Implementations should write a human-readable crash report to stderr
/// before the process terminates.
COWEL_WASM_IMPORT("env", "cowel_lsp_host_log_fatal")
void cowel_lsp_host_log_fatal(
    int type,
    const char8_t* message_ptr,
    std::size_t message_len,
    const char8_t* file_ptr,
    std::size_t file_len,
    const char8_t* func_ptr,
    std::size_t func_len,
    std::size_t line
);
}

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
    /// Per-URI hover entries collected during validation.
    String_Map<std::vector<std::pair<lsp::Range, std::u8string>>> hover_map;
    /// Set to true after a "shutdown" request.
    bool shutdown_requested = false;
    /// Set to true after an LSP "exit" notification.
    bool should_exit = false;
    /// Accumulates LSP-framed output for the current message.
    /// Drained to stdout by the native main loop;
    /// read via lsp_get_output_ptr / lsp_get_output_length in WASM.
    std::u8string output;
    /// When true, position `character` values are UTF-8 byte offsets;
    /// otherwise, they are UTF-16 code-unit offsets.
    bool use_utf8_positions = false;

    /// @brief Inserts or updates an entry in `open_docs`
    /// using transparent comparison to avoid a key allocation on update.
    /// @returns An iterator to the inserted or updated entry.
    String_Map<std::u8string>::iterator
    upsert_doc(const std::u8string_view uri, const std::u8string_view text)
    {
        auto it = open_docs.find(uri);
        if (it == open_docs.end()) {
            it = open_docs.emplace(std::u8string(uri), std::u8string(text)).first;
        }
        else {
            it->second.assign(text);
        }
        return it;
    }
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

/// @brief Temporary storage associated with handling a single request or notification.
/// The goal is to:
/// - Prevent expensive memory management,
//    by making all allocations throughout a request go through `memory`.
/// - Provide temporary persistence for various created `lsp::` structs
///   (these store string views and spans, and sometimes need a new backing container).
struct Request_Context {
    std::pmr::monotonic_buffer_resource memory;
    lsp::Deserialize_Storage storage;
};

void do_write_message(const json::Value& msg, Request_Context& context)
{
    const auto body = [&] -> std::pmr::u8string {
        std::pmr::u8string result { &context.memory };
        U8String_Consumer consumer { result };
        json::write_value(consumer, msg);
        return result;
    }();

    server_state.output += u8"Content-Length: "sv;
    server_state.output += to_characters8(body.size());
    server_state.output += u8"\r\n\r\n"sv;
    server_state.output += body;
}

void write_message(const lsp::Response_Message& msg, Request_Context& context)
{
    do_write_message(to_json(msg, &context.memory), context);
}

void write_message(const lsp::Notification_Message& msg, Request_Context& context)
{
    do_write_message(to_json(msg, &context.memory), context);
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
    /// This uses `deque` instead of `vector` to guarantee reference stability,
    /// which is convenient in some places.
    std::deque<Loaded_File> includes;
    /// Diagnostics collected by the `log` callback.
    std::vector<Diagnostic> diagnostics;
};

/// @brief COWEL file-load callback.
/// Resolves `rel_path` relative to the file identified by `relative_to`,
/// preferring in-memory editor overrides when available.
[[nodiscard]]
cowel_file_result_u8 load_file_callback(
    const void* const data,
    const cowel_string_view_u8 rel_path,
    const cowel_file_id relative_to
) noexcept
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    auto& validation_context = *static_cast<Validation_Context*>(const_cast<void*>(data));

    const std::string_view path_sv { reinterpret_cast<const char*>(rel_path.text),
                                     rel_path.length };

    // Resolve the base directory from which this include is relative.
    const std::u8string_view base_path = (relative_to == COWEL_FILE_ID_MAIN)
        ? validation_context.main_path
        : validation_context.includes[std::size_t(relative_to)].path;

    const std::filesystem::path resolved
        = (std::filesystem::path { base_path }.parent_path() / std::string { path_sv })
              .lexically_normal();
    const std::u8string resolved_str = resolved.u8string();

    {
        // Check if this file is open in the editor (in-memory override).
        const std::u8string uri = path_to_uri(resolved_str);
        const auto it = server_state.open_docs.find(uri);
        if (it != server_state.open_docs.end()) {
            const int id = static_cast<int>(validation_context.includes.size());
            validation_context.includes.push_back({ resolved_str, it->second });
            const auto& entry = validation_context.includes.back();
            return {
                COWEL_IO_OK,
                { entry.content.data(), entry.content.size() },
                id,
            };
        }
    }

    // Ask the host to read the file from the filesystem.
    void* content_ptr = nullptr;
    std::size_t content_len = 0;
    const bool load_success = cowel_lsp_host_read_file(
        resolved_str.data(), resolved_str.size(), &content_ptr, &content_len
    );
    if (!load_success) {
        return { COWEL_IO_ERROR_NOT_FOUND, {}, -1 };
    }

    const int id = static_cast<int>(validation_context.includes.size());
    std::u8string file_text(static_cast<const char8_t*>(content_ptr), content_len);
    validation_context.includes.push_back({ resolved_str, std::move(file_text) });
    cowel_free(content_ptr, content_len, 1);
    const auto& entry = validation_context.includes.back();
    return {
        COWEL_IO_OK,
        { entry.content.data(), entry.content.size() },
        id,
    };
}

/// @brief COWEL diagnostic callback.
/// Converts `d` into a `Diagnostic` and appends it to the context.
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

/// @brief Scans @p bytes for the byte offset of the start of the given @p line (0-based).
[[nodiscard]]
std::size_t find_line_start_byte(const std::u8string_view bytes, const std::size_t line) noexcept
{
    std::size_t pos = 0;
    for (std::size_t i = 0; i < line; ++i) {
        while (pos < bytes.size() && bytes[pos] != u8'\n') {
            ++pos;
        }
        if (pos < bytes.size()) {
            ++pos; // skip the '\n'
        }
    }
    return pos;
}

/// @brief Converts a UTF-8 byte-offset column to an LSP `character` value.
/// @param line_start_byte is the byte offset of the first byte of the line
/// (i.e. `loc.begin - loc.column`).
/// @param use_utf8 When true, the column is returned unchanged;
/// otherwise it is converted to a UTF-16 code-unit count.
[[nodiscard]]
std::size_t column_to_character(
    const std::u8string_view bytes,
    const std::size_t line_start_byte,
    const std::size_t utf8_column,
    const bool use_utf8
) noexcept
{
    if (use_utf8) {
        return utf8_column;
    }
    const std::size_t safe_start = std::min(line_start_byte, bytes.size());
    const std::size_t safe_end = std::min(line_start_byte + utf8_column, bytes.size());
    return unchecked_utf8_to_utf16_length(bytes.substr(safe_start, safe_end - safe_start));
}

/// @brief Computes the LSP end position by scanning `loc.length` bytes
/// starting at `loc.begin` in @p bytes, counting newlines
/// and converting to UTF-8 or UTF-16 code units according to @p use_utf8.
[[nodiscard]]
lsp::Position compute_end_position(
    const std::u8string_view bytes,
    const Diagnostic_Location& loc,
    const bool use_utf8
)
{
    const std::size_t from = std::min(loc.begin, bytes.size());
    const std::size_t to = std::min(loc.begin + loc.length, bytes.size());
    const std::size_t line_start_byte = (from >= loc.column) ? from - loc.column : 0;
    lsp::Position result {
        loc.line,
        column_to_character(bytes, line_start_byte, loc.column, use_utf8),
    };
    for (std::size_t i = from; i < to; ++i) {
        if (bytes[i] == u8'\n') {
            ++result.line;
            result.character = 0;
        }
        else if (use_utf8) {
            ++result.character;
        }
        else if ((bytes[i] & 0xC0u) != 0x80u) {
            // Leading byte: count as 1 unit for BMP, 2 for supplementary planes.
            result.character += (bytes[i] >= char8_t { 0xF0u }) ? 2u : 1u;
        }
    }
    return result;
}

/// @brief Runs COWEL on `content (at `uri`),
/// and returns a map from document URI to its LSP `Diagnostic[]` JSON array.
[[nodiscard]]
String_Map<std::vector<lsp::Diagnostic>> validate_document(
    const std::u8string_view uri,
    const std::u8string_view content,
    Request_Context& context
)
{
    Validation_Context validation_context {
        .main_path = uri_to_path(uri),
        .includes = {},
        .diagnostics = {},
    };

    const cowel_options_u8 opts {
        .source = { content.data(), content.size() },
        .highlight_theme_json = {},
        .mode = COWEL_MODE_DOCUMENT,
        .collect_hovers = true,
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
        .load_file_data = &validation_context,
        .log = log_callback,
        .log_data = &validation_context,
        .highlighter = nullptr,
        .highlight_policy = COWEL_SYNTAX_HIGHLIGHT_POLICY_FALL_BACK,
        .preamble = {},
    };

    const cowel_gen_result_u8 gen = cowel_generate_html_u8(&opts);
    if (gen.output.text != nullptr) {
        cowel_free(static_cast<void*>(gen.output.text), gen.output.length, alignof(char8_t));
    }

    // Collect hover entries and store them in server_state.hover_map keyed by URI.
    {
        std::vector<std::pair<lsp::Range, std::u8string>> hover_entries;
        if (gen.hovers != nullptr) {
            hover_entries.reserve(gen.hovers_size);
            for (std::size_t i = 0; i < gen.hovers_size; ++i) {
                const cowel_hover_u8& h = gen.hovers[i];
                const std::size_t line_start_byte
                    = find_line_start_byte(content, h.line);
                lsp::Position start_pos;
                start_pos.line = h.line;
                start_pos.character = column_to_character(
                    content, line_start_byte, h.column, server_state.use_utf8_positions
                );
                lsp::Position end_pos;
                end_pos.line = h.line;
                end_pos.character = column_to_character(
                    content, line_start_byte, h.column + h.length, server_state.use_utf8_positions
                );
                std::u8string article { h.article, h.article_length };
                hover_entries.emplace_back(lsp::Range { start_pos, end_pos }, std::move(article));
            }
            cowel_free(
                static_cast<void*>(gen.hovers), gen.hovers_alloc_size, alignof(cowel_hover_u8)
            );
        }
        server_state.hover_map[std::u8string(uri)] = std::move(hover_entries);
    }

    static constexpr std::vector<lsp::Diagnostic> empty_diagnostics;

    // Copies a string into context.memory so the resulting view remains
    // stable for the lifetime of the request,
    // even after validation_context is destroyed.
    const auto copy_sv = [&](const std::u8string_view s) -> std::u8string_view {
        if (s.empty()) {
            return {};
        }
        auto* const buf = static_cast<char8_t*>(context.memory.allocate(s.size(), 1));
        std::copy(s.begin(), s.end(), buf);
        return { buf, s.size() };
    };

    // Build the result map, pre-inserting an empty array for the main document
    // so it is always published (clears stale diagnostics after a fix).
    String_Map<std::vector<lsp::Diagnostic>> by_uri;
    by_uri.emplace(uri, empty_diagnostics);

    for (const auto& diagnostic : validation_context.diagnostics) {
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
            if (primary.file_id >= 0
                && std::size_t(primary.file_id) < validation_context.includes.size()) {
                const auto& include = validation_context.includes[std::size_t(primary.file_id)];
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
                const std::size_t line_start_byte
                    = (loc.begin >= loc.column) ? loc.begin - loc.column : 0;
                start_pos.line = loc.line;
                start_pos.character = column_to_character(
                    diagnostic_bytes, line_start_byte, loc.column, server_state.use_utf8_positions
                );
                end_pos
                    = compute_end_position(diagnostic_bytes, loc, server_state.use_utf8_positions);
            }
            return { start_pos, end_pos };
        }();

        const lsp::Diagnostic lsp_diagnostic {
            .range = range,
            .severity = severity,
            .code = copy_sv(diagnostic.id),
            .source = u8"cowel"sv,
            .message = copy_sv(diagnostic.message),
        };
        const auto [it, success] = by_uri.try_emplace(diagnostic_uri, empty_diagnostics);
        it->second.push_back(lsp_diagnostic);
    }

    return by_uri;
}

/// @brief Searches for `.cowel_config.json` starting in the directory of `doc_uri`
/// and walking up ancestor directories until one is found or the filesystem root is reached.
/// - Returns `std::nullopt` when no config file is found in any ancestor directory,
///   or when the nearest config has no `"include"` key;
///   in either case the file should be validated as its own entry point.
/// - Returns an empty vector when the nearest config has an `"include"` key
///   whose value is an empty array,
///   meaning validation is disabled for files under this config's scope.
/// - Otherwise returns the absolute filesystem paths matched by each glob pattern
///   in the `"include"` array.
[[nodiscard]]
std::optional<std::vector<std::u8string>> find_config_entry_points(const std::u8string_view doc_uri)
{
    constexpr auto config_file_name = ".cowel_config.json"sv;

    const std::u8string doc_path = uri_to_path(doc_uri);
    std::filesystem::path dir = std::filesystem::path(doc_path).parent_path();

    while (true) {
        const std::u8string config_path = (dir / config_file_name).u8string();

        // TODO: This always copies the config contents,
        //       and could be optimized to avoid that if the config is already among `open_docs`.
        std::u8string config_content;
        bool config_found = false;
        const std::u8string config_uri = path_to_uri(config_path);
        if (const auto it = server_state.open_docs.find(config_uri);
            it != server_state.open_docs.end()) {
            config_content = it->second;
            config_found = true;
        }
        else {
            void* config_ptr = nullptr;
            std::size_t config_len = 0;
            const bool load_success = cowel_lsp_host_read_file(
                config_path.data(), config_path.size(), &config_ptr, &config_len
            );
            if (load_success) {
                config_content.assign(static_cast<const char8_t*>(config_ptr), config_len);
                cowel_free(config_ptr, config_len, 1);
                config_found = true;
            }
        }

        if (config_found) {
            // Found the nearest config; parse the "include" array.
            std::pmr::monotonic_buffer_resource mem;
            const std::optional<json::Value> parsed = json::load(config_content, &mem);
            if (!parsed) {
                return std::vector<std::u8string> {};
            }
            const auto* obj = parsed->as_object();
            if (!obj) {
                return std::vector<std::u8string> {};
            }
            const auto* arr = obj->find_array(u8"include"sv);
            if (!arr) {
                // Config present but no "include" key: validate standalone.
                return std::nullopt;
            }

            // Expand each pattern via the host glob function.
            const std::u8string dir_str = dir.u8string();
            std::vector<std::u8string> result;
            for (const json::Value& v : *arr) {
                const auto* const pattern_str = v.as_string();
                if (!pattern_str) {
                    continue;
                }
                void* glob_ptr = nullptr;
                std::size_t glob_len = 0;
                const bool glob_success = cowel_lsp_host_glob(
                    dir_str.data(), dir_str.size(), //
                    pattern_str->data(), pattern_str->size(), //
                    &glob_ptr, &glob_len
                );
                if (!glob_success) {
                    continue;
                }
                const auto* p = static_cast<const char8_t*>(glob_ptr);
                const char8_t* const end = p + glob_len;
                while (p < end) {
                    const std::size_t len = std::char_traits<char8_t>::length(p);
                    result.emplace_back(p, len);
                    p += len + 1;
                }
                cowel_free(glob_ptr, glob_len, 1);
            }
            return result;
        }

        // Not found here; walk up to the parent directory.
        const std::filesystem::path parent = dir.parent_path();
        if (parent == dir) {
            break; // Reached the filesystem root.
        }
        dir = parent;
    }
    return std::nullopt; // No config found.
}

/// @brief Runs COWEL on `uri` and publishes `textDocument/publishDiagnostics`
/// for the document and any included files.
void publish_diagnostics_for(
    const std::u8string_view uri,
    const std::u8string_view content,
    Request_Context& context
)
{
    static constexpr std::vector<lsp::Diagnostic> empty_diagnostics;

    const std::optional<std::vector<std::u8string>> entry_point_paths
        = find_config_entry_points(uri);

    String_Map<std::vector<lsp::Diagnostic>> by_uri;
    if (!entry_point_paths) {
        // No config found: validate the opened document directly as entry point.
        by_uri = validate_document(uri, content, context);
    }
    else if (entry_point_paths->empty()) {
        // Config found but has no include entries:
        // validation is disabled; publish empty diagnostics to clear stale errors.
        by_uri.emplace(uri, empty_diagnostics);
    }
    else {
        // Config-driven: validate config entry points instead.
        // Pre-insert empty diagnostics for the opened document
        // so stale errors are cleared even if it is not an entry point.
        by_uri.emplace(uri, empty_diagnostics);

        for (const std::u8string& ep_path : *entry_point_paths) {
            const std::u8string ep_uri = path_to_uri(ep_path);

            // Load entry point content: prefer open_docs (editor overrides),
            // then fall back to the filesystem via cowel_lsp_host_read_file.
            std::u8string ep_content_storage;
            std::u8string_view ep_content;
            if (const auto it = server_state.open_docs.find(ep_uri);
                it != server_state.open_docs.end()) {
                ep_content = it->second;
            }
            else {
                void* ptr = nullptr;
                std::size_t len = 0;
                if (!cowel_lsp_host_read_file(ep_path.data(), ep_path.size(), &ptr, &len)) {
                    continue;
                }
                ep_content_storage.assign(static_cast<const char8_t*>(ptr), len);
                cowel_free(ptr, len, 1);
                ep_content = ep_content_storage;
            }

            String_Map<std::vector<lsp::Diagnostic>> ep_results
                = validate_document(ep_uri, ep_content, context);
            for (const auto& [d_uri, d_diags] : ep_results) {
                std::vector<lsp::Diagnostic>& slot
                    = by_uri.try_emplace(d_uri, empty_diagnostics).first->second;
                for (const lsp::Diagnostic& d : d_diags) {
                    slot.push_back(d);
                }
            }
        }
    }

    // Sort by URI for deterministic output order:
    struct Diagnostics_Entry {
        std::u8string_view uri;
        const std::vector<lsp::Diagnostic>* diagnostics;
    };
    const auto sorted = [&] {
        std::vector<Diagnostics_Entry> result;
        result.reserve(by_uri.size());
        for (const auto& [d_uri, d_diags] : by_uri) {
            result.emplace_back(d_uri, &d_diags);
        }
        std::ranges::sort(result, {}, &Diagnostics_Entry::uri);
        return result;
    }();
    for (const auto& [document_uri, diags_ptr] : sorted) {
        const lsp::Notification_Message message {
            .method = lsp::method::text_document::publish_diagnostics,
            .params = lsp::to_json(
                lsp::Publish_Diagnostics_Params {
                    .uri = document_uri,
                    .diagnostics = *diags_ptr,
                },
                &context.memory
            ),
        };
        write_message(message, context);
    }
}

/// @brief Sends an empty `publishDiagnostics` for `uri` to clear any
/// previously published diagnostics.
void clear_diagnostics_for(const std::u8string_view uri, Request_Context& context)
{
    const auto params = lsp::to_json(
        lsp::Publish_Diagnostics_Params { .uri = uri, .diagnostics = {} }, &context.memory
    );
    write_message(
        lsp::Notification_Message {
            .method = lsp::method::text_document::publish_diagnostics,
            .params = params,
        },
        context
    );
}

void handle_hover(
    const lsp::Hover_Params& params,
    const lsp::Request_Message::Id& id,
    Request_Context& context
)
{
    const std::u8string_view uri = params.text_document.uri;
    const lsp::Position pos = params.position;

    const auto it = server_state.hover_map.find(uri);
    if (it != server_state.hover_map.end()) {
        for (const auto& [range, article] : it->second) {
            const bool after_start = pos.line > range.start.line
                || (pos.line == range.start.line
                    && pos.character >= range.start.character);
            const bool before_end = pos.line < range.end.line
                || (pos.line == range.end.line
                    && pos.character < range.end.character);
            if (after_start && before_end) {
                const lsp::Hover hover_result {
                    .contents = {
                        .kind = lsp::markup_kind::markdown,
                        .value = article,
                    },
                    .range = range,
                };
                write_message(
                    lsp::Response_Message {
                        .id = lsp::to_response_id(id),
                        .result = lsp::to_json(hover_result, &context.memory),
                        .error = {},
                    },
                    context
                );
                return;
            }
        }
    }

    // No hover found at this position — return null per LSP spec.
    write_message(
        lsp::Response_Message {
            .id = lsp::to_response_id(id),
            .result = json::null,
            .error = {},
        },
        context
    );
}

void handle_initialize(
    const lsp::Initialize_Params& params,
    const lsp::Request_Message::Id& id,
    Request_Context& context
)
{
    // Negotiate position encoding.
    // The LSP spec mandates UTF-16 support; prefer UTF-8 if the client advertises it.
    const auto use_utf8 = [&] -> bool {
        if (const auto& caps = params.capabilities) {
            if (const auto& gen = caps->general) {
                if (const auto& encodings = gen->position_encodings) {
                    for (const std::u8string_view enc : *encodings) {
                        if (enc == lsp::position_encoding_kind::utf8) {
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }();
    server_state.use_utf8_positions = use_utf8;
    const auto position_encoding
        = use_utf8 ? lsp::position_encoding_kind::utf8 : std::optional<std::u8string_view> {};

    static constexpr lsp::Text_Document_Sync_Options sync_options {
        .open_close = true,
        .change = 1,
    };
    static constexpr lsp::Server_Info server_info = { .name = u8"cowel"sv };
    const lsp::Initialize_Result initialize_result {
        .capabilities = {
            .position_encoding = position_encoding,
            .text_document_sync = sync_options,
            .hover_provider = true,
        },
        .server_info = server_info,
    };
    write_message(
        lsp::Response_Message {
            .id = lsp::to_response_id(id),
            .result = lsp::to_json(initialize_result, &context.memory),
            .error = {},
        },
        context
    );
}

void handle_did_open(const lsp::Did_Open_Text_Document_Params& params, Request_Context& context)
{
    const auto it = server_state.upsert_doc(params.text_document.uri, params.text_document.text);
    publish_diagnostics_for(it->first, it->second, context);
}

void handle_did_change(const lsp::Did_Change_Text_Document_Params& params, Request_Context& context)
{
    if (params.content_changes.empty()) {
        return;
    }
    // Full sync: take the last change's text.
    const auto it
        = server_state.upsert_doc(params.text_document.uri, params.content_changes.back().text);
    publish_diagnostics_for(it->first, it->second, context);
}

void handle_did_close(const lsp::Did_Close_Text_Document_Params& params, Request_Context& context)
{
    const std::u8string_view uri = params.text_document.uri;
    if (const auto it = server_state.open_docs.find(uri); it != server_state.open_docs.end()) {
        server_state.open_docs.erase(it);
    }
    clear_diagnostics_for(uri, context);
}

void dispatch(const lsp::Notification_Message& notification, Request_Context& context)
{
    if (notification.method == lsp::method::initialized) {
        return;
    }
    if (notification.method == lsp::method::exit) {
        server_state.should_exit = true;
        return;
    }

    if (notification.method == lsp::method::text_document::did_open) {
        if (notification.params) {
            if (const auto p = lsp::from_json<lsp::Did_Open_Text_Document_Params>(
                    *notification.params, context.storage
                )) {
                handle_did_open(*p, context);
            }
        }
        return;
    }
    if (notification.method == lsp::method::text_document::did_change) {
        if (notification.params) {
            if (const auto p = lsp::from_json<lsp::Did_Change_Text_Document_Params>(
                    *notification.params, context.storage
                )) {
                handle_did_change(*p, context);
            }
        }
        return;
    }
    if (notification.method == lsp::method::text_document::did_close) {
        if (notification.params) {
            if (const auto p = lsp::from_json<lsp::Did_Close_Text_Document_Params>(
                    *notification.params, context.storage
                )) {
                handle_did_close(*p, context);
            }
        }
        return;
    }
    // Unknown notification methods are silently ignored;
    // there is no channel for response anyway.
}

void dispatch(const lsp::Request_Message& request, Request_Context& context)
{
    if (request.method == lsp::method::initialize) {
        lsp::Initialize_Params params {};
        if (request.params != nullptr) {
            if (const auto p
                = lsp::from_json<lsp::Initialize_Params>(*request.params, context.storage)) {
                params = *p;
            }
        }
        handle_initialize(params, request.id, context);
        return;
    }
    if (request.method == lsp::method::shutdown) {
        server_state.shutdown_requested = true;
        write_message(
            lsp::Response_Message {
                .id = lsp::to_response_id(request.id),
                .result = json::null,
                .error = {},
            },
            context
        );
        return;
    }
    if (request.method == lsp::method::text_document::hover) {
        if (request.params != nullptr) {
            if (const auto p
                = lsp::from_json<lsp::Hover_Params>(*request.params, context.storage)) {
                handle_hover(*p, request.id, context);
                return;
            }
        }
    }
    write_message(
        lsp::Response_Message {
            .id = lsp::to_response_id(request.id),
            .result = {},
            .error
            = lsp::Response_Error { lsp::Error_Code::method_not_found, u8"Method not found"sv },
        },
        context
    );
}

/// @brief Parses and dispatches one JSON-RPC message body.
void process_message(const std::u8string_view body)
{
    Request_Context context {};

    const auto parsed = json::load(body, &context.memory);
    if (!parsed.has_value()) {
        write_message(
            lsp::Response_Message {
                .id = json::null,
                .result = {},
                .error = lsp::Response_Error { lsp::Error_Code::parse_error, u8"Parse error"sv },
            },
            context
        );
        return;
    }
    const auto respond_to_invalid_request = [&](const lsp::Response_Message::Id id = lsp::null) {
        write_message(
            lsp::Response_Message {
                .id = id,
                .result = {},
                .error
                = lsp::Response_Error { lsp::Error_Code::invalid_request, u8"Invalid Request"sv },
            },
            context
        );
    };

    const auto* const obj = parsed->as_object();
    if (obj == nullptr) {
        respond_to_invalid_request();
        return;
    }
    const auto* const id_val = obj->find_value(u8"id"sv);
    lsp::Deserialize_Storage storage;

    if (id_val == nullptr) {
        // No id field: this is a notification.
        const std::optional<lsp::Notification_Message> notification
            = lsp::from_json<lsp::Notification_Message>(*parsed, storage);
        if (!notification) {
            respond_to_invalid_request();
            return;
        }
        dispatch(*notification, context);
    }
    else {
        // id field is present: this is a request.
        // Try to deserialize the full Request_Message.
        // If deserialization fails, echo the id back in the error response.
        const std::optional<lsp::Request_Message> request
            = lsp::from_json<lsp::Request_Message>(*parsed, storage);
        if (!request) {
            lsp::Response_Message::Id error_id = lsp::null;
            if (const auto* n = id_val->as_number()) {
                error_id = static_cast<lsp::Integer>(*n);
            }
            else if (const auto* s = id_val->as_string()) {
                error_id = std::u8string_view { s->data(), s->size() };
            }
            respond_to_invalid_request(error_id);
            return;
        }
        dispatch(*request, context);
    }
}

} // namespace

ULIGHT_DIAGNOSTIC_PUSH()
#pragma GCC diagnostic ignored "-Wmissing-declarations"

extern "C" {

/// @brief Registers the LSP assertion handler.
/// Must be called once after `_initialize()` to ensure
/// that internal assertion failures produce a diagnostic
/// via `cowel_lsp_host_log_fatal` instead of silently trapping.
COWEL_EXPORT
void cowel_lsp_register_assertion_handler() noexcept
{
    cowel_set_assertion_handler_u8([](const cowel_assertion_error_u8* const err) noexcept {
        cowel_lsp_host_log_fatal(
            static_cast<int>(err->type), err->message.text, err->message.length,
            err->file_name.text, err->file_name.length, err->function_name.text,
            err->function_name.length, err->line
        );
    });
}

/// @brief Processes one JSON-RPC message body (bytes only, no Content-Length
/// header). Clears any previous output, dispatches the message, and buffers
/// any response(s) for retrieval via `lsp_get_output_ptr`/`lsp_get_output_length`.
COWEL_EXPORT
void cowel_lsp_process_message(const char8_t* const json, const std::size_t length) noexcept
{
    server_state.output.clear();
    process_message({ json, length });
}

/// @brief Returns a pointer into WASM memory holding the accumulated output bytes.
/// Valid until the next call to `lsp_process_message`.
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

} // namespace cowel
