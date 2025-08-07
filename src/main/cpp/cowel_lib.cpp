#include <bit>
#include <cstddef>
#include <cstring>
#include <exception>
#include <memory_resource>
#include <new>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/policy/capture.hpp"
#include "cowel/policy/html.hpp"

#include "cowel/assets.hpp"
#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/context.hpp"
#include "cowel/cowel.h"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/document_generation.hpp"
#include "cowel/fwd.hpp"
#include "cowel/memory_resources.hpp"
#include "cowel/output_language.hpp"
#include "cowel/parse.hpp"
#include "cowel/services.hpp"
#include "cowel/ulight_highlighter.hpp"

namespace cowel {
namespace {

static_assert(std::is_same_v<cowel::File_Id, cowel_file_id>);

using cowel::as_u8string_view;

[[nodiscard]]
std::u8string_view as_u8string_view(cowel_string_view_u8 str)
{
    return { str.text, str.length };
}

[[nodiscard]]
std::u8string_view as_u8string_view(cowel_mutable_string_view_u8 str)
{
    return { str.text, str.length };
}

[[nodiscard]]
cowel_string_view as_cowel_string_view(std::string_view str)
{
    return { str.data(), str.length() };
}

[[nodiscard]]
cowel_string_view_u8 as_cowel_string_view(std::u8string_view str)
{
    return { str.data(), str.length() };
}

struct File_Loader_Less {
    using is_transparent = void;

    [[nodiscard]]
    bool operator()(const auto& x, const auto& y) const noexcept
    {
        return std::u8string_view { x.data(), x.size() }
        < std::u8string_view { y.data(), y.size() };
    }
};

struct Loaded_File {
    cowel_file_id id;
    std::pmr::vector<char8_t> text;
};

[[nodiscard]]
constexpr File_Load_Error io_status_to_load_error(cowel_io_status error)
{
    COWEL_ASSERT(error != COWEL_IO_OK);
    switch (error) {
    case COWEL_IO_ERROR_READ: return File_Load_Error::read_error;
    case COWEL_IO_ERROR_NOT_FOUND: return File_Load_Error::not_found;
    case COWEL_IO_ERROR_PERMISSIONS: return File_Load_Error::permissions;
    default: return File_Load_Error::error;
    }
}

struct File_Loader_From_Options final : File_Loader {
private:
    cowel_load_file_fn_u8* m_load_file;
    const void* m_load_file_data;

public:
    explicit File_Loader_From_Options(cowel_load_file_fn_u8* load_file, const void* load_file_data)
        : m_load_file { load_file }
        , m_load_file_data { load_file_data }
    {
    }

    explicit File_Loader_From_Options(const cowel_options_u8& options)
        : File_Loader_From_Options { options.load_file, options.load_file_data }
    {
    }

    [[nodiscard]]
    Result<File_Entry, File_Load_Error> load(std::u8string_view path) final
    {
        if (!m_load_file) {
            return File_Load_Error::error;
        }
        const cowel_file_result_u8 result
            = m_load_file(m_load_file_data, as_cowel_string_view(path));
        if (result.status != COWEL_IO_OK) {
            return io_status_to_load_error(result.status);
        }
        return File_Entry { .id = result.id,
                            .source = as_u8string_view(result.data),
                            .name = path };
    }
};

struct Logger_From_Options final : Logger {
private:
    cowel_log_fn_u8* m_log;
    const void* m_log_data;
    std::pmr::vector<char8_t> m_buffer;

public:
    explicit Logger_From_Options(
        cowel_log_fn_u8* log,
        const void* log_data,
        cowel_severity min_severity,
        std::pmr::memory_resource* memory
    )
        : Logger { Severity(min_severity) }
        , m_log { log }
        , m_log_data { log_data }
        , m_buffer { memory }
    {
    }

    explicit Logger_From_Options(const cowel_options_u8& options, std::pmr::memory_resource* memory)
        : Logger_From_Options { options.log, options.log_data, options.min_log_severity, memory }
    {
    }

