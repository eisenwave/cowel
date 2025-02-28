#ifndef MMML_IO_ERROR_HPP
#define MMML_IO_ERROR_HPP

#include "mmml/fwd.hpp"

namespace mmml {

enum struct IO_Error_Code : Default_Underlying {
    cannot_open,
    read_error,
    write_error,
};

} // namespace mmml

#endif
