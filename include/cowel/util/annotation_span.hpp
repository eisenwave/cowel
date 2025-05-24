#ifndef COWEL_ANNOTATION_SPAN_HPP
#define COWEL_ANNOTATION_SPAN_HPP

#include <cstddef>

#include "cowel/fwd.hpp"

namespace cowel {

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

} // namespace cowel

#endif
