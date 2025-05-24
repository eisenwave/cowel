#include <string_view>
#include <vector>

#include "cowel/util/from_chars.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/to_chars.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/directive_processing.hpp"

namespace cowel {

namespace {

[[nodiscard]] [[maybe_unused]]
std::pmr::string vec_to_string(const std::pmr::vector<char>& v)
{
    return { v.data(), v.size(), v.get_allocator() };
}

long long operate(Expression_Type type, long long x, long long y)
{
    switch (type) {
    case Expression_Type::add: return x + y;
    case Expression_Type::subtract: return x - y;
    case Expression_Type::multiply: return x * y;
    case Expression_Type::divide: return x / y;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid expression type.");
}

} // namespace

void Expression_Behavior::generate_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    Context& context
) const
{
    long long result = expression_type_neutral_element(m_type);
    for (const ast::Argument& arg : d.get_arguments()) {
        std::pmr::vector<char8_t> arg_text { context.get_transient_memory() };
        to_plaintext(arg_text, arg.get_content(), context);
        const auto arg_string = as_u8string_view(arg_text);

        const std::optional x = from_chars<long long>(arg_string);
        if (!x) {
            const std::u8string_view message[] {
                u8"Unable to perform operation because \"",
                arg_string,
                u8"\" is not a valid integer.",
            };
            context.try_error(diagnostic::arithmetic_parse, arg.get_source_span(), message);
            try_generate_error_plaintext(out, d, context);
            return;
        }
        if (m_type == Expression_Type::divide && *x == 0) {
            const std::u8string_view message[] {
                u8"The dividend \"",
                arg_string,
                u8"\" evaluated to zero, and a division by zero would occur.",
            };
            context.try_error(diagnostic::arithmetic_div_by_zero, arg.get_source_span(), message);
            try_generate_error_plaintext(out, d, context);
            return;
        }
        result = operate(m_type, result, *x);
    }
    const Characters8 result_chars = to_characters8(result);
    append(out, result_chars.as_string());
}

void Variable_Behavior::generate_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    const Argument_Matcher& args,
    Context& context
) const
{
    std::pmr::vector<char8_t> data { context.get_transient_memory() };
    if (argument_to_plaintext(data, d, args, var_parameter, context)) {
        generate_var_plaintext(out, d, as_u8string_view(data), context);
    }
}

void Variable_Behavior::generate_html(
    HTML_Writer& out,
    const ast::Directive& d,
    const Argument_Matcher& args,
    Context& context
) const
{
    std::pmr::vector<char8_t> data { context.get_transient_memory() };
    if (argument_to_plaintext(data, d, args, var_parameter, context)) {
        generate_var_html(out, d, as_u8string_view(data), context);
    }
}

void Get_Variable_Behavior::generate_var_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive&,
    std::u8string_view var,
    Context& context
) const
{
    const auto it = context.get_variables().find(var);
    if (it != context.get_variables().end()) {
        out.insert(out.end(), it->second.begin(), it->second.end());
    }
}

void Get_Variable_Behavior::generate_var_html(
    HTML_Writer& out,
    const ast::Directive&,
    std::u8string_view var,
    Context& context
) const
{
    if (const std::pmr::u8string* const value = context.get_variable(var)) {
        out.write_inner_html(*value);
    }
}

void process(
    Variable_Operation op,
    const ast::Directive& d,
    std::u8string_view var,
    Context& context
)
{
    std::pmr::vector<char8_t> body_string { context.get_transient_memory() };
    to_plaintext(body_string, d.get_content(), context);

    const auto it = context.get_variables().find(var);
    if (op == Variable_Operation::set) {
        std::pmr::u8string value { body_string.data(), body_string.size(),
                                   context.get_persistent_memory() };
        if (it == context.get_variables().end()) {
            std::pmr::u8string key // NOLINT(misc-const-correctness)
                { var.data(), var.size(), context.get_persistent_memory() };
            context.get_variables().emplace(std::move(key), std::move(value));
        }
        else {
            it->second = std::move(value);
        }
    }
}

} // namespace cowel
