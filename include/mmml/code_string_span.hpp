#ifndef MMML_CODE_STRING_SPAN_HPP
#define MMML_CODE_STRING_SPAN_HPP

#include <cstddef>

#include "mmml/code_span_type.hpp"

namespace mmml {

struct Code_String_Span {
    std::size_t begin;
    std::size_t length;
    Code_Span_Type type;

    [[nodiscard]]
    constexpr std::size_t end() const
    {
        return begin + length;
    }
};

} // namespace mmml

#endif
