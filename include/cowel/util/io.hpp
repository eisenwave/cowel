#ifndef COWEL_IO_HPP
#define COWEL_IO_HPP

#ifndef COWEL_EMSCRIPTEN
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory_resource>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "cowel/util/function_ref.hpp"
#include "cowel/util/meta.hpp"
#include "cowel/util/result.hpp"

#include "cowel/fwd.hpp"

namespace cowel {

enum struct IO_Error_Code : Default_Underlying {
    /// @brief The file couldn't be opened.
    /// This may be due to disk errors, security issues, bad file paths, or other issues.
    cannot_open,
    /// @brief An error occurred while reading a file.
    read_error,
    /// @brief An error occurred while writing a file.
    write_error,
    /// @brief The file is not properly encoded.
    /// For example, if an attempt is made to read a text file as UTF-8 that is not encoded as such.
    corrupted,
};

struct [[nodiscard]] Unique_File {
private:
    std::FILE* m_file = nullptr;

public:
    constexpr Unique_File() = default;

    constexpr Unique_File(std::FILE* f)
        : m_file { f }
    {
    }

    constexpr Unique_File(Unique_File&& other) noexcept
        : m_file { std::exchange(other.m_file, nullptr) }
    {
    }

    Unique_File(const Unique_File&) = delete;
    Unique_File& operator=(const Unique_File&) = delete;

    constexpr Unique_File& operator=(Unique_File&& other) noexcept
    {
        swap(*this, other);
        other.close();
        return *this;
    }

    constexpr friend void swap(Unique_File& x, Unique_File& y) noexcept
    {
        std::swap(x.m_file, y.m_file);
    }

    void close() noexcept
    {
        if (m_file) {
            std::fclose(m_file);
        }
    }

    [[nodiscard]]
    constexpr std::FILE* release() noexcept
    {
        return std::exchange(m_file, nullptr);
    }

    [[nodiscard]]
    constexpr std::FILE* get() const noexcept
    {
        return m_file;
    }

    [[nodiscard]]
    constexpr operator bool() const noexcept
    {
        return m_file != nullptr;
    }

    constexpr ~Unique_File()
    {
        close();
    }
};

/// @brief Forwards the arguments to `std::fopen` and wraps the result in `Unique_File`.
[[nodiscard]]
inline Unique_File fopen_unique(const char* path, const char* mode) noexcept
{
    return std::fopen(path, mode);
}

/// @brief Reads all bytes from a file and calls a given consumer with them, chunk by chunk.
/// @param consume_chunk Invoked repeatedly with temporary chunks of bytes.
/// The chunks may be located within the same underlying buffer,
/// so they should not be used after `consume_chunk` has been invoked.
/// @param path the file path
[[nodiscard]]
Result<void, IO_Error_Code> file_to_bytes_chunked(
    Function_Ref<void(std::span<const std::byte>)> consume_chunk,
    std::u8string_view path
);

/// @brief Reads all bytes from a file and appends them to a given vector.
/// @param path the file path
template <byte_like Byte, typename Alloc>
[[nodiscard]]
Result<void, IO_Error_Code> file_to_bytes(std::vector<Byte, Alloc>& out, std::u8string_view path)
{
    return file_to_bytes_chunked(
        [&out](std::span<const std::byte> chunk) -> void {
            if (chunk.empty()) {
                return;
            }
            const std::size_t old_size = out.size();
            out.resize(out.size() + chunk.size());
            std::memcpy(out.data() + old_size, chunk.data(), chunk.size());
        },
        path
    );
}

[[nodiscard]]
Result<void, IO_Error_Code> load_utf8_file(std::pmr::vector<char8_t>& out, std::u8string_view path);

[[nodiscard]]
Result<std::pmr::vector<char8_t>, IO_Error_Code>
load_utf8_file(std::u8string_view path, std::pmr::memory_resource* memory);

[[nodiscard]]
Result<std::pmr::vector<char32_t>, IO_Error_Code>
load_utf32le_file(std::u8string_view path, std::pmr::memory_resource* memory);

} // namespace cowel
#endif

#endif
