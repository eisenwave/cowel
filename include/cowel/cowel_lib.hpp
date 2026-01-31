#ifndef COWEL_LIB_HPP
#define COWEL_LIB_HPP

#include <memory_resource>

#include "cowel/util/assert.hpp"

#include "cowel/cowel.h"
#include "cowel/services.hpp"

namespace cowel {

[[nodiscard]]
constexpr File_Load_Error io_status_to_load_error(cowel_io_status error)
{
    COWEL_ASSERT(error != COWEL_IO_OK);
    switch (error) {
    case COWEL_IO_ERROR_READ: return File_Load_Error::read_error;
    case COWEL_IO_ERROR_NOT_FOUND: return File_Load_Error::not_found;
    case COWEL_IO_ERROR_PERMISSIONS: return File_Load_Error::permissions;
    default: return File_Load_Error::error;
    }
}

[[nodiscard]]
constexpr std::u8string_view as_u8string_view(cowel_string_view_u8 str)
{
    return { str.text, str.length };
}

[[nodiscard]]
constexpr std::u8string_view as_u8string_view(cowel_mutable_string_view_u8 str)
{
    return { str.text, str.length };
}

[[nodiscard]]
constexpr cowel_string_view as_cowel_string_view(std::string_view str)
{
    return { str.data(), str.length() };
}

[[nodiscard]]
constexpr cowel_string_view_u8 as_cowel_string_view(std::u8string_view str)
{
    return { str.data(), str.length() };
}

struct Allocator_Options {
    [[nodiscard]]
    static Allocator_Options from_memory_resource(std::pmr::memory_resource*) noexcept;

    cowel_alloc_fn* alloc;
    const void* alloc_data;

    cowel_free_fn* free;
    const void* free_data;
};

} // namespace cowel

#endif
