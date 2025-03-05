#ifndef MMML_EMSCRIPTEN

#include <cstdio>
#include <cstring>

#include "mmml/util/assert.hpp"
#include "mmml/util/io.hpp"

#include "mmml/fwd.hpp"

namespace mmml {

[[nodiscard]]
Result<void, IO_Error_Code> file_to_bytes_chunked(
    Function_Ref<void(std::span<const std::byte>)> consume_chunk,
    std::string_view path
)
{
    constexpr std::size_t block_size = BUFSIZ;
    char buffer[block_size] {};

    if (path.size() > block_size) {
        return IO_Error_Code::cannot_open;
    }
    std::memcpy(buffer, path.data(), path.size());

    Unique_File stream = fopen_unique(buffer, "rb");
    if (!stream) {
        return IO_Error_Code::cannot_open;
    }

    std::size_t read_size;
    do {
        read_size = std::fread(buffer, 1, block_size, stream.get());
        if (std::ferror(stream.get())) {
            return IO_Error_Code::read_error;
        }
        std::span<std::byte> chunk { reinterpret_cast<std::byte*>(buffer), read_size };
        consume_chunk(chunk);
    } while (read_size == block_size);

    return {};
}

} // namespace mmml
#endif
