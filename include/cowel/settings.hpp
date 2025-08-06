#ifndef COWEL_SETTINGS_HPP
#define COWEL_SETTINGS_HPP

#include <cstddef>

#include "ulight/impl/platform.h"

#define COWEL_HOT ULIGHT_HOT
#define COWEL_COLD ULIGHT_COLD

#ifdef ULIGHT_EXCEPTIONS
#define COWEL_EXCEPTIONS ULIGHT_EXCEPTIONS
#endif

namespace cowel {

/// @brief If `true`, adds assertions in various places
/// which check for writing of empty strings to content policies and other places.
/// The point is to identify potential optimization opportunities/correctness problems,
/// where empty strings ultimately have no effect anyway.
inline constexpr bool enable_empty_string_assertions = false;

/// @brief The default `char8_t` buffer size
/// when it is necessary to process a `Char_Sequence` in a chunked/buffered way.
inline constexpr std::size_t default_char_sequence_buffer_size = 1024;

/// @brief The buffer size for buffered `HTML_Writer`s.
inline constexpr std::size_t html_writer_buffer_size = 512;

} // namespace cowel

#endif
