#ifndef COWEL_CODE_SPAN_TYPE_HPP
#define COWEL_CODE_SPAN_TYPE_HPP

#include "cowel/fwd.hpp"

namespace cowel {

enum struct Diagnostic_Highlight : Default_Underlying {
    text,
    error_text,
    code_position,
    error,
    warning,
    note,
    line_number,
    punctuation,
    position_indicator,
    code_citation,
    internal_error_notice,
    operand,
    op,
    tag,
    attribute,
    internal,
    escape,
    success,
    diff_common,
    diff_del,
    diff_ins,
};

} // namespace cowel

#endif
