#include <filesystem>
#include <memory_resource>
#include <string_view>
#include <vector>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/cowel_lib.hpp"
#include "cowel/relative_file_loader.hpp"
#include "cowel/services.hpp"

#ifdef COWEL_EMSCRIPTEN
#error "This file not be included in emscripten builds."
#endif

namespace cowel {
namespace {

[[nodiscard]]
constexpr cowel_io_status io_error_to_io_status(IO_Error_Code error)
{
    switch (error) {
    case IO_Error_Code::read_error: return COWEL_IO_ERROR_READ;
    case IO_Error_Code::cannot_open: return COWEL_IO_ERROR_NOT_FOUND;
    default: return COWEL_IO_ERROR;
    }
}

} // namespace

Relative_File_Loader::Relative_File_Loader(
    std::filesystem::path&& base,
    std::pmr::memory_resource* memory
)
    : m_base { std::move(base) }
    , m_entries { memory }
{
}

auto Relative_File_Loader::do_load(Char_Sequence8 path_chars, File_Id relative_to)
    -> Complete_Result
{
    std::pmr::memory_resource* const memory = m_entries.get_allocator().resource();
    std::pmr::vector<char8_t> path_copy { memory };
    const std::u8string_view path_chars_string = path_chars.as_string_view();
    if (!path_chars_string.empty()) {
        path_copy.insert(path_copy.end(), path_chars_string.begin(), path_chars_string.end());
    }
    else {
        path_copy.resize(path_chars.size());
        path_chars.extract(std::span { path_copy });
    }
    const auto path_string = as_u8string_view(path_copy);

    const std::filesystem::path relative { path_string, std::filesystem::path::generic_format };
    auto resolved = [&] -> std::filesystem::path {
        if (relative_to == File_Id::main) {
            return m_base / relative;
        }
        const auto parent = at(relative_to).path.parent_path();
        return parent / relative;
    }();
    std::u8string resolved_string = resolved.generic_u8string();

    Result<std::pmr::vector<char8_t>, IO_Error_Code> result
        = load_utf8_file(resolved.generic_u8string(), memory);

    auto& entry = m_entries.emplace_back(Owned_File_Entry {
        .path = std::move(resolved),
        .path_string = std::move(resolved_string),
        .text = result ? std::move(*result) : std::pmr::vector<char8_t> {},
    });

    const auto result_status = result ? COWEL_IO_OK : io_error_to_io_status(result.error());
    const auto result_data = result
        ? cowel_mutable_string_view_u8 { entry.text.data(), entry.text.size() }
        : cowel_mutable_string_view_u8 {};

    return Complete_Result {
        .file_result {
            .status = result_status,
            .data = result_data,
            .id = cowel_file_id(m_entries.size() - 1),
        },
        .entry = entry,
    };
}

Result<File_Entry, File_Load_Error>
Relative_File_Loader::load(Char_Sequence8 path, File_Id relative_to)
{
    const Complete_Result result = do_load(path, relative_to);

    if (result.file_result.status != COWEL_IO_OK) {
        return io_status_to_load_error(result.file_result.status);
    }
    return File_Entry {
        .id = File_Id(result.file_result.id),
        .source = { result.file_result.data.text, result.file_result.data.length },
        .name = result.entry.path_string,
    };
}

} // namespace cowel