    void operator()(Diagnostic diagnostic) final
    {
        if (!m_log) {
            return;
        }
        COWEL_ASSERT(m_buffer.empty());

        /// If `chars` is contiguous, simply returns the underlying `u8string_view`.
        /// Otherwise, spills the contents of `chars` into `m_buffer`.
        const auto char_sequence_to_sv = [&](Char_Sequence8 chars) -> cowel_string_view_u8 {
            const std::u8string_view result = chars.as_string_view();
            if (chars.empty() || !result.empty()) {
                return as_cowel_string_view(result);
            }
            const std::size_t initial_size = m_buffer.size();
            m_buffer.resize(initial_size + chars.size());
            chars.extract(std::span { m_buffer }.subspan(initial_size));
            COWEL_ASSERT(chars.empty());
            return as_cowel_string_view(as_u8string_view(m_buffer).substr(initial_size));
        };

        const cowel_diagnostic_u8 diagnostic_u8 {
            .severity = cowel_severity(diagnostic.severity),
            .id = char_sequence_to_sv(diagnostic.id),
            .message = char_sequence_to_sv(diagnostic.message),
            .file_name = {},
            .file_id = diagnostic.location.file,
            .begin = diagnostic.location.begin,
            .length = diagnostic.location.length,
            .line = diagnostic.location.line,
            .column = diagnostic.location.column,
        };
        m_log(m_log_data, &diagnostic_u8);
        m_buffer.clear();
    }
};

[[nodiscard]]
cowel_gen_result_u8 do_generate_html(const cowel_options_u8& options)
{
    Pointer_Memory_Resource pointer_memory { options };
    Global_Memory_Resource global_memory;
    auto* const memory = options.alloc && options.free
        ? static_cast<std::pmr::memory_resource*>(&pointer_memory)
        : static_cast<std::pmr::memory_resource*>(&global_memory);

    Vector_Text_Sink html_sink { Output_Language::html, memory };
    HTML_Content_Policy html_policy { html_sink };

    const Builtin_Directive_Set builtin_behavior {};

    File_Loader_From_Options file_loader { options.load_file, options.load_file_data };
    Logger_From_Options logger { options, memory };
    static constinit Ulight_Syntax_Highlighter highlighter;

    [[maybe_unused]]
    const auto try_log
        = [&](std::u8string_view message, Severity severity) {
              if (cowel_severity(severity) < options.min_log_severity) {
                  return;
              }
              logger(Diagnostic {
                  .severity = Severity::trace,
                  .id = u8"trace"sv,
                  .location = { {}, -1 },
                  .message = message,
              });
          };

    try_log(u8"Trace logging enabled."sv, Severity::trace);

    const auto source = as_u8string_view(options.source);

    const ast::Pmr_Vector<ast::Content> root_content = parse_and_build(
        source, File_Id {}, memory,
        [&](std::u8string_view id, File_Source_Span pos, std::u8string_view message) {
            logger(Diagnostic { Severity::error, id, pos, message });
        }
    );

    const std::u8string_view highlight_theme_source = options.highlight_theme_json.length == 0
        ? assets::wg21_json
        : as_u8string_view(options.highlight_theme_json);

    const Generation_Options gen_options { .error_behavior = &builtin_behavior.get_error_behavior(),
                                           .highlight_theme_source = highlight_theme_source,
                                           .file_loader = file_loader,
                                           .logger = logger,
                                           .highlighter = highlighter,
                                           .memory = memory };

    const Processing_Status status = run_generation(
        [&](Context& context) -> Processing_Status {
            const Macro_Name_Resolver macro_resolver { builtin_behavior.get_macro_behavior() };
            context.add_resolver(builtin_behavior);
            context.add_resolver(macro_resolver);

            if (options.mode == COWEL_MODE_MINIMAL) {
                return consume_all(html_policy, root_content, 0, context);
            }
            COWEL_ASSERT(options.mode == COWEL_MODE_DOCUMENT);
            return write_wg21_document(html_policy, root_content, context);
        },
        gen_options
    );

    if (html_sink->empty()) {
        return { .status = static_cast<cowel_processing_status>(status), .output = {} };
    }

    const cowel_mutable_string_view_u8 result
        = cowel_alloc_text_u8({ html_sink->data(), html_sink->size() });
    if (result.text == nullptr) {
        try_log(u8"Failed to allocate memory for the generated HTML."sv, Severity::fatal);
        return { .status = COWEL_PROCESSING_FATAL, .output = result };
    }

    return { .status = static_cast<cowel_processing_status>(status), .output = result };
}

[[maybe_unused]] [[noreturn]]
void uncaught_exception(const cowel_options& options)
{
    if (!options.log) {
        std::terminate();
    }
    constexpr std::string_view message = "Uncaught exception.";
    constexpr std::string_view id = "exception";

    const cowel_diagnostic diagnostic {
        .severity = COWEL_SEVERITY_MAX,
        .id = as_cowel_string_view(id),
        .message = as_cowel_string_view(message),
        .file_name = {},
        .file_id = 0,
        .begin = 0,
        .length = 0,
        .line = 0,
        .column = 0,
    };
    options.log(options.log_data, &diagnostic);
    std::terminate();
}

[[maybe_unused]] [[noreturn]]
void uncaught_exception_u8(const cowel_options_u8& options)
{
    if (!options.log) {
        std::terminate();
    }
    constexpr std::u8string_view message = u8"Uncaught exception.";
    constexpr std::u8string_view id = u8"exception";

    const cowel_diagnostic_u8 diagnostic {
        .severity = COWEL_SEVERITY_MAX,
        .id = as_cowel_string_view(id),
        .message = as_cowel_string_view(message),
        .file_name = {},
        .file_id = 0,
        .begin = 0,
        .length = 0,
        .line = 0,
        .column = 0,
    };
    options.log(options.log_data, &diagnostic);
    std::terminate();
}

thread_local cowel_assertion_handler_fn_u8* thread_local_assertion_handler;

void do_handle_assertion(const Assertion_Error& error)
{
    const std::u8string_view file_name
        = reinterpret_cast<const char8_t*>(error.location.file_name());
    const std::u8string_view function_name
        = reinterpret_cast<const char8_t*>(error.location.function_name());
    const cowel_assertion_error_u8 cowel_error {
        .type = cowel_assertion_type(error.type),
        .message = as_cowel_string_view(error.message),
        .file_name = as_cowel_string_view(file_name),
        .function_name = as_cowel_string_view(function_name),
        .line = error.location.line(),
        .column = error.location.column(),
    };
    thread_local_assertion_handler(&cowel_error);
};

void do_set_assertion_handler_u8(cowel_assertion_handler_fn_u8* handler) noexcept
{
    thread_local_assertion_handler = handler;
    assertion_handler = &do_handle_assertion;
}

} // namespace
} // namespace cowel

