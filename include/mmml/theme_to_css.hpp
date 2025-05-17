#ifndef MMML_THEME_TO_CSS_HPP
#define MMML_THEME_TO_CSS_HPP

#include <memory_resource>
#include <optional>
#include <string_view>
#include <vector>

#include "ulight/ulight.hpp"

namespace mmml {

bool theme_to_css(
    std::pmr::vector<char8_t>& out,
    std::u8string_view theme_json,
    std::pmr::memory_resource* memory
);

[[nodiscard]]
std::optional<ulight::Highlight_Type> highlight_type_by_long_string(std::u8string_view str);

} // namespace mmml

#endif
