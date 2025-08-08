#ifndef COWEL_RELATIVE_FILE_LOADER_HPP
#define COWEL_RELATIVE_FILE_LOADER_HPP

#include <filesystem>
#include <memory_resource>
#include <string_view>
#include <vector>

#include "cowel/util/io.hpp"

#include "cowel/cowel.h"
#include "cowel/cowel_lib.hpp"
#include "cowel/fwd.hpp"
#include "cowel/services.hpp"

namespace cowel {

[[nodiscard]]
constexpr cowel_io_status io_error_to_io_status(IO_Error_Code error)
{
    switch (error) {
    case IO_Error_Code::read_error: return COWEL_IO_ERROR_READ;
    case IO_Error_Code::cannot_open: return COWEL_IO_ERROR_NOT_FOUND;
    default: return COWEL_IO_ERROR;
    }
}

struct Owned_File_Entry {
    std::pmr::vector<char8_t> name;
    std::pmr::vector<char8_t> text;
};

/// @brief A `File_Loader` implementation which can be used both as
/// an internal implementation of `File_Loader` for testing
/// and as an external implementation which is fed into the `cowel.h` top-level API.
///
/// This class loads files relative to a given constant base directory.
struct Relative_File_Loader final : File_Loader {
    std::filesystem::path base;
    std::pmr::vector<Owned_File_Entry> entries;

    [[nodiscard]]
    explicit Relative_File_Loader(std::filesystem::path&& base, std::pmr::memory_resource* memory)
        : base { std::move(base) }
        , entries { memory }
    {
    }

    /// @brief External implementation to be used with `cowel.h` API.
    [[nodiscard]]
    cowel_file_result_u8 do_load(std::u8string_view path)
    {
        const std::filesystem::path relative { path, std::filesystem::path::generic_format };
        const std::filesystem::path resolved = base / relative;

        std::pmr::memory_resource* const memory = entries.get_allocator().resource();
        std::pmr::vector<char8_t> path_copy { path.begin(), path.end(), memory };
        Result<std::pmr::vector<char8_t>, IO_Error_Code> result
            = load_utf8_file(resolved.generic_u8string(), memory);

        if (!result) {
            // Even though loading failed, we store the file as an entry
            // so that we can later get its name during logging.
            entries.emplace_back(std::move(path_copy));
            // Using entries.size() as the id is correct because id 0 refers to the main file,
            // whereas the first loaded file gets id 1.
            return {
                .status = io_error_to_io_status(result.error()),
                .data = {},
                .id = File_Id(entries.size()),
            };
        }

        auto& entry = entries.emplace_back(std::move(path_copy), std::move(*result));
        return cowel_file_result_u8 {
            .status = COWEL_IO_OK,
            .data = { entry.text.data(), entry.text.size() },
            .id = File_Id(entries.size()),
        };
    }

    /// @brief Internal implementation for `File_Loader` service.
    [[nodiscard]]
    Result<File_Entry, File_Load_Error> load(std::u8string_view path) final
    {
        const cowel_file_result_u8 result = do_load(path);
        if (result.status != COWEL_IO_OK) {
            return io_status_to_load_error(result.status);
        }
        return File_Entry { .id = result.id,
                            .source = { result.data.text, result.data.length },
                            .name = path };
    }
};

} // namespace cowel

#endif
