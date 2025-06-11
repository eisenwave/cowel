#ifndef COWEL_TEST_IO_HPP
#define COWEL_TEST_IO_HPP

#include "cowel/fwd.hpp"

#ifdef COWEL_EMSCRIPTEN
#error "This file should not be included when using emscripten."
#endif

#include <memory_resource>
#include <string_view>
#include <vector>

#include "cowel/util/annotated_string.hpp"
#include "cowel/util/io.hpp"
#include "cowel/util/result.hpp"

#include "cowel/print.hpp"

namespace cowel {

[[nodiscard]]
inline bool load_utf8_file_or_error(
    std::pmr::vector<char8_t>& out,
    std::u8string_view path,
    std::pmr::memory_resource* memory
)
{
    const Result<void, IO_Error_Code> result = load_utf8_file(out, path);
    if (result) {
        return true;
    }

    Basic_Annotated_String<char8_t, Diagnostic_Highlight> error { memory };
    print_io_error(error, path, result.error());
    print_code_string_stdout(error);
    return false;
}

} // namespace cowel

#endif
