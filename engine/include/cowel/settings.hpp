#ifndef COWEL_SETTINGS_HPP
#define COWEL_SETTINGS_HPP

#include <cstddef>

#include "ulight/impl/platform.h"

#ifndef NDEBUG // debug builds
#define COWEL_DEBUG 1
#define COWEL_IF_DEBUG(...) __VA_ARGS__
#define COWEL_IF_NOT_DEBUG(...)
#else // release builds
#define COWEL_IF_DEBUG(...)
#define COWEL_IF_NOT_DEBUG(...) __VA_ARGS__
#endif

#ifdef ULIGHT_EMSCRIPTEN
#define COWEL_EMSCRIPTEN 1
#endif

#define COWEL_IF_EMSCRIPTEN(...) ULIGHT_IF_EMSCRIPTEN(...)

#ifdef COWEL_EMSCRIPTEN
#define COWEL_WASM_IMPORT(module, name) __attribute__((import_module(module), import_name(name)))
#else
#define COWEL_WASM_IMPORT(module, name)
#endif

#ifdef ULIGHT_CLANG
#define COWEL_CLANG 1
#if __clang_major__ < 20
#error "COWEL requires at least Clang 20 to compile"
#endif
#endif

#ifdef ULIGHT_GCC
#define COWEL_GCC 1
#endif

#define COWEL_UNREACHABLE() __builtin_unreachable()

#define COWEL_HOT ULIGHT_HOT
#define COWEL_COLD ULIGHT_COLD

#ifdef ULIGHT_EXCEPTIONS
#define COWEL_EXCEPTIONS ULIGHT_EXCEPTIONS
#endif

#ifdef _LIBCPP_VERSION
#define COWEL_LIBCXX 1
#endif

#ifdef __GLIBCXX__
#define COWEL_LIBSTDCXX 1
#endif

#if !defined(COWEL_BUILD_WASM) && !defined(COWEL_BUILD_NATIVE)
#error                                                                                             \
    "Unable to determine build type because neither COWEL_BUILD_WASM nor COWEL_BUILD_NATIVE is defined."
#endif

#define COWEL_VERSION_MAJOR 0
#define COWEL_VERSION_MINOR 7

namespace cowel {

using Int32 = int;
using Uint32 = unsigned;
using Int64 = long long;
using Uint64 = unsigned long long;

#if defined(COWEL_CLANG) || defined(COWEL_GCC)
__extension__ typedef signed __int128 Int128; // NOLINT modernize-use-using
__extension__ typedef unsigned __int128 Uint128; // NOLINT modernize-use-using
#else
#error "COWEL currently only supports Clang or GCC."
#endif

/// @brief A type with sufficient size (but possibly not alignment)
/// to provide storage for `Integer`.
/// This is mainly useful to prevent `Int128` from causing 16-byte alignment
/// for the entire `Value`, which leads to excessive internal padding.
struct Underaligned_Int128_Storage {
    alignas(unsigned long long) unsigned char bytes[sizeof(Int128)];
};

static_assert(sizeof(Underaligned_Int128_Storage) == sizeof(Int128));

/// @brief If `true`, the current build is a debug build (not a release build).
inline constexpr bool is_debug_build = COWEL_IF_DEBUG(true) COWEL_IF_NOT_DEBUG(false);

/// @brief If `true`, adds assertions in various places
/// which check for writing of empty strings to content policies and other places.
/// The point is to identify potential optimization opportunities/correctness problems,
/// where empty strings ultimately have no effect anyway.
inline constexpr bool enable_empty_string_assertions = is_debug_build;

/// @brief The default `char8_t` buffer size
/// when it is necessary to process a `Char_Sequence` in a chunked/buffered way.
inline constexpr std::size_t default_char_sequence_buffer_size = 1024;

/// @brief The buffer size for buffered `HTML_Writer`s.
inline constexpr std::size_t html_writer_buffer_size = 512;

// clang-format off
enum struct Standard_Library : unsigned char {
    /// @brief Unknown standard library.
    unknown = 0,
    /// @brief LLVM libc++.
    libcxx = 1,
    /// @brief GNU libstdc++.
    libstdcxx = 2,
    /// @brief The current standard library.
    current =
#ifdef COWEL_LIBCXX
    libcxx
#elifdef COWEL_LIBSTDCXX
    libstdcxx
#else
    unknown
#endif
};
// clang-format on

static_assert(
    Standard_Library::current != Standard_Library::unknown,
    "Currently, only compiling with libstdc++ or libc++ is supported."
);

} // namespace cowel

#endif
