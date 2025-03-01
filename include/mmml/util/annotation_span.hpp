#ifndef MMML_CODE_STRING_SPAN_HPP
#define MMML_CODE_STRING_SPAN_HPP

#include <cstddef>

#include "mmml/util/annotation_type.hpp"

namespace mmml {

struct Annotation_Span {
    std::size_t begin;
    std::size_t length;
    Annotation_Type type;

    [[nodiscard]]
    constexpr std::size_t end() const
    {
        return begin + length;
    }
};

} // namespace mmml

#endif
