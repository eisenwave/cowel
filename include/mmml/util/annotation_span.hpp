#ifndef MMML_CODE_STRING_SPAN_HPP
#define MMML_CODE_STRING_SPAN_HPP

#include <cstddef>

#include "mmml/fwd.hpp"

namespace mmml {

template <typename T>
struct Annotation_Span {
    std::size_t begin;
    std::size_t length;
    T value;

    [[nodiscard]]
    constexpr std::size_t end() const
    {
        return begin + length;
    }
};

} // namespace mmml

#endif
