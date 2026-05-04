#ifdef __unix__
#include "stdio.h" // NOLINT for fileno
#include <cstdio>
#include <unistd.h>
#endif

#include "cowel/util/tty.hpp"

namespace cowel {

bool is_tty(std::FILE* file) noexcept
{
#ifdef __unix__
    return isatty(fileno(file));
#else
    return false;
#endif
}

const bool is_stdin_tty = is_tty(stdin);
const bool is_stdout_tty = is_tty(stdout);
const bool is_stderr_tty = is_tty(stderr);

} // namespace cowel
