#ifndef COWEL_EXPRESSION_KIND
#define COWEL_EXPRESSION_KIND

#include "cowel/fwd.hpp"

namespace cowel {

enum struct Comparison_Expression_Kind : Default_Underlying {
    eq,
    ne,
    lt,
    gt,
    le,
    ge,
};

enum struct Unary_Expression_Kind : Default_Underlying {
    bitwise_not,
    logical_not,
    plus,
    minus,
};

}; // namespace cowel

#endif
