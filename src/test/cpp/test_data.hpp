#ifndef COWEL_TEST_DATA_HPP
#define COWEL_TEST_DATA_HPP

#include <span>
#include <string_view>
#include <variant>

#include "cowel/content_status.hpp"
#include "cowel/fwd.hpp"

namespace cowel {

enum struct Test_Behavior : Default_Underlying {
    trivial,
    paragraphs,
    empty_head,
};

struct Path {
    std::u8string_view value;
};

struct Source {
    std::u8string_view contents;
};

struct Basic_Test {
    std::variant<Path, Source> document;
    std::variant<Path, Source> expected_html;
    Processing_Status expected_status = Processing_Status::ok;
    std::initializer_list<std::u8string_view> expected_diagnostics = {};
    Test_Behavior behavior = Test_Behavior::trivial;
};

extern const std::span<const Basic_Test> basic_tests;

} // namespace cowel

#endif
