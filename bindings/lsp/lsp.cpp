/// @file lsp.cpp
/// @brief Standalone LSP server for COWEL.
///
/// Compiled as a WASM binary;
/// exposes a C API consumed by the TypeScript LSP runner in `bindings/node/`.

#include <algorithm>
#include <filesystem>
#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "cowel/util/code_point_names.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/to_chars.hpp"
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

/// @brief A hover entry: LSP range and Markdown article text.
struct Hover_Entry {
    lsp::Range range;
    std::u8string article;
};

struct Document {
    /// The UTF-8 text content of the document.
    std::u8string content {};
    /// The filesystem path of this document.
    std::u8string path {};
    /// The `file://` URI of this document.
    std::u8string uri {};
    /// Hover entries collected during validation.
    std::vector<Hover_Entry> hovers {};
    /// List of file URIs that were transitively included during its last validation run.
    /// Used to find which open documents need re-validation
    /// when an included file changes.
    std::vector<std::u8string> includes {};
    /// True when this is a disk-loaded document transiently inserted into `open_docs`
    /// for a validation run; such entries are erased when the run completes.
    bool transient = false;
};

/// @brief All mutable server state bundled to avoid scattered globals.
struct Server_State {
private:
    /// Documents currently open in the editor (URI → document).
    String_Map<Document> m_open_docs;
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

public:
    [[nodiscard]]
    explicit Server_State()
        = default;

    [[nodiscard]]
    const String_Map<Document>& get_open_docs() const
    {
        return m_open_docs;
    }

    [[nodiscard]]
    Document* find_open_document(const std::u8string_view uri)
    {
        const auto it = m_open_docs.find(uri);
        return it != m_open_docs.end() ? &it->second : nullptr;
    }

    [[nodiscard]]
    bool has_open_document(const std::u8string_view uri) const
    {
        return m_open_docs.find(uri) != m_open_docs.end();
    }
    void erase_open_document(const std::u8string_view uri)
    {
        const auto it = m_open_docs.find(uri);
        if (it != m_open_docs.end()) {
            m_open_docs.erase(it);
        }
    }
    void clear_transient_documents()
    {
        std::erase_if(m_open_docs, [](const auto& kv) { return kv.second.transient; });
    }

    /// @brief Inserts or updates an entry in `open_docs`.
    /// @returns An iterator to the inserted or updated entry.
    String_Map<Document>::iterator insert_or_assign_document(
        const std::u8string_view uri,
        const std::u8string_view path,
        const std::u8string_view text
    )
    {
        const auto it = m_open_docs.find(uri);
        if (it == m_open_docs.end()) {
            return m_open_docs
                .emplace(
                    uri,
                    Document {
                        .content { text },
                        .path { path },
                        .uri { uri },
                    }
                )
                .first;
        }
        it->second = Document {
            .content { text },
            .path { path },
            .uri { uri },
        };
        return it;
    }

    /// @brief Inserts a transient (disk-loaded) document into `open_docs`.
    /// @returns A reference to the inserted or pre-existing entry.
    Document& emplace_transient_document(
        const std::u8string_view uri,
        const std::u8string_view path,
        const std::u8string_view content
    )
    {
        return m_open_docs
            .emplace(
                uri,
                Document {
                    .content { content },
                    .path { path },
                    .uri { uri },
                    .transient = true,
                }
            )
            .first->second;
    }

    void append_output(const std::u8string_view s)
    {
        output += s;
    }
    void clear_output()
    {
        output.clear();
    }
    [[nodiscard]]
    std::u8string_view get_output() const
    {
        return output;
    }

    [[nodiscard]]
    bool uses_utf8_positions() const
    {
        return use_utf8_positions;
    }
    void set_use_utf8_positions(const bool value)
    {
        use_utf8_positions = value;
    }

    [[nodiscard]]
    bool is_exit_requested() const
    {
        return should_exit;
    }
    void request_exit()
    {
        should_exit = true;
    }

    void request_shutdown()
    {
        shutdown_requested = true;
    }

