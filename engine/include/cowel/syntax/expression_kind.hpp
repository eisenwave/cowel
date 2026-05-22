#ifndef COWEL_EXPRESSION_KIND_HPP
#define COWEL_EXPRESSION_KIND_HPP

#include "cowel/util/assert.hpp"

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

enum struct Binary_Expression_Kind : Default_Underlying {
    logical_or,
    logical_and,
    eq,
    ne,
    lt,
    gt,
    le,
    ge,
    plus,
    minus,
    multiply,
    divide,
    remainder,
};

[[nodiscard]]
constexpr Binary_Expression_Kind
comparison_kind_binary_expression_kind(const Comparison_Expression_Kind kind)
{
    using enum Comparison_Expression_Kind;
    switch (kind) {
    case eq: return Binary_Expression_Kind::eq;
    case ne: return Binary_Expression_Kind::ne;
    case lt: return Binary_Expression_Kind::lt;
    case gt: return Binary_Expression_Kind::gt;
    case le: return Binary_Expression_Kind::le;
    case ge: return Binary_Expression_Kind::ge;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid comparison kind.");
}

[[nodiscard]]
constexpr Comparison_Expression_Kind
binary_expression_kind_comparison_kind(const Binary_Expression_Kind kind)
{
    using enum Binary_Expression_Kind;
    switch (kind) {
    case eq: return Comparison_Expression_Kind::eq;
    case ne: return Comparison_Expression_Kind::ne;
    case lt: return Comparison_Expression_Kind::lt;
    case gt: return Comparison_Expression_Kind::gt;
    case le: return Comparison_Expression_Kind::le;
    case ge: return Comparison_Expression_Kind::ge;
    default: break;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Expression kind is not a comparison.");
}

enum struct Builtin_Operation_Kind : Default_Underlying {
    tautology,
    contradiction,
    logical_or_bool_bool,
    logical_and_bool_bool,
    eq_bool_bool,
    eq_int_int,
    eq_float_float,
    eq_str_str,
    eq_dynamic_groups,
    ne_bool_bool,
    ne_int_int,
    ne_float_float,
    ne_str_str,
    lt_int_int,
    lt_float_float,
    lt_str_str,
    gt_int_int,
    gt_float_float,
    gt_str_str,
    le_int_int,
    le_float_float,
    le_str_str,
    ge_int_int,
    ge_float_float,
    ge_str_str,
    plus_int_int,
    plus_float_float,
    plus_str_str,
    minus_int_int,
    minus_float_float,
    multiply_int_int,
    multiply_float_float,
    div_float_float,
    rem_to_zero_int_int,
};

}; // namespace cowel

#endif
