#ifndef MMML_CODE_SPAN_TYPE_HPP
#define MMML_CODE_SPAN_TYPE_HPP

#include <string_view>

#include "mmml/assert.hpp"
#include "mmml/fwd.hpp"

namespace mmml {

/// @brief The type of a code span in a syntax-highlighted document.
/// While languages have many more tokens, these all fall into the listed handful categories
/// for the purpose of syntax highlighting.
enum struct Code_Span_Type : Default_Underlying {
    text,
    identifier,
    type_name,
    variable_name,
    function_name,
    annotation_name,
    bracket,
    number,
    string,
    comment,
    operation,
    punctuation,
    keyword,
    boolean_literal,
    error,

    diagnostic_text,
    diagnostic_error_text,
    diagnostic_code_position,
    diagnostic_error,
    diagnostic_warning,
    diagnostic_note,
    diagnostic_line_number,
    diagnostic_punctuation,
    diagnostic_position_indicator,
    diagnostic_code_citation,
    diagnostic_internal_error_notice,
    diagnostic_operand,
    diagnostic_operator,
    diagnostic_tag,
    diagnostic_attribute,
    diagnostic_internal,
    diagnostic_escape,

    html_preamble,
    html_comment,
    html_tag_identifier,
    html_tag_bracket,
    html_attribute_key,
    html_attribute_equal,
    html_attribute_value,
    html_inner_text
};

} // namespace mmml

#endif
