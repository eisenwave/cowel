#ifndef MMML_TEST_DOCUMENT_FILE_TESTING_HPP
#define MMML_TEST_DOCUMENT_FILE_TESTING_HPP

#include <string_view>

#include "mmml/fwd.hpp"

#include "compilation_stage.hpp"

namespace mmml {

[[nodiscard]]
bool test_for_success(
    std::string_view file,
    Compilation_Stage until_stage = Compilation_Stage::process
);

} // namespace mmml

#endif
