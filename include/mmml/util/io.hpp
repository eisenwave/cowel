#ifndef MMML_IO_HPP
#define MMML_IO_HPP

#include <memory_resource>
#include <string_view>
#include <vector>

#include "mmml/util/io_error.hpp"
#include "mmml/util/result.hpp"

namespace mmml {

/// @brief Reads all bytes from a file and appends them to a given vector.
[[nodiscard]]
Result<void, IO_Error_Code> file_to_bytes(std::pmr::vector<char>& out, std::string_view path);

} // namespace mmml

#endif
