#include <cstdio>
#include <cstring>

#include "mmml/assert.hpp"
#include "mmml/fwd.hpp"
#include "mmml/io.hpp"

namespace mmml {

[[nodiscard]]
Result<void, IO_Error_Code> file_to_bytes(std::pmr::vector<char>& out, std::string_view path)
{
    constexpr std::size_t block_size = 4096;
    char buffer[block_size] {};

    MMML_ASSERT(path.size() < block_size);
    std::memcpy(buffer, path.data(), path.size());

    auto stream = std::fopen(buffer, "rb");
    if (!stream) {
        return IO_Error_Code::cannot_open;
    }

    std::size_t read_size;
    do {
        read_size = std::fread(buffer, 1, block_size, stream);
        if (std::ferror(stream)) {
            return IO_Error_Code::read_error;
        }
        out.insert(out.end(), buffer, buffer + read_size);
    } while (read_size == block_size);

    return {};
}

} // namespace mmml
