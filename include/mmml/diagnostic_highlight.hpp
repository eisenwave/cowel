#ifndef MMML_CODE_SPAN_TYPE_HPP
#define MMML_CODE_SPAN_TYPE_HPP

#include "mmml/fwd.hpp"

namespace mmml {

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

} // namespace mmml

#endif
