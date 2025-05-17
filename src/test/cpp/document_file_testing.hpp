#ifndef COWEL_TEST_DOCUMENT_FILE_TESTING_HPP
#define COWEL_TEST_DOCUMENT_FILE_TESTING_HPP

#include <string_view>

#include "compilation_stage.hpp"

namespace cowel {

[[nodiscard]]
bool test_for_success(
    std::string_view file,
    Compilation_Stage until_stage = Compilation_Stage::process
);

} // namespace cowel

#endif
