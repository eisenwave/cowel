#include <bit>
#include <cstddef>
#include <cstring>
#include <exception>
#include <memory_resource>
#include <new>
#include <span>
#include <string_view>
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
#include "cowel/cowel_lib.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/document_generation.hpp"
#include "cowel/fwd.hpp"
#include "cowel/memory_resources.hpp"
#include "cowel/output_language.hpp"
#include "cowel/parse.hpp"
#include "cowel/services.hpp"
#include "cowel/settings.hpp"
#include "cowel/ulight_highlighter.hpp"

namespace cowel {

Allocator_Options Allocator_Options::from_memory_resource( //
    std::pmr::memory_resource* const memory
) noexcept
{
    if (memory == nullptr) {
        return {};
    }

    constexpr auto alloc_fn = [](std::pmr::memory_resource* memory, std::size_t size,
                                 std::size_t alignment) noexcept -> void* {
#ifdef COWEL_EXCEPTIONS
        try {
#endif
            return memory->allocate(size, alignment);
#ifdef COWEL_EXCEPTIONS
        } catch (...) {
            return nullptr;
        }
#endif
    };
    constexpr auto free_fn = [](std::pmr::memory_resource* memory, void* pointer, std::size_t size,
                                std::size_t alignment) noexcept -> void {
        memory->deallocate(pointer, size, alignment);
    };

    const Function_Ref<void*(std::size_t, std::size_t) noexcept> alloc_ref
        = { const_v<alloc_fn>, memory };
    const Function_Ref<void(void*, std::size_t, std::size_t) noexcept> free_ref
        = { const_v<free_fn>, memory };

    return {
        .alloc = alloc_ref.get_invoker(),
        .alloc_data = alloc_ref.get_entity(),
        .free = free_ref.get_invoker(),
        .free_data = free_ref.get_entity(),
    };
}

namespace {

static_assert(int(File_Id::main) == COWEL_FILE_ID_MAIN);

/// If `chars` is contiguous, simply returns the underlying `u8string_view`.
/// Otherwise, spills the contents of `chars` into `buffer`.
cowel_string_view_u8 char_sequence_to_sv(Char_Sequence8 chars, std::pmr::vector<char8_t>& buffer)
{
    COWEL_ASSERT(buffer.empty());

    const std::u8string_view result = chars.as_string_view();
    if (chars.empty() || !result.empty()) {
        return as_cowel_string_view(result);
    }
    buffer.resize(chars.size());
    chars.extract(std::span { buffer });
    COWEL_ASSERT(chars.empty());
    return as_cowel_string_view(as_u8string_view(buffer));
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

struct File_Loader_From_Options final : File_Loader {
private:
    cowel_load_file_fn_u8* m_load_file;
    const void* m_load_file_data;
    std::pmr::vector<char8_t> m_buffer;
    cowel_file_id m_max_valid_file_id = COWEL_FILE_ID_MAIN;

public:
    explicit File_Loader_From_Options(
        cowel_load_file_fn_u8* load_file,
        const void* load_file_data,
        std::pmr::memory_resource* memory
    )
        : m_load_file { load_file }
        , m_load_file_data { load_file_data }
        , m_buffer { memory }
    {
    }

    explicit File_Loader_From_Options(
        const cowel_options_u8& options,
        std::pmr::memory_resource* memory
    )
        : File_Loader_From_Options {
            options.load_file,
            options.load_file_data,
            memory,
        }
    {
    }

    [[nodiscard]]
    Result<File_Entry, File_Load_Error> load(Char_Sequence8 path_chars, File_Id relative_to) final
    {
        COWEL_ASSERT(is_valid(relative_to));

        if (!m_load_file) {
            return File_Load_Error::error;
        }
        COWEL_ASSERT(m_buffer.empty());
        const cowel_string_view_u8 path = char_sequence_to_sv(path_chars, m_buffer);

        const cowel_file_result_u8 result
            = m_load_file(m_load_file_data, path, cowel_file_id(relative_to));
        m_max_valid_file_id = std::max(m_max_valid_file_id, result.id);

        if (result.status != COWEL_IO_OK) {
            return io_status_to_load_error(result.status);
        }
        return File_Entry { .id = File_Id(result.id),
                            .source = as_u8string_view(result.data),
                            .name = as_u8string_view(path) };
    }

    [[nodiscard]]
    bool is_valid(File_Id id) const noexcept final
    {
        return id <= File_Id(m_max_valid_file_id);
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

    void operator()(Diagnostic diagnostic) override
    {
        if (!m_log) {
            return;
        }
        COWEL_ASSERT(m_buffer.empty());

        const cowel_diagnostic_u8 diagnostic_u8 {
            .severity = cowel_severity(diagnostic.severity),
            .id = char_sequence_to_sv(diagnostic.id, m_buffer),
            .message = char_sequence_to_sv(diagnostic.message, m_buffer),
            .file_name = {},
            .file_id = cowel_file_id(diagnostic.location.file),
            .begin = diagnostic.location.begin,
            .length = diagnostic.location.length,
            .line = diagnostic.location.line,
            .column = diagnostic.location.column,
        };
        m_log(m_log_data, &diagnostic_u8);
        m_buffer.clear();
    }

    [[nodiscard]]
    Function_Ref<void(const cowel_diagnostic_u8*) noexcept> as_cowel_log_fn() override
    {
        return { m_log, m_log_data };
    }
};

struct Syntax_Highlighter_From_Options final : Syntax_Highlighter {
private:
    std::pmr::vector<std::u8string_view> m_supported_languages;
    cowel_syntax_highlighter_u8 m_highlighter;
    Syntax_Highlighter* m_fallback;

public:
    [[nodiscard]]
    explicit Syntax_Highlighter_From_Options(
        const cowel_syntax_highlighter_u8* highlighter,
        Syntax_Highlighter* fallback,
        std::pmr::memory_resource* const memory
    )
        : m_supported_languages { memory }
        , m_highlighter { *highlighter }
        , m_fallback { fallback }
    {
        COWEL_ASSERT(m_highlighter.supported_languages);
        COWEL_ASSERT(m_highlighter.highlight_by_lang_name);
        COWEL_ASSERT(m_highlighter.highlight_by_lang_index);

        const std::span<const cowel_string_view_u8> cowel_supported_languages {
            highlighter->supported_languages,
            highlighter->supported_languages_size,
        };
        const auto fallback_languages = m_fallback ? m_fallback->get_supported_languages()
                                                   : std::span<const std::u8string_view> {};
        m_supported_languages.reserve(cowel_supported_languages.size() + fallback_languages.size());
        for (const auto lang : cowel_supported_languages) {
            m_supported_languages.push_back(as_u8string_view(lang));
        }
        m_supported_languages.insert(
            m_supported_languages.end(), fallback_languages.begin(), fallback_languages.end()
        );
        std::ranges::sort(m_supported_languages);
        const auto erased_range = std::ranges::unique(m_supported_languages);
        m_supported_languages.erase(erased_range.begin(), erased_range.end());
    }

    [[nodiscard]]
    std::span<const std::u8string_view> get_supported_languages() const override
    {
        return m_supported_languages;
    }

    [[nodiscard]]
    Distant<std::u8string_view> match_supported_language(
        const std::u8string_view language,
        std::pmr::memory_resource* const memory
    ) const override
    {
        const auto [index, distance] = closest_match(m_supported_languages, language, memory);
        return { m_supported_languages[index], distance };
    }

    [[nodiscard]]
    Result<void, Syntax_Highlight_Error> operator()(
        std::pmr::vector<Highlight_Span>& out,
        const std::u8string_view code,
        const std::u8string_view language,
        std::pmr::memory_resource* const memory
    ) override
    {
        constexpr auto flush = [](std::pmr::vector<Highlight_Span>* out_ptr,
                                  cowel_syntax_highlight_token* tokens, std::size_t size) noexcept {
            // cowel_syntax_highlight_token is identical in layout to Highlight_Span,
            // so rather than copying element by element, we can just memcpy everything.
            // Unfortunately, there is no resize_and_overwrite function for std::vector,
            // so resize and overwriting happen in separate steps, and require some zeroing.
            const std::size_t initial_size = out_ptr->size();
            out_ptr->resize(initial_size + size);
            static_assert(std::is_trivially_copyable_v<cowel_syntax_highlight_token>);
            static_assert(std::is_trivially_copyable_v<Highlight_Span>);
            static_assert(sizeof(cowel_syntax_highlight_token) == sizeof(Highlight_Span));
            std::memcpy(
                out_ptr->data() + initial_size, tokens, size * sizeof(cowel_syntax_highlight_token)
            );
        };
        // We create this Function_Ref just to stash away the ugliness
        // of downcasting void* and const_casting.
        const Function_Ref<void(cowel_syntax_highlight_token*, std::size_t) noexcept> flush_ref {
            const_v<flush>, &out
        };

        cowel_syntax_highlight_token tokens[512];
        const cowel_syntax_highlight_buffer buffer {
            .data = tokens,
            .size = std::size(tokens),
            .flush = flush_ref.get_invoker(),
            .flush_data = flush_ref.get_entity(),
        };
        const cowel_syntax_highlight_status status = m_highlighter.highlight_by_lang_name(
            m_highlighter.data, &buffer, code.data(), code.size(), language.data(), language.size()
        );
        switch (status) {
        case COWEL_SYNTAX_HIGHLIGHT_OK: {
            return {};
        }
        case COWEL_SYNTAX_HIGHLIGHT_ERROR: {
            return Syntax_Highlight_Error::other;
        }
        case COWEL_SYNTAX_HIGHLIGHT_UNSUPPORTED_LANGUAGE: {
            if (!m_fallback) {
                return Syntax_Highlight_Error::unsupported_language;
            }
            return (*m_fallback)(out, code, language, memory);
        }
        case COWEL_SYNTAX_HIGHLIGHT_BAD_CODE: {
            return Syntax_Highlight_Error::bad_code;
        }
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid status.");
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

    File_Loader_From_Options file_loader { options.load_file, options.load_file_data, memory };
    Logger_From_Options logger { options, memory };

    [[maybe_unused]]
    const auto try_log
        = [&](std::u8string_view message, Severity severity) {
              if (cowel_severity(severity) < options.min_log_severity) {
                  return;
              }
              logger(
                  Diagnostic {
                      .severity = Severity::trace,
                      .id = u8"trace"sv,
                      .location = { {}, File_Id::main },
                      .message = message,
                  }
              );
          };

    try_log(u8"Trace logging enabled."sv, Severity::trace);

    static constinit Ulight_Syntax_Highlighter ulight_highlighter;
    auto from_options_highlighter = [&] -> std::optional<Syntax_Highlighter_From_Options> {
        if (!options.highlighter) {
            return {};
        }
        Syntax_Highlighter* const fallback
            = options.highlight_policy == COWEL_SYNTAX_HIGHLIGHT_POLICY_FALL_BACK
            ? &ulight_highlighter
            : nullptr;
        return Syntax_Highlighter_From_Options { options.highlighter, fallback, memory };
    }();
    auto& highlighter = [&] -> Syntax_Highlighter& {
        if (from_options_highlighter) {
            return *from_options_highlighter;
        }
        if (options.highlight_policy == COWEL_SYNTAX_HIGHLIGHT_POLICY_EXCLUSIVE) {
            return no_support_syntax_highlighter;
        }
        return ulight_highlighter;
    }();

    const auto preamble_source = as_u8string_view(options.preamble);
    const auto main_source = as_u8string_view(options.source);

    const std::convertible_to<Parse_Error_Consumer> auto on_parse_error
        = [&](std::u8string_view id, const Source_Span& pos, Char_Sequence8 message) {
              const File_Source_Span file_pos { pos, File_Id::main };
              logger(Diagnostic { Severity::error, id, file_pos, message });
          };
    ast::Pmr_Vector<ast::Markup_Element> root_content;
    const bool preamble_parse_success = lex_and_parse_and_build(
        root_content, preamble_source, File_Id::main, memory, on_parse_error //
    );
    if (!preamble_parse_success) {
        return { .status = COWEL_PROCESSING_ERROR, .output = {} };
    }

    const bool main_parse_success = lex_and_parse_and_build(
        root_content, main_source, File_Id::main, memory, on_parse_error //
    );
    if (!main_parse_success) {
        return { .status = COWEL_PROCESSING_ERROR, .output = {} };
    }

    const std::u8string_view highlight_theme_source = options.highlight_theme_json.length == 0
        ? assets::wg21_json
        : as_u8string_view(options.highlight_theme_json);

    COWEL_ASSERT(options.preserved_variables_size == 0 || options.preserved_variables != nullptr);
    std::pmr::vector<std::u8string_view> preserved_variables { memory };
    preserved_variables.reserve(options.preserved_variables_size);
    for (std::size_t i = 0; i < options.preserved_variables_size; ++i) {
        const std::u8string_view name {
            options.preserved_variables[i].text,
            options.preserved_variables[i].length,
        };
        preserved_variables.push_back(name);
    }
    const std::convertible_to<Variables_Consumer> auto consume_variables
        = [&](const std::span<const std::u8string_view> values) {
              COWEL_ASSERT(options.consume_variables);
              std::pmr::vector<cowel_string_view_u8> cowel_values { memory };
              cowel_values.reserve(options.preserved_variables_size);
              for (const auto v : values) {
                  cowel_values.push_back({ v.data(), v.length() });
              }
              options.consume_variables(
                  options.consume_variables_data, cowel_values.data(), cowel_values.size()
              );
          };

    const Generation_Options gen_options {
        .error_behavior = &builtin_behavior.get_error_behavior(),
        .preserved_variables = preserved_variables,
        .consume_variables = options.consume_variables ? consume_variables : Variables_Consumer {},
        .highlight_theme_source = highlight_theme_source,
        .builtin_name_resolver = builtin_behavior,
        .file_loader = file_loader,
        .logger = logger,
        .highlighter = highlighter,
        .memory = memory,
    };

    const Processing_Status status = run_generation(
        [&](Context& context) -> Processing_Status {
            if (options.mode == COWEL_MODE_MINIMAL) {
                return splice_all(html_policy, root_content, Frame_Index::root, context);
            }
            COWEL_ASSERT(options.mode == COWEL_MODE_DOCUMENT);
            return write_wg21_document(html_policy, root_content, context);
        },
        gen_options
    );

    if (html_sink->empty()) {
        return { .status = static_cast<cowel_processing_status>(status), .output = {} };
    }

    auto* const result_data
        = static_cast<char8_t*>(memory->allocate(html_sink->size(), alignof(char8_t)));
    const cowel_mutable_string_view_u8 result { result_data, html_sink->size() };
    if (result.text == nullptr) {
        try_log(u8"Failed to allocate memory for the generated HTML."sv, Severity::fatal);
        return { .status = COWEL_PROCESSING_FATAL, .output = result };
    }

    std::memcpy(result_data, html_sink->data(), html_sink->size());
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
}

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
