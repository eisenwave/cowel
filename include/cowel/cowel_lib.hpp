#ifndef COWEL_LIB_HPP
#define COWEL_LIB_HPP

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

} // namespace cowel

#endif
