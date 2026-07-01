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
    assign,
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

#define COWEL_BUILTIN_OPERATION_ENUM_DATA(F)                                                       \
    F(tautology)                                                                                   \
    F(contradiction)                                                                               \
    F(let_dynamic)                                                                                 \
    F(let_int)                                                                                     \
    F(let_float)                                                                                   \
    F(let_bool)                                                                                    \
    F(let_str)                                                                                     \
    F(let_unit)                                                                                    \
    F(let_null)                                                                                    \
    F(let_regex)                                                                                   \
    F(let_block)                                                                                   \
    F(let_group)                                                                                   \
    F(assign_dynamic)                                                                              \
    F(assign_int)                                                                                  \
    F(assign_float)                                                                                \
    F(assign_bool)                                                                                 \
    F(assign_str)                                                                                  \
    F(assign_unit)                                                                                 \
    F(assign_null)                                                                                 \
    F(assign_regex)                                                                                \
    F(assign_block)                                                                                \
    F(assign_group)                                                                                \
    F(logical_or_dynamic)                                                                          \
    F(logical_or_bool_bool)                                                                        \
    F(logical_and_dynamic)                                                                         \
    F(logical_and_bool_bool)                                                                       \
    F(eq_dynamic)                                                                                  \
    F(eq_bool_bool)                                                                                \
    F(eq_int_int)                                                                                  \
    F(eq_float_float)                                                                              \
    F(eq_str_str)                                                                                  \
    F(eq_dynamic_groups)                                                                           \
    F(ne_dynamic)                                                                                  \
    F(ne_bool_bool)                                                                                \
    F(ne_int_int)                                                                                  \
    F(ne_float_float)                                                                              \
    F(ne_str_str)                                                                                  \
    F(lt_dynamic)                                                                                  \
    F(lt_int_int)                                                                                  \
    F(lt_float_float)                                                                              \
    F(lt_str_str)                                                                                  \
    F(gt_dynamic)                                                                                  \
    F(gt_int_int)                                                                                  \
    F(gt_float_float)                                                                              \
    F(gt_str_str)                                                                                  \
    F(le_dynamic)                                                                                  \
    F(le_int_int)                                                                                  \
    F(le_float_float)                                                                              \
    F(le_str_str)                                                                                  \
    F(ge_dynamic)                                                                                  \
    F(ge_int_int)                                                                                  \
    F(ge_float_float)                                                                              \
    F(ge_str_str)                                                                                  \
    F(plus_dynamic)                                                                                \
    F(plus_int_int)                                                                                \
    F(plus_float_float)                                                                            \
    F(minus_dynamic)                                                                               \
    F(minus_int_int)                                                                               \
    F(minus_float_float)                                                                           \
    F(multiply_dynamic)                                                                            \
    F(multiply_int_int)                                                                            \
    F(multiply_float_float)                                                                        \
    F(div_dynamic)                                                                                 \
    F(div_float_float)                                                                             \
    F(rem_to_zero_dynamic)                                                                         \
    F(rem_to_zero_int_int)

#define COWEL_BUILTIN_OPERATION_ENUMERATOR(id) id,

enum struct Builtin_Operation_Kind : Default_Underlying {
    COWEL_BUILTIN_OPERATION_ENUM_DATA(COWEL_BUILTIN_OPERATION_ENUMERATOR)
};

/// @brief Returns `true` of `kind` is a dynamic operation.
/// That is, an operation whose operand types are checked dynamically
/// rather than requiring a type check in advance.
[[nodiscard]]
bool builtin_operation_kind_is_dynamically_typed(Builtin_Operation_Kind kind);

[[nodiscard]]
constexpr Builtin_Operation_Kind
binary_expression_kind_builtin_operation_kind(const Binary_Expression_Kind kind)
{
    using enum Binary_Expression_Kind;
    switch (kind) {
    case assign: //
        return Builtin_Operation_Kind::assign_dynamic;
    case logical_or: //
        return Builtin_Operation_Kind::logical_or_dynamic;
    case logical_and: //
        return Builtin_Operation_Kind::logical_and_dynamic;
    case eq: //
        return Builtin_Operation_Kind::eq_dynamic;
    case ne: //
        return Builtin_Operation_Kind::ne_dynamic;
    case lt: //
        return Builtin_Operation_Kind::lt_dynamic;
    case gt: //
        return Builtin_Operation_Kind::gt_dynamic;
    case le: //
        return Builtin_Operation_Kind::le_dynamic;
    case ge: //
        return Builtin_Operation_Kind::ge_dynamic;
    case plus: //
        return Builtin_Operation_Kind::plus_dynamic;
    case minus: //
        return Builtin_Operation_Kind::minus_dynamic;
    case multiply: //
        return Builtin_Operation_Kind::multiply_dynamic;
    case divide: //
        return Builtin_Operation_Kind::div_dynamic;
    case remainder: //
        return Builtin_Operation_Kind::rem_to_zero_dynamic;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid expression kind.");
}

}; // namespace cowel

#endif
