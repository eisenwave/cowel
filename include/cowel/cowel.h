#ifndef COWEL_H
#define COWEL_H

#ifdef __EMSCRIPTEN__
#define COWEL_EMSCRIPTEN 1
#define COWEL_IF_EMSCRIPTEN(...) __VA_ARGS__
#else
#define COWEL_IF_EMSCRIPTEN(...)
#endif

#ifdef __GNUC__
#define COWEL_EXPORT [[gnu::used]]
#else
#define COWEL_EXPORT
#endif

#ifdef __cplusplus
#define COWEL_NOEXCEPT noexcept
#define COWEL_DEPRECATED [[deprecated]]
#define COWEL_NODISCARD [[nodiscard]]
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define COWEL_NOEXCEPT
#define COWEL_DEPRECATED [[deprecated]]
#define COWEL_NODISCARD [[nodiscard]]
#else
#define COWEL_NOEXCEPT
#define COWEL_DEPRECATED
#define COWEL_NODISCARD
#endif

#include <stddef.h> // NOLINT(modernize-deprecated-headers)

#ifdef __cplusplus
extern "C" {
#endif

// NOLINTNEXTLINE(performance-enum-size)
enum cowel_mode {
    /// @brief Document generation with the usual `<head>` body etc. orchestration.
    COWEL_MODE_DOCUMENT,
    /// @brief Minimalistic generation.
    /// Content is directly written into the resulting HTML,
    /// without any pre-defined styles, no `<head>, `<body>`, etc.
    COWEL_MODE_MINIMAL,
};

// NOLINTNEXTLINE(performance-enum-size)
enum cowel_io_status {
    /// @brief The operation succeeded.
    COWEL_IO_OK,
    /// @brief The operation failed with a generic I/O error.
    COWEL_IO_ERROR,
    /// @brief The operation failed because a file could not be found.
    COWEL_IO_ERROR_NOT_FOUND,
    /// @brief
    COWEL_IO_ERROR_READ,
    COWEL_IO_ERROR_PERMISSIONS,
};

// NOLINTNEXTLINE(performance-enum-size)
enum cowel_severity {
    COWEL_SEVERITY_MIN = 0,
    COWEL_SEVERITY_TRACE = 10,
    COWEL_SEVERITY_DEBUG = 20,
    COWEL_SEVERITY_INFO = 30,
    COWEL_SEVERITY_SOFT_WARNING = 40,
    COWEL_SEVERITY_WARNING = 50,
    COWEL_SEVERITY_ERROR = 70,
    COWEL_SEVERITY_FATAL = 90,
    COWEL_SEVERITY_MAX = 90,
    COWEL_SEVERITY_NONE = 100,
};

// NOLINTNEXTLINE(performance-enum-size)
enum cowel_processing_status {
    /// @brief Content could be produced successfully,
    /// and generation should continue.
    COWEL_PROCESSING_OK,
    /// @brief Content generation was aborted (due to a break/return-like construct).
    /// However, this is not an error.
    COWEL_PROCESSING_BREAK,
    /// @brief An error occurred,
    /// but that error is recoverable.
    COWEL_PROCESSING_ERROR,
    /// @brief An error occurred,
    /// but processing continued until `COWEL_GEN_BREAK` was produced.
    /// This is effectively a combination of `COWEL_GEN_ERROR` and `COWEL_GEN_BREAK`.
    COWEL_PROCESSING_ERROR_BREAK,
    /// @brief An unrecoverable error occurred,
    /// and generation of the document as a whole has to be abandoned.
    COWEL_PROCESSING_FATAL,
};

// NOLINTNEXTLINE(performance-enum-size)
enum cowel_assertion_type {
    COWEL_ASSERTION_NOT_TRUE,
    COWEL_ASSERTION_UNREACHABLE,
};

/// @brief A container for a string and a length.
/// The string does not have to be null-terminated.
struct cowel_string_view {
    /// @brief Pointer to text UTF-8 text data.
    const char* text;
    /// @brief Length of text data in bytes/UTF-8 code units.
    size_t length;
};

/// @brief See `cowel_string_view`.
struct cowel_string_view_u8 {
    const char8_t* text;
    size_t length;
};

/// @brief Like `cowel_string_view`, but storing a pointer to mutable text instead.
struct cowel_mutable_string_view {
    char* text;
    size_t length;
};

/// @brief See `cowel_mutable_string_view`.
struct cowel_mutable_string_view_u8 {
    char8_t* text;
    size_t length;
};

typedef int cowel_file_id; // NOLINT(modernize-use-using)

/// @brief Data associated with a diagnostic message.
struct cowel_diagnostic {
    /// @brief The level of severity for this diagnostic.
    cowel_severity severity;
    /// @brief A unique identifier for the diagnostic.
    cowel_string_view id;
    /// @brief The diagnostic message.
    cowel_string_view message;
    /// @brief The name of the file in which the diagnostic was raised.
    /// This is often an empty string, since the `file_id` carries information about the
    /// file already, and the user is expected to keep track of which files have which name.
    /// However, the file name is sometimes overriden by this data member.
    cowel_string_view file_name;
    /// @brief The id of the file in which the diagnostic occurred.
    cowel_file_id file_id;
    /// @brief The code unit within the file where the diagnostic occurred.
    size_t begin;
    /// @brief The (possibly zero, in past-the-file cases) amount of code units
    /// starting at `begin` where the diagnostic occurred.
    /// If both `begin` and `length` are zero,
    /// the diagnostic is considered to address the entire file.
    size_t length;
    /// @brief The line index (starting at zero) where the diagnostic occurred.
    size_t line;
    /// @brief The offset from the start of the line in code units.
    size_t column;
};

/// @brief See `cowel_diagnostic`.
struct cowel_diagnostic_u8 {
    cowel_severity severity;
    cowel_string_view_u8 id;
    cowel_string_view_u8 message;
    cowel_string_view_u8 file_name;
    cowel_file_id file_id;
    size_t begin;
    size_t length;
    size_t line;
    size_t column;
};

/// @brief A type which contains result information when a file was loaded.
struct cowel_file_result {
    /// @brief The status of loading the file, indicating success or failure.
    cowel_io_status status;
    /// @brief The pointer to the loaded file data,
    /// allocated using `cowel_options::alloc`.
    /// If loading failed, this is null.
    cowel_mutable_string_view data;
    /// @brief A unique identifier for the loaded file.
    /// The identifier zero refers to the main file,
    /// which is not actually loaded, but whose source is provided within `cowel_options`.
    cowel_file_id id;
};

/// @brief See `cowel_file_result`.
struct cowel_file_result_u8 {
    cowel_io_status status;
    cowel_mutable_string_view_u8 data;
    cowel_file_id id;
};

struct cowel_assertion_error_u8 {
    cowel_assertion_type type;
    cowel_string_view_u8 message;
    cowel_string_view_u8 file_name;
    cowel_string_view_u8 function_name;
    size_t line;
    size_t column;
};

// NOLINTNEXTLINE(modernize-use-using)
typedef void* cowel_alloc_fn(const void* data, size_t size, size_t alignment) COWEL_NOEXCEPT;

// NOLINTNEXTLINE(modernize-use-using)
typedef void
cowel_free_fn(const void* data, void* pointer, size_t size, size_t alignment) COWEL_NOEXCEPT;

// NOLINTNEXTLINE(modernize-use-using)
typedef cowel_file_result
cowel_load_file_fn(const void* data, cowel_string_view path) COWEL_NOEXCEPT;
// NOLINTNEXTLINE(modernize-use-using)
typedef cowel_file_result_u8
cowel_load_file_fn_u8(const void* data, cowel_string_view_u8 path) COWEL_NOEXCEPT;

// NOLINTNEXTLINE(modernize-use-using)
typedef void cowel_log_fn(const void* data, const cowel_diagnostic* diagnostic) COWEL_NOEXCEPT;
// NOLINTNEXTLINE(modernize-use-using)
typedef void
cowel_log_fn_u8(const void* data, const cowel_diagnostic_u8* diagnostic) COWEL_NOEXCEPT;

// NOLINTNEXTLINE(modernize-use-using)
typedef void cowel_assertion_handler_fn_u8(const cowel_assertion_error_u8* error);

struct cowel_options {
    /// @brief The UTF-8-encoded cowel source code.
    cowel_string_view source;
    /// @brief The UTF-8-encoded (JSON) source for the highlight theme.
    /// If this string is empty, the builtin theme is used.
    cowel_string_view highlight_theme_source;
    /// @brief The processing mode.
    cowel_mode mode;
    /// @brief The minimum (inclusive) level that log messages must have to be logged.
    cowel_severity min_log_severity;
    /// @brief Reserved space.
    void* reserved_0[4];

    /// @brief A (possibly null) pointer to a function which performs memory allocation.
    /// If `alloc` is null, `cowel_alloc` is used instead,
    /// i.e. global allocation takes place.
    /// Therefore, make sure to provide both or neither `alloc` and `free`.
    cowel_alloc_fn* alloc;
    /// @brief Additional data passed into `alloc`.
    const void* alloc_data;

    /// @brief A (possibly null) pointer to a function which frees the memory obtained from `free`.
    /// The provided function has to be callable with a null pointer as the `pointer` argument,
    /// in which case it has no effect.
    /// If `free` is null, `cowel_free` is used instead,
    /// i.e. global deallocation takes place.
    /// Therefore, make sure to provide both or neither `alloc` and `free`.
    cowel_free_fn* free;
    /// @brief Additional data passed into `free`.
    const void* free_data;

    /// @brief A (possibly null) pointer to a function which loads files.
    /// `load_file` is invoked with file paths relative to the provided document,
    /// in the portable format (`/` is used as the path separator).
    /// For example, if the main document contains a `\cowel_include{d/a.cow}` directive,
    /// `load_file` is invoked `with `"d/a.cow"`.
    /// If that loaded document contains `\cowel_include{b.cow}`,
    /// `load_file` is invoked with `d/b.cow`.
    /// If `load_file` is null, the effect is the same as providing a function
    /// which always fails loading a file, with status `COWEL_IO_ERROR`.
    cowel_load_file_fn* load_file;
    /// @brief Additional data passed into `load_file`.
    const void* load_file_data;

    /// @brief A (possibly null) pointer to a function which emits diagnostics.
    /// This will be invoked when warnings, errors, and other messages are emitted
    /// while the document is processed.
    /// `log` is never invoked with a diagnostic whose severity is lower than
    /// the provided `min_log_severity`.
    /// If `log` is null, all diagnostics are discarded.
    cowel_log_fn* log;
    /// @brief Additional data passed into `log`.
    const void* log_data;

    /// @brief Reserved space.
    void* reserved_1[4];
};

/// @brief See `cowel_options`.
struct cowel_options_u8 {
    cowel_string_view_u8 source;
    cowel_string_view_u8 highlight_theme_json;
    cowel_mode mode;
    cowel_severity min_log_severity;
    void* reserved_0[4];

    cowel_alloc_fn* alloc;
    const void* alloc_data;
    cowel_free_fn* free;
    const void* free_data;
    cowel_load_file_fn_u8* load_file;
    const void* load_file_data;
    cowel_log_fn_u8* log;
    const void* log_data;
    void* reserved_1[4];
};

static_assert(sizeof(cowel_string_view) == sizeof(cowel_string_view_u8));
static_assert(sizeof(cowel_mutable_string_view) == sizeof(cowel_mutable_string_view_u8));
static_assert(sizeof(cowel_diagnostic) == sizeof(cowel_diagnostic_u8));
static_assert(sizeof(cowel_options) == sizeof(cowel_options_u8));

struct cowel_gen_result {
    cowel_processing_status status;
    cowel_mutable_string_view output;
};

struct cowel_gen_result_u8 {
    cowel_processing_status status;
    cowel_mutable_string_view_u8 output;
};

// clang-format off

/// @brief The default function for performing allocation.
/// @param size The amount of bytes to allocate.
/// @param alignment The alignment value for the allocation.
/// Must be a power of two.
/// @returns A pointer to the allocated data,
/// or a null pointer if allocation fails.
COWEL_EXPORT COWEL_NODISCARD
void* cowel_alloc(size_t size, size_t alignment) COWEL_NOEXCEPT;

/// @brief Frees allocations previously allocated by `cowel_alloc`.
/// If `pointer` is null, does nothing.
/// @param pointer The (possibly null) pointer to data previously allocated by `cowel_alloc`.
COWEL_EXPORT
void cowel_free(void* pointer, size_t size, size_t alignment) COWEL_NOEXCEPT;

COWEL_EXPORT COWEL_NODISCARD
cowel_mutable_string_view cowel_alloc_text(cowel_string_view text) COWEL_NOEXCEPT;

COWEL_EXPORT COWEL_NODISCARD
cowel_mutable_string_view_u8 cowel_alloc_text_u8(cowel_string_view_u8 text) COWEL_NOEXCEPT;

/// @brief Runs document generation using the specified options.
/// The result is a string containing the generated HTML,
/// allocated using `options.alloc`,
/// or using `cowel_alloc` if `options.alloc` is null.
COWEL_EXPORT COWEL_NODISCARD
cowel_gen_result cowel_generate_html(const cowel_options* options) COWEL_NOEXCEPT;

/// @brief See `cowel_run`.
COWEL_EXPORT COWEL_NODISCARD
cowel_gen_result_u8 cowel_generate_html_u8(const cowel_options_u8* options) COWEL_NOEXCEPT;

COWEL_EXPORT
void cowel_set_assertion_handler_u8(cowel_assertion_handler_fn_u8* handler) COWEL_NOEXCEPT;

// clang-format on

#ifdef __cplusplus
}
#endif

#endif
