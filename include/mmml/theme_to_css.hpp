#ifndef MMML_THEME_TO_CSS_HPP
#define MMML_THEME_TO_CSS_HPP

#include <memory_resource>
#include <string_view>
#include <vector>

namespace mmml {

bool theme_to_css(
    std::pmr::vector<char8_t>& out,
    std::u8string_view theme_json,
    std::pmr::memory_resource* memory
);

}

#endif