    /// @brief Converts a UTF-8 byte-offset column to an LSP `character` value.
    /// @param line_start_byte is the byte offset of the first byte of the line
    /// (i.e. `loc.begin - loc.column`).
    /// Uses `use_utf8_positions` to decide whether to return the byte offset unchanged
    /// or convert it to a UTF-16 code-unit count.
    [[nodiscard]]
    std::size_t column_to_character(
        const std::u8string_view bytes,
        const std::size_t line_start_byte,
        const std::size_t utf8_column
    ) const
    {
        if (use_utf8_positions) {
            return utf8_column;
        }
        const std::size_t safe_start = std::min(line_start_byte, bytes.size());
        const std::size_t safe_end = std::min(line_start_byte + utf8_column, bytes.size());
        return unchecked_utf8_to_utf16_length(bytes.substr(safe_start, safe_end - safe_start));
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

    server_state.append_output(u8"Content-Length: "sv);
    server_state.append_output(to_characters8(body.size()));
    server_state.append_output(u8"\r\n\r\n"sv);
    server_state.append_output(body);
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
    constexpr auto prefix = u8"file://"sv;
    std::u8string result { prefix };
    result.reserve(prefix.length() + (path.length() * 3 / 2));
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

/// @brief Per-validation context, passed as `data` into COWEL callbacks.
struct Validation_Context {
    /// Filesystem path of the main document.
    std::u8string main_path;
    /// Pointers to included files, indexed by `cowel_file_id` (>= 0).
    /// Each points into `server_state.open_docs`
    /// (either a pre-existing editor document or a transient disk-loaded one).
    std::vector<Document*> includes;
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
        : validation_context.includes[std::size_t(relative_to)]->path;

    const std::filesystem::path resolved
        = (std::filesystem::path { base_path }.parent_path() / std::string { path_sv })
              .lexically_normal();
    const std::u8string resolved_str = resolved.u8string();

    const std::u8string uri = path_to_uri(resolved_str);

    // Check if this file is open in the editor (in-memory override).
    if (Document* const doc = server_state.find_open_document(uri)) {
        const int id = static_cast<int>(validation_context.includes.size());
        validation_context.includes.push_back(doc);
        return {
            COWEL_IO_OK,
            { doc->content.data(), doc->content.size() },
            id,
        };
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

    // Insert the file into server state as a transient document for the duration of
    // this validation run; it will be erased by validate_document once compilation completes.
    const int id = static_cast<int>(validation_context.includes.size());
    Document& transient_doc = server_state.emplace_transient_document(
        uri, resolved_str, { static_cast<const char8_t*>(content_ptr), content_len }
    );
    cowel_free(content_ptr, content_len, 1);
    validation_context.includes.push_back(&transient_doc);
    return {
        COWEL_IO_OK,
        { transient_doc.content.data(), transient_doc.content.size() },
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

/// @brief Computes the LSP end position by scanning `loc.length` bytes
/// starting at `loc.begin` in @p bytes, counting newlines
/// and converting to UTF-8 or UTF-16 code units according to the server's position encoding.
[[nodiscard]]
lsp::Position compute_end_position(const std::u8string_view bytes, const Diagnostic_Location& loc)
{
    const std::size_t from = std::min(loc.begin, bytes.size());
    const std::size_t to = std::min(loc.begin + loc.length, bytes.size());
    const std::size_t line_start_byte = (from >= loc.column) ? from - loc.column : 0;
    lsp::Position result {
        loc.line,
        server_state.column_to_character(bytes, line_start_byte, loc.column),
    };
    for (std::size_t i = from; i < to; ++i) {
        if (bytes[i] == u8'\n') {
            ++result.line;
            result.character = 0;
        }
        else if (server_state.uses_utf8_positions()) {
            ++result.character;
        }
        else if ((bytes[i] & 0xC0u) != 0x80u) {
            // Leading byte: count as 1 unit for BMP, 2 for supplementary planes.
            result.character += (bytes[i] >= char8_t { 0xF0u }) ? 2u : 1u;
        }
    }
    return result;
}

/// @brief Result of a single `validate_document` call.
struct Validate_Result {
    /// Diagnostics grouped by document URI.
    String_Map<std::vector<lsp::Diagnostic>> by_uri;
    /// `file://` URIs of every file transitively loaded during validation
    /// (the direct and indirect includes of the validated entry point).
    std::vector<std::u8string> included_uris;
};

/// @brief Runs COWEL on `content` (at `uri`) and returns a `Validate_Result`
/// containing a map from document URI to its LSP `Diagnostic[]` array
/// and the full transitive include closure.
[[nodiscard]]
Validate_Result validate_document(
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
        .flags = COWEL_GEN_FLAGS_COLLECT_HOVERS,
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

    cowel_gen_result_u8 gen_result = cowel_generate_html_u8(&opts);

    // Collect hover entries and map them by file URI using file_id.
    {
        String_Map<std::vector<Hover_Entry>> hover_by_uri;
        if (gen_result.hovers != nullptr) {
            for (std::size_t i = 0; i < gen_result.hovers_size; ++i) {
                const cowel_hover_u8& h = gen_result.hovers[i];

                // Determine which file this hover belongs to.
                std::u8string hover_uri { uri };
                std::u8string_view hover_bytes = content;

                if (h.file_id >= 0 && std::size_t(h.file_id) < validation_context.includes.size()) {
                    const Document* const include
                        = validation_context.includes[std::size_t(h.file_id)];
                    hover_uri = include->uri;
                    hover_bytes = include->content;
                }

                const std::size_t line_start_byte = h.begin - h.column;
                const lsp::Position start_pos {
                    .line = h.line,
                    .character
                    = server_state.column_to_character(hover_bytes, line_start_byte, h.column),
                };
                const lsp::Position end_pos {
                    .line = h.line,
                    .character = server_state.column_to_character(
                        hover_bytes, line_start_byte, h.column + h.length
                    ),
                };
                hover_by_uri[hover_uri].push_back(
                    {
                        .range = { start_pos, end_pos },
                        .article = { h.article, h.article_length },
                    }
                );
            }
        }
        // Store hovers for each URI they belong to,
        // but only for documents that are open in the editor.
        for (auto& [hover_uri, entries] : hover_by_uri) {
            if (Document* const doc = server_state.find_open_document(hover_uri)) {
                doc->hovers = std::move(entries);
            }
        }
    }
    // Free the generation result (both output HTML and hover buffer).
    cowel_free_gen_result_u8(&opts, &gen_result);

    static constexpr std::vector<lsp::Diagnostic> empty_diagnostics;

    // Copies a string into context.memory so the resulting view remains
    // stable for the lifetime of the request,
    // even after validation_context is destroyed.
    const auto copy_sv = [&](const std::u8string_view s) -> std::u8string_view {
        if (s.empty()) {
            return {};
        }
        auto* const buf = static_cast<char8_t*>(context.memory.allocate(s.size(), 1));
        std::ranges::copy(s, buf);
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
                const Document* const include
                    = validation_context.includes[std::size_t(primary.file_id)];
                diagnostic_uri = include->uri;
                diagnostic_bytes = include->content;
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
                start_pos.character = server_state.column_to_character(
                    diagnostic_bytes, line_start_byte, loc.column
                );
                end_pos = compute_end_position(diagnostic_bytes, loc);
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

    Validate_Result result;
    result.by_uri = std::move(by_uri);
    result.included_uris.reserve(validation_context.includes.size());
    for (const Document* const include : validation_context.includes) {
        result.included_uris.push_back(include->uri);
    }
    server_state.clear_transient_documents();

    return result;
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
        if (const Document* const doc = server_state.find_open_document(config_uri)) {
            config_content = doc->content;
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

[[nodiscard]]
String_Map<std::vector<lsp::Diagnostic>> collect_diagnostics_by_uri(
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
        auto [doc_by_uri, included_uris] = validate_document(uri, content, context);
        // Pre-insert empty diagnostic entries for open included documents
        // that have no current errors,
        // so any previously published diagnostics for those files are cleared.
        for (const std::u8string& inc_uri : included_uris) {
            if (server_state.has_open_document(inc_uri)) {
                doc_by_uri.try_emplace(inc_uri, empty_diagnostics);
            }
        }
        by_uri = std::move(doc_by_uri);
        if (Document* const doc = server_state.find_open_document(uri)) {
            doc->includes = std::move(included_uris);
        }
        return by_uri;
    }

    if (entry_point_paths->empty()) {
        // Config found but has no include entries:
        // validation is disabled; publish empty diagnostics to clear stale errors.
        by_uri.emplace(uri, empty_diagnostics);
        return by_uri;
    }

    // Config-driven: validate config entry points instead.
    // Pre-insert empty diagnostics for the opened document
    // so stale errors are cleared even if it is not an entry point.
    by_uri.emplace(uri, empty_diagnostics);

    for (const std::u8string& entry_point_path : *entry_point_paths) {
        const std::u8string entry_point_uri = path_to_uri(entry_point_path);

        // Load entry point content: prefer open_docs (editor overrides),
        // then fall back to the filesystem via cowel_lsp_host_read_file.
        std::u8string ep_content_storage;
        std::u8string_view ep_content;
        if (const Document* const doc = server_state.find_open_document(entry_point_uri)) {
            ep_content = doc->content;
        }
        else {
            void* text = nullptr;
            std::size_t length = 0;
            const bool read_file_success = cowel_lsp_host_read_file(
                entry_point_path.data(), entry_point_path.size(), &text, &length
            );
            if (!read_file_success) {
                continue;
            }
            ep_content_storage.assign(static_cast<const char8_t*>(text), length);
            cowel_free(text, length, 1);
            ep_content = ep_content_storage;
        }

        auto [ep_by_uri, ep_included_uris]
            = validate_document(entry_point_uri, ep_content, context);
        if (Document* const doc = server_state.find_open_document(entry_point_uri)) {
            doc->includes = std::move(ep_included_uris);
        }
        for (const auto& [d_uri, d_diags] : ep_by_uri) {
            std::vector<lsp::Diagnostic>& slot
                = by_uri.try_emplace(d_uri, empty_diagnostics).first->second;
            for (const lsp::Diagnostic& d : d_diags) {
                slot.push_back(d);
            }
        }
    }

    return by_uri;
}

/// @brief Runs COWEL on `uri` and publishes `textDocument/publishDiagnostics`
/// for the document and any included files.
void publish_diagnostics_for(
    const std::u8string_view uri,
    const std::u8string_view content,
    Request_Context& context
)
{
    const String_Map<std::vector<lsp::Diagnostic>> by_uri
        = collect_diagnostics_by_uri(uri, content, context);

    // Sort by URI for deterministic output order:
    struct Diagnostics_Entry {
        std::u8string_view uri;
        const std::vector<lsp::Diagnostic>* diagnostics;
    };
    const auto sorted_by_uri = [&] {
        std::vector<Diagnostics_Entry> result;
        result.reserve(by_uri.size());
        for (const auto& [d_uri, d_diags] : by_uri) {
            result.emplace_back(d_uri, &d_diags);
        }
        std::ranges::sort(result, {}, &Diagnostics_Entry::uri);
        return result;
    }();
    for (const auto& [document_uri, diags_ptr] : sorted_by_uri) {
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

struct URI_And_Content {
    std::u8string_view uri;
    std::u8string_view content;
};

/// @brief Returns a list of opened root documents that depend on the given document URI.
/// This includes both the document specified by `included_uri` (if one is open),
/// as well as any documents that include it (`cowel_include`).
[[nodiscard]]
std::vector<URI_And_Content> get_open_dependent_roots(const std::u8string_view included_uri)
{
    // Collect URIs that appear in any open document's known include closure.
    // Documents in this set are compiled as fragments via their includer,
    // not as entry points, so they are not roots.
    std::vector<std::u8string_view> non_root_uris;
    for (const auto& [doc_uri, doc] : server_state.get_open_docs()) {
        non_root_uris.insert(non_root_uris.end(), doc.includes.begin(), doc.includes.end());
    }
    std::ranges::sort(non_root_uris);

    std::vector<URI_And_Content> roots;
    for (const auto& [doc_uri, doc] : server_state.get_open_docs()) {
        if (std::ranges::binary_search(non_root_uris, doc_uri)) {
            // fragment — compiled as part of its includer
            continue;
        }
        if (doc_uri != included_uri) {
            if (!std::ranges::contains(doc.includes, included_uri)) {
                continue;
            }
        }
        roots.push_back({ doc_uri, doc.content });
    }
    return roots;
}

/// @brief Validates every open "root" document affected by a change to `uri`.
/// A root document is an open document that does not appear in the transitive
/// include closure of any other open document;
/// it is compiled as an entry point rather than as a fragment.
///
/// Roots whose known include closure contains `uri`,
/// or that ARE `uri`, are re-validated exactly once,
/// avoiding redundant compilation of shared included files.
void revalidate_from(const std::u8string_view uri, Request_Context& context)
{
    for (const URI_And_Content& r : get_open_dependent_roots(uri)) {
        publish_diagnostics_for(r.uri, r.content, context);
    }
}

/// @brief Converts an LSP `Position` to a byte offset in `text`.
/// Respects the server's negotiated position encoding
/// (UTF-8 bytes or UTF-16 code units).
[[nodiscard]]
std::size_t lsp_position_to_byte_offset(const std::u8string_view text, const lsp::Position pos)
{
    // Walk to the start of the target line.
    std::size_t i = 0;
    for (std::size_t line = 0; line < pos.line && i < text.size(); ++i) {
        if (text[i] == u8'\n') {
            ++line;
        }
    }

    if (server_state.uses_utf8_positions()) {
        return std::min(i + pos.character, text.size());
    }
    // UTF-16 mode: limit the search to the current line, then convert.
    std::size_t line_end = i;
    while (line_end < text.size() && text[line_end] != u8'\n') {
        ++line_end;
    }
    return i + unchecked_utf16_offset_to_utf8_offset(text.substr(i, line_end - i), pos.character);
}

/// @brief Looks backward from `cursor_byte` in `text` for a named code-point
/// escape prefix of the form `\'<NAME>`.
/// @returns The typed prefix (text between `\'` and `cursor_byte`),
/// or `std::nullopt` if the cursor is not inside a named escape.
[[nodiscard]]
std::optional<std::u8string_view>
extract_named_escape_prefix(const std::u8string_view text, const std::size_t cursor_byte)
{
    static constexpr Charset256 is_code_point_name_char
        = is_ascii_upper_alpha_set | is_ascii_digit_set | u8' ' | u8'-';

    for (std::size_t pos = cursor_byte; pos > 0; --pos) {
        const char8_t c = text[pos - 1];
        if (c == u8'\'') {
            if (pos >= 2 && text[pos - 2] == u8'\\') {
                return text.substr(pos, cursor_byte - pos);
            }
            return {};
        }
        if (!is_code_point_name_char(c)) {
            return {};
        }
    }
    return {};
}

/// @brief Scans forward from `cursor_byte` through code-point name characters
/// to find the closing `'\''` of a named escape on the same line.
/// @returns The byte offset of the `'\''` if one is reachable via only
/// code-point name characters without crossing a line boundary;
/// otherwise `std::nullopt` (no existing closing quote).
[[nodiscard]]
std::optional<std::size_t>
find_named_escape_suffix_end(const std::u8string_view text, const std::size_t cursor_byte)
{
    static constexpr Charset256 is_code_point_name_char
        = is_ascii_upper_alpha_set | is_ascii_digit_set | u8' ' | u8'-';

    for (std::size_t pos = cursor_byte; pos < text.size(); ++pos) {
        const char8_t c = text[pos];
        if (c == u8'\'') {
            return pos;
        }
        if (!is_code_point_name_char(c)) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

void handle_completion(
    const lsp::Completion_Params& params,
    const lsp::Request_Message::Id& id,
    Request_Context& context
)
{
    static constexpr std::size_t max_completions = 50;
    static constexpr std::size_t max_name_length = 96;

    const std::u8string_view uri = params.text_document.uri;
    const Document* const doc = server_state.find_open_document(uri);
    if (doc == nullptr) {
        write_message(
            lsp::Response_Message {
                .id = lsp::to_response_id(id),
                .result = json::null,
            },
            context
        );
        return;
    }

    const std::size_t cursor_byte = lsp_position_to_byte_offset(doc->content, params.position);
    const std::optional<std::u8string_view> prefix
        = extract_named_escape_prefix(doc->content, cursor_byte);
    if (!prefix) {
        write_message(
            lsp::Response_Message {
                .id = lsp::to_response_id(id),
                .result = json::null,
            },
            context
        );
        return;
    }

    // Code-point names are pure ASCII,
    // so their byte length equals their UTF-16 code-unit count.
    // `prefix_char_count` is therefore valid for both position encodings.
    const std::size_t prefix_char_count = prefix->size();
    const lsp::Position prefix_start {
        .line = params.position.line,
        .character = params.position.character - prefix_char_count,
    };

    // Scan forward to see whether there is an existing closing quote to retain,
    // e.g. when the cursor is in the middle of `\'DIGIT ZERO'`.
    // All code-point name characters are ASCII, so byte count == char count.
    const std::optional<std::size_t> closing_quote_byte
        = find_named_escape_suffix_end(doc->content, cursor_byte);
    const std::size_t suffix_end_byte = closing_quote_byte.value_or(cursor_byte);
    const lsp::Position edit_end {
        .line = params.position.line,
        .character = params.position.character + (suffix_end_byte - cursor_byte),
    };

    // A local temporary buffer is fine because the maximum amount of results is small,
    // and `write_message` does not store the message.
    Fixed_String8<10> detail_strings[max_completions];
    Fixed_String8<4> sort_text_strings[max_completions];
    Fixed_String8<max_name_length> new_text_strings[max_completions];
    lsp::Completion_Item completion_items[max_completions];

    std::array<Code_Point_Prefix_Match, max_completions + 1> match_buffer {};
    const std::size_t raw_count = code_point_names_starting_with(match_buffer, *prefix);
    const std::size_t count = std::min(raw_count, max_completions);

    for (std::size_t i = 0; i < count; ++i) {
        const std::u8string_view name = match_buffer[i].name;
        const auto cp = std::uint32_t(match_buffer[i].code_point);
        const auto hex = to_characters8(cp, 16, true);
        detail_strings[i] = Fixed_String8<10>(u8"U+");
        for (std::size_t pad = hex.size(); pad < 4; ++pad) {
            detail_strings[i].push_back(u8'0');
        }
        detail_strings[i].append(hex);
        const auto dec = to_characters8(i);
        sort_text_strings[i] = {};
        for (std::size_t pad = dec.size(); pad < 3; ++pad) {
            sort_text_strings[i].push_back(u8'0');
        }
        sort_text_strings[i].append(dec);
        // When the escape has no closing quote, the inserted text must supply one.
        std::u8string_view new_text = name;
        if (!closing_quote_byte.has_value()) {
            new_text_strings[i] = Fixed_String8<max_name_length>(name);
            if (new_text_strings[i].size() < max_name_length) {
                new_text_strings[i].push_back(u8'\'');
            }
            new_text = std::u8string_view(new_text_strings[i]);
        }
        completion_items[i] = lsp::Completion_Item {
            .label = name,
            .kind = lsp::CompletionItemKind::Text,
            .detail = std::u8string_view(detail_strings[i]),
            .sort_text = std::u8string_view(sort_text_strings[i]),
            .filter_text = name,
            .text_edit
            = lsp::Text_Edit { .range = { prefix_start, edit_end }, .new_text = new_text },
        };
    }

    const lsp::Completion_List completion_list {
        .is_incomplete = raw_count > count,
        .items = { completion_items, count },
    };
    write_message(
        lsp::Response_Message {
            .id = lsp::to_response_id(id),
            .result = lsp::to_json(completion_list, &context.memory),
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

    const Document* const doc = server_state.find_open_document(uri);
    if (doc == nullptr) {
        // No hover found at this position — return null per LSP spec.
        write_message(
            lsp::Response_Message {
                .id = lsp::to_response_id(id),
                .result = json::null,
                .error = {},
            },
            context
        );
        return;
    }

    for (const Hover_Entry& entry : doc->hovers) {
        const lsp::Range& range = entry.range;
        const std::u8string& article = entry.article;
        const bool after_start = pos.line > range.start.line
            || (pos.line == range.start.line && pos.character >= range.start.character);
        const bool before_end = pos.line < range.end.line
            || (pos.line == range.end.line && pos.character < range.end.character);
        if (!after_start || !before_end) {
            continue;
        }
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
    server_state.set_use_utf8_positions(use_utf8);
    const auto position_encoding
        = use_utf8 ? lsp::position_encoding_kind::utf8 : std::optional<std::u8string_view> {};

    static constexpr lsp::Text_Document_Sync_Options sync_options {
        .open_close = true,
        .change = 1,
    };
    static constexpr lsp::Server_Info server_info = { .name = u8"cowel"sv };
    static constexpr std::array<std::u8string_view, 1> completion_trigger_chars { u8"'"sv };
    static constexpr lsp::Completion_Options completion_provider {
        .trigger_characters = std::span<const std::u8string_view> { completion_trigger_chars },
    };
    const lsp::Initialize_Result initialize_result {
        .capabilities = {
            .position_encoding = position_encoding,
            .text_document_sync = sync_options,
            .hover_provider = true,
            .completion_provider = completion_provider,
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
    const auto& [uri, doc] = *server_state.insert_or_assign_document(
        params.text_document.uri, uri_to_path(params.text_document.uri), params.text_document.text
    );
    revalidate_from(uri, context);
}

void handle_did_change(const lsp::Did_Change_Text_Document_Params& params, Request_Context& context)
{
    if (params.content_changes.empty()) {
        return;
    }
    const auto& [uri, doc] = *server_state.insert_or_assign_document(
        params.text_document.uri, uri_to_path(params.text_document.uri),
        params.content_changes.back().text
    );
    revalidate_from(uri, context);
}

void handle_did_close(const lsp::Did_Close_Text_Document_Params& params, Request_Context& context)
{
    const std::u8string_view uri = params.text_document.uri;
    server_state.erase_open_document(uri);
    clear_diagnostics_for(uri, context);
}

void dispatch(const lsp::Notification_Message& notification, Request_Context& context)
{
    if (notification.method == lsp::method::initialized) {
        return;
    }
    if (notification.method == lsp::method::exit) {
        server_state.request_exit();
        return;
    }

    if (notification.method == lsp::method::text_document::did_open) {
        if (notification.params && std::holds_alternative<json::Object>(*notification.params)) {
            if (const auto p = lsp::from_json<lsp::Did_Open_Text_Document_Params>(
                    std::get<json::Object>(*notification.params), context.storage
                )) {
                handle_did_open(*p, context);
            }
        }
        return;
    }
    if (notification.method == lsp::method::text_document::did_change) {
        if (notification.params && std::holds_alternative<json::Object>(*notification.params)) {
            if (const auto p = lsp::from_json<lsp::Did_Change_Text_Document_Params>(
                    std::get<json::Object>(*notification.params), context.storage
                )) {
                handle_did_change(*p, context);
            }
        }
        return;
    }
    if (notification.method == lsp::method::text_document::did_close) {
        if (notification.params && std::holds_alternative<json::Object>(*notification.params)) {
            if (const auto p = lsp::from_json<lsp::Did_Close_Text_Document_Params>(
                    std::get<json::Object>(*notification.params), context.storage
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
        server_state.request_shutdown();
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
    if (request.method == lsp::method::text_document::hover && request.params != nullptr) {
        if (const auto p = lsp::from_json<lsp::Hover_Params>(*request.params, context.storage)) {
            handle_hover(*p, request.id, context);
            return;
        }
    }
    if (request.method == lsp::method::text_document::completion && request.params != nullptr) {
        if (const auto p
            = lsp::from_json<lsp::Completion_Params>(*request.params, context.storage)) {
            handle_completion(*p, request.id, context);
            return;
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
    server_state.clear_output();
    process_message({ json, length });
}

/// @brief Returns a pointer into WASM memory holding the accumulated output bytes.
/// Valid until the next call to `lsp_process_message`.
COWEL_EXPORT
const char* cowel_lsp_get_output_ptr() noexcept
{
    return reinterpret_cast<const char*>(server_state.get_output().data());
}

/// @brief Returns the number of accumulated output bytes.
COWEL_EXPORT
std::size_t cowel_lsp_get_output_length() noexcept
{
    return server_state.get_output().size();
}

/// @brief Returns true if the LSP client sent an "exit" notification.
/// The caller should terminate the process after this returns true.
COWEL_EXPORT
bool cowel_lsp_should_exit() noexcept
{
    return server_state.is_exit_requested();
}

} // extern "C"

ULIGHT_DIAGNOSTIC_POP()

} // namespace cowel
