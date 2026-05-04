#ifndef COWEL_TTY_HPP
#define COWEL_TTY_HPP

#include "cowel/fwd.hpp"

#ifdef COWEL_EMSCRIPTEN
#error "This file should not be included on emscripten builds."
#endif

#include <cstdio>

namespace cowel {

// https://pubs.opengroup.org/onlinepubs/009695399/functions/isatty.html
[[nodiscard]]
bool is_tty(std::FILE*) noexcept;

// These convenience constants aren't really necessary.
// Their purpose is to reduce the number of individual calls to `is_tty`,
// so that effectively, globally, just one call per file is made.

/// @brief True if `is_tty(stdin)` is `true`.
extern const bool is_stdin_tty;
/// @brief True if `is_tty(stdout)` is `true`.
extern const bool is_stdout_tty;
/// @brief True if `is_tty(stderr)` is `true`.
extern const bool is_stderr_tty;

} // namespace cowel
#endif