extern "C" {

void* cowel_alloc(size_t size, size_t alignment) noexcept
{
    return ::operator new(size, std::align_val_t(alignment), std::nothrow);
}

void cowel_free(void* pointer, size_t size, size_t alignment) noexcept
{
    ::operator delete(pointer, size, std::align_val_t(alignment));
}

cowel_mutable_string_view cowel_alloc_text(cowel_string_view text) noexcept
{
    void* const result = cowel_alloc(text.length, alignof(char));
    if (!result) {
        return {};
    }
    std::memcpy(result, text.text, text.length);
    return { static_cast<char*>(result), text.length };
}

cowel_mutable_string_view_u8 cowel_alloc_text_u8(cowel_string_view_u8 text) noexcept
{
    void* const result = cowel_alloc(text.length, alignof(char8_t));
    if (!result) {
        return {};
    }
    std::memcpy(result, text.text, text.length);
    return { static_cast<char8_t*>(result), text.length };
}

void cowel_set_assertion_handler_u8(cowel_assertion_handler_fn_u8* handler) noexcept
{
    cowel::do_set_assertion_handler_u8(handler);
}

cowel_gen_result cowel_generate_html(const cowel_options* options) noexcept
{
#ifdef ULIGHT_EXCEPTIONS
    try {
#endif
        COWEL_ASSERT(options != nullptr);
        const cowel_options_u8& options_u8 = std::bit_cast<cowel_options_u8>(*options);
        const cowel_gen_result_u8 result_u8 = cowel_generate_html_u8(&options_u8);
        return std::bit_cast<cowel_gen_result>(result_u8);
#ifdef ULIGHT_EXCEPTIONS
    } catch (...) {
        cowel::uncaught_exception(*options);
    }
#endif
}

cowel_gen_result_u8 cowel_generate_html_u8(const cowel_options_u8* options) noexcept
{
#ifdef ULIGHT_EXCEPTIONS
    try {
#endif
        COWEL_ASSERT(options != nullptr);
        return cowel::do_generate_html(*options);
#ifdef ULIGHT_EXCEPTIONS
    } catch (...) {
        cowel::uncaught_exception_u8(*options);
    }
#endif
}
}
