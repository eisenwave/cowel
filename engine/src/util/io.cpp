#include <bit>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory_resource>
#include <span>
#include <string_view>
#include <vector>

#include "cowel/util/function_ref.hpp"
#include "cowel/util/io.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/unicode.hpp"

#include "cowel/fwd.hpp"

namespace cowel {

[[nodiscard]]
Result<void, IO_Error_Code> file_to_bytes_chunked(
    Function_Ref<void(std::span<const std::byte>)> consume_chunk,
    std::u8string_view path
)
{
    constexpr std::size_t block_size = BUFSIZ;
    char buffer[block_size] {};

    if (path.size() > block_size) {
        return IO_Error_Code::cannot_open;
    }
    std::memcpy(buffer, path.data(), path.size());

    const Unique_File stream = fopen_unique(buffer, "rb");
    if (!stream) {
        return IO_Error_Code::cannot_open;
    }

    std::size_t read_size;
    do {
        read_size = std::fread(buffer, 1, block_size, stream.get());
        if (std::ferror(stream.get())) {
            return IO_Error_Code::read_error;
        }
        const std::span<std::byte> chunk { reinterpret_cast<std::byte*>(buffer), read_size };
        consume_chunk(chunk);
    } while (read_size == block_size);

    return {};
}

Result<void, IO_Error_Code> load_utf8_file(std::pmr::vector<char8_t>& out, std::u8string_view path)
{
    const std::size_t initial_size = out.size();
    Result<void, IO_Error_Code> r = file_to_bytes(out, path);
    if (!r) {
        return r;
    }
    const std::u8string_view str { out.data() + initial_size, out.size() - initial_size };
    if (!utf8::is_valid(str)) {
        return IO_Error_Code::corrupted;
    }
    return {};
}

Result<std::pmr::vector<char8_t>, IO_Error_Code>
load_utf8_file(std::u8string_view path, std::pmr::memory_resource* memory)
{
    std::pmr::vector<char8_t> result { memory };
    if (auto r = load_utf8_file(result, path); !r) {
        return r.error();
    }
    return result;
}

[[nodiscard]]
Result<std::pmr::vector<char32_t>, IO_Error_Code>
load_utf32le_file(std::u8string_view path, std::pmr::memory_resource* memory)
{
    std::pmr::vector<std::byte> bytes { memory };

    Result<void, IO_Error_Code> r = file_to_bytes(bytes, path);
    if (!r) {
        return r.error();
    }
    if (bytes.size() % sizeof(char32_t) != 0) {
        return IO_Error_Code::corrupted;
    }

    // We could make this code more portable in the long run, but I don't care for now.
    static_assert(std::endian::native == std::endian::little);
    std::pmr::vector<char32_t> result { bytes.size() / 4, memory };
    std::memcpy(result.data(), bytes.data(), bytes.size());

    return result;
}

void find_files_recursively(
    std::pmr::vector<fs::path>& out,
    const fs::path& directory,
    const Function_Ref<bool(const fs::directory_entry&)> filter
)
{
    COWEL_ASSERT(fs::is_directory(directory));

    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(directory)) {
        if (!filter || filter(entry)) {
            out.push_back(entry.path());
        }
    }
}

Result<void, IO_Error_Code>
bytes_to_file(const void* data, std::size_t amount, std::u8string_view path)
{
    const std::string c_path(reinterpret_cast<const char*>(path.data()), path.size());
    Unique_File file = fopen_unique(c_path.c_str(), "wb");
    if (!file) {
        return IO_Error_Code::cannot_open;
    }
    const std::size_t bytes_written = std::fwrite(data, 1, amount, file.get());
    if (bytes_written != amount) {
        return IO_Error_Code::write_error;
    }
    std::fflush(file.get());
    return {};
}

} // namespace cowel
