#include <algorithm>
#include <cstddef>
#include <memory_resource>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "cowel/util/annotated_string.hpp"
#include "cowel/util/from_chars.hpp"
#include "cowel/util/io.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/tty.hpp"

#include "cowel/diagnostic_highlight.hpp"
#include "cowel/fwd.hpp"
#include "cowel/print.hpp"

#include "diff.hpp"

#include "cowel/syntax/ast.hpp"
#include "cowel/syntax/lex.hpp"
#include "cowel/syntax/parse.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

std::ostream& operator<<(std::ostream& out, std::u8string_view str)
{
    return out << as_string_view(str);
}

enum struct Node_Kind : Default_Underlying {
    unit_literal,
    null_literal,
    bool_literal,
    int_literal,
    decimal_float_literal,
    infinity,
    unquoted_member_name,
    unquoted_string,
    bitwise_not,
    logical_not,
    unary_plus,
    unary_minus,
    binary_logical_or,
    binary_logical_and,
    binary_equals,
    binary_not_equals,
    binary_less_than,
    binary_greater_than,
    binary_less_equal,
    binary_greater_equal,
    binary_add,
    binary_subtract,
    binary_multiply,
    binary_divide,
    binary_modulo,
    text,
    escape,
    line_comment,
    directive,
    quoted_member_name,
    quoted_string,
    block,
    group,
    named_argument,
    positional_argument,
    ellipsis,
    expression_splice,
};

struct Node {
    Node_Kind kind;
    std::u8string_view name_or_text {};
    std::vector<Node> children {};

    [[nodiscard]]
    static Node unit(std::u8string_view text)
    {
        return { .kind = Node_Kind::unit_literal, .name_or_text = text };
    }

    [[nodiscard]]
    static Node null(std::u8string_view text)
    {
        return { .kind = Node_Kind::null_literal, .name_or_text = text };
    }

    [[nodiscard]]
    static Node boolean(std::u8string_view text)
    {
        return { .kind = Node_Kind::bool_literal, .name_or_text = text };
    }

    [[nodiscard]]
    static Node unquoted_member_name(std::u8string_view name)
    {
        return { .kind = Node_Kind::unquoted_member_name, .name_or_text = name };
    }

    [[nodiscard]]
    static Node unquoted_string(std::u8string_view text)
    {
        return { .kind = Node_Kind::unquoted_string, .name_or_text = text };
    }

    [[nodiscard]]
    static Node integer(std::u8string_view text)
    {
        return { .kind = Node_Kind::int_literal, .name_or_text = text };
    }

    [[nodiscard]]
    static Node floating_point(std::u8string_view text)
    {
        return { .kind = Node_Kind::decimal_float_literal, .name_or_text = text };
    }

    [[nodiscard]]
    static Node infinity(std::u8string_view text)
    {
        return { .kind = Node_Kind::infinity, .name_or_text = text };
    }

    [[nodiscard]]
    static Node bitwise_not(Node&& operand)
    {
        return { .kind = Node_Kind::bitwise_not, .children = { std::move(operand) } };
    }

    [[nodiscard]]
    static Node logical_not(Node&& operand)
    {
        return { .kind = Node_Kind::logical_not, .children = { std::move(operand) } };
    }

    [[nodiscard]]
    static Node unary_plus(Node&& operand)
    {
        return { .kind = Node_Kind::unary_plus, .children = { std::move(operand) } };
    }

    [[nodiscard]]
    static Node unary_minus(Node&& operand)
    {
        return { .kind = Node_Kind::unary_minus, .children = { std::move(operand) } };
    }

    [[nodiscard]]
    static Node logical_or(Node&& lhs, Node&& rhs)
    {
        return {
            .kind = Node_Kind::binary_logical_or,
            .children = { std::move(lhs), std::move(rhs) },
        };
    }

    [[nodiscard]]
    static Node logical_and(Node&& lhs, Node&& rhs)
    {
        return {
            .kind = Node_Kind::binary_logical_and,
            .children = { std::move(lhs), std::move(rhs) },
        };
    }

    [[nodiscard]]
    static Node equals(Node&& lhs, Node&& rhs)
    {
        return {
            .kind = Node_Kind::binary_equals,
            .children = { std::move(lhs), std::move(rhs) },
        };
    }

    [[nodiscard]]
    static Node not_equals(Node&& lhs, Node&& rhs)
    {
        return {
            .kind = Node_Kind::binary_not_equals,
            .children = { std::move(lhs), std::move(rhs) },
        };
    }

    [[nodiscard]]
    static Node less_than(Node&& lhs, Node&& rhs)
    {
        return {
            .kind = Node_Kind::binary_less_than,
            .children = { std::move(lhs), std::move(rhs) },
        };
    }

    [[nodiscard]]
    static Node greater_than(Node&& lhs, Node&& rhs)
    {
        return {
            .kind = Node_Kind::binary_greater_than,
            .children = { std::move(lhs), std::move(rhs) },
        };
    }

    [[nodiscard]]
    static Node less_equal(Node&& lhs, Node&& rhs)
    {
        return {
            .kind = Node_Kind::binary_less_equal,
            .children = { std::move(lhs), std::move(rhs) },
        };
    }

    [[nodiscard]]
    static Node greater_equal(Node&& lhs, Node&& rhs)
    {
        return {
            .kind = Node_Kind::binary_greater_equal,
            .children = { std::move(lhs), std::move(rhs) },
        };
    }

    [[nodiscard]]
    static Node add(Node&& lhs, Node&& rhs)
    {
        return {
            .kind = Node_Kind::binary_add,
            .children = { std::move(lhs), std::move(rhs) },
        };
    }

    [[nodiscard]]
    static Node subtract(Node&& lhs, Node&& rhs)
    {
        return {
            .kind = Node_Kind::binary_subtract,
            .children = { std::move(lhs), std::move(rhs) },
        };
    }

    [[nodiscard]]
    static Node multiply(Node&& lhs, Node&& rhs)
    {
        return {
            .kind = Node_Kind::binary_multiply,
            .children = { std::move(lhs), std::move(rhs) },
        };
    }

    [[nodiscard]]
    static Node divide(Node&& lhs, Node&& rhs)
    {
        return {
            .kind = Node_Kind::binary_divide,
            .children = { std::move(lhs), std::move(rhs) },
        };
    }

    [[nodiscard]]
    static Node modulo(Node&& lhs, Node&& rhs)
    {
        return {
            .kind = Node_Kind::binary_modulo,
            .children = { std::move(lhs), std::move(rhs) },
        };
    }

    [[nodiscard]]
    static Node text(std::u8string_view text)
    {
        return { .kind = Node_Kind::text, .name_or_text = text };
    }

    [[nodiscard]]
    static Node escape(std::u8string_view text)
    {
        COWEL_ASSERT(text.starts_with(u8'\\'));
        return { .kind = Node_Kind::escape, .name_or_text = text };
    }

    [[nodiscard]]
    static Node line_comment(std::u8string_view text)
    {
        COWEL_ASSERT(text.starts_with(u8"\\:"));
        return { .kind = Node_Kind::line_comment, .name_or_text = text };
    }

    [[nodiscard]]
    static Node directive(std::u8string_view name)
    {
        return { .kind = Node_Kind::directive, .name_or_text = name };
    }
    [[nodiscard]]
    static Node directive(std::u8string_view name, Node&& args, Node&& content)
    {
        COWEL_ASSERT(args.kind == Node_Kind::group);
        COWEL_ASSERT(content.kind == Node_Kind::block);
        return {
            .kind = Node_Kind::directive,
            .name_or_text = name,
            .children = { std::move(args), std::move(content) },
        };
    }
    [[nodiscard]]
    static Node directive_with_arguments(std::u8string_view name, Node&& arguments)
    {
        COWEL_ASSERT(arguments.kind == Node_Kind::group);
        return {
            .kind = Node_Kind::directive,
            .name_or_text = name,
            .children = { std::move(arguments) },
        };
    }
    [[nodiscard]]
    static Node directive_with_content(std::u8string_view name, Node&& content)
    {
        COWEL_ASSERT(content.kind == Node_Kind::block);
        return {
            .kind = Node_Kind::directive,
            .name_or_text = name,
            .children = { std::move(content) },
        };
    }

    [[nodiscard]]
    static Node quoted_member_name(std::vector<Node>&& elements)
    {
        return {
            .kind = Node_Kind::quoted_member_name,
            .children = std::move(elements),
        };
    }

    [[nodiscard]]
    static Node quoted_string(std::vector<Node>&& elements)
    {
        return {
            .kind = Node_Kind::quoted_string,
            .children = std::move(elements),
        };
    }

    [[nodiscard]]
    static Node block(std::vector<Node>&& elements)
    {
        return {
            .kind = Node_Kind::block,
            .children = std::move(elements),
        };
    }
    [[nodiscard]]
    static Node block()
    {
        return block({});
    }

    [[nodiscard]]
    static Node group(std::vector<Node>&& arguments)
    {
        return {
            .kind = Node_Kind::group,
            .children = std::move(arguments),
        };
    }
    [[nodiscard]]
    static Node group()
    {
        return group({});
    }

    [[nodiscard]]
    static Node named(Node&& name, Node&& value)
    {
        COWEL_ASSERT(
            name.kind == Node_Kind::unquoted_member_name
            || name.kind == Node_Kind::quoted_member_name
        );
        return {
            .kind = Node_Kind::named_argument,
            .children = { std::move(name), std::move(value) },
        };
    }
    [[nodiscard]]
    static Node named(std::u8string_view name, Node&& value)
    {
        return named(Node::unquoted_member_name(name), std::move(value));
    }

    [[nodiscard]]
    static Node positional(Node&& value)
    {
        return {
            .kind = Node_Kind::positional_argument,
            .children = { std::move(value) },
        };
    }

    [[nodiscard]]
    static Node ellipsis()
    {
        return { .kind = Node_Kind::ellipsis };
    }

    [[nodiscard]]
    static Node expression_splice(Node&& expression)
    {
        return {
            .kind = Node_Kind::expression_splice,
            .children = { std::move(expression) },
        };
    }

    [[nodiscard]]
    static Node from(const ast::Markup_Element& actual)
    {
        if (const auto* const d = std::get_if<ast::Directive>(&actual)) {
            return from(*d);
        }
        if (const auto* const p = std::get_if<ast::Primary>(&actual)) {
            return from(*p);
        }
        if (const auto* const e = std::get_if<ast::Expression>(&actual)) {
            return expression_splice(from(*e));
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid markup element.");
    }

    [[nodiscard]]
    static Node from(const ast::Primary& actual)
    {
        switch (const auto kind = actual.get_kind()) {

        case ast::Primary_Kind::unit_literal: {
            return unit(actual.get_source());
        }

        case ast::Primary_Kind::null_literal: {
            return null(actual.get_source());
        }

        case ast::Primary_Kind::bool_literal: {
            return boolean(actual.get_source());
        }

        case ast::Primary_Kind::unquoted_string: {
            return unquoted_string(actual.get_source());
        }

        case ast::Primary_Kind::int_literal: {
            return integer(actual.get_source());
        }

        case ast::Primary_Kind::decimal_float_literal: {
            return floating_point(actual.get_source());
        }

        case ast::Primary_Kind::infinity: {
            return infinity(actual.get_source());
        }

        case ast::Primary_Kind::text: {
            return text(actual.get_source());
        }

        case ast::Primary_Kind::escape: {
            return escape(actual.get_source());
        }

        case ast::Primary_Kind::comment: {
            return line_comment(actual.get_source());
        }

        case ast::Primary_Kind::quoted_string:
        case ast::Primary_Kind::block: {
            std::vector<Node> children;
            children.reserve(actual.get_elements_size());
            for (const ast::Markup_Element& c : actual.get_elements()) {
                children.push_back(from(c));
            }
            return kind == ast::Primary_Kind::quoted_string ? quoted_string(std::move(children))
                                                            : block(std::move(children));
        }

        case ast::Primary_Kind::group: {
            std::vector<Node> group_members;
            group_members.reserve(actual.get_members_size());
            for (const ast::Group_Member& member : actual.get_members()) {
                group_members.push_back(from(member));
            }
            return group(std::move(group_members));
        }
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid primary.");
    }

    [[nodiscard]]
    static Node from(const ast::Directive& actual)
    {
        if (const ast::Primary* args = actual.get_arguments()) {
            if (const ast::Primary* content = actual.get_content()) {
                return directive(actual.get_name(), from(*args), from(*content));
            }
            else { // NOLINT(readability-else-after-return)
                return directive_with_arguments(actual.get_name(), from(*args));
            }
        }
        else {
            if (const ast::Primary* content = actual.get_content()) {
                return directive_with_content(actual.get_name(), from(*content));
            }
            else { // NOLINT(readability-else-after-return)
                return directive(actual.get_name());
            }
        }
    }

    [[nodiscard]]
    static Node from(const ast::Unary_Expression& actual)
    {
        Node operand = from(actual.get_operand());
        switch (actual.get_kind()) {
        case Unary_Expression_Kind::bitwise_not: return bitwise_not(std::move(operand));
        case Unary_Expression_Kind::logical_not: return logical_not(std::move(operand));
        case Unary_Expression_Kind::plus: return unary_plus(std::move(operand));
        case Unary_Expression_Kind::minus: return unary_minus(std::move(operand));
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid expression kind.");
    }

    [[nodiscard]]
    static Node from(const ast::Binary_Expression& actual)
    {
        Node lhs = from(actual.get_lhs());
        Node rhs = from(actual.get_rhs());
        switch (actual.get_kind()) {
        case Binary_Expression_Kind::logical_or: return logical_or(std::move(lhs), std::move(rhs));
        case Binary_Expression_Kind::logical_and:
            return logical_and(std::move(lhs), std::move(rhs));
        case Binary_Expression_Kind::eq: return equals(std::move(lhs), std::move(rhs));
        case Binary_Expression_Kind::ne: return not_equals(std::move(lhs), std::move(rhs));
        case Binary_Expression_Kind::lt: return less_than(std::move(lhs), std::move(rhs));
        case Binary_Expression_Kind::gt: return greater_than(std::move(lhs), std::move(rhs));
        case Binary_Expression_Kind::le: return less_equal(std::move(lhs), std::move(rhs));
        case Binary_Expression_Kind::ge: return greater_equal(std::move(lhs), std::move(rhs));
        case Binary_Expression_Kind::plus: return add(std::move(lhs), std::move(rhs));
        case Binary_Expression_Kind::minus: return subtract(std::move(lhs), std::move(rhs));
        case Binary_Expression_Kind::multiply: return multiply(std::move(lhs), std::move(rhs));
        case Binary_Expression_Kind::divide: return divide(std::move(lhs), std::move(rhs));
        case Binary_Expression_Kind::remainder: return modulo(std::move(lhs), std::move(rhs));
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid expression kind.");
    }

    [[nodiscard]]
    static Node from(const ast::Group_Member& arg)
    {
        switch (arg.get_kind()) {
        case ast::Member_Kind::ellipsis: {
            return ellipsis();
        }
        case ast::Member_Kind::named: {
            return Node::named(from_member_name(arg.get_name()), from(arg.get_value()));
        }
        case ast::Member_Kind::positional: {
            return positional(from(arg.get_value()));
        }
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid argument type.");
    }

    [[nodiscard]]
    static Node from(const ast::Expression& arg)
    {
        const auto visitor = [&](const auto& v) -> Node { return from(v); };
        return std::visit(visitor, arg);
    }

    [[nodiscard]]
    static Node from_member_name(const ast::Primary& arg)
    {
        if (arg.get_kind() == ast::Primary_Kind::unquoted_string) {
            return unquoted_member_name(arg.get_source());
        }
        COWEL_ASSERT(arg.get_kind() == ast::Primary_Kind::quoted_string);
        std::vector<Node> children;
        children.reserve(arg.get_elements_size());
        for (const ast::Markup_Element& c : arg.get_elements()) {
            children.push_back(from(c));
        }
        return quoted_member_name(std::move(children));
    }

    [[maybe_unused]]
    friend bool operator==(const Node&, const Node&)
        = default;
};

template <typename T, typename Alloc>
std::ostream& operator<<(std::ostream& os, const std::vector<T, Alloc>& vec)
{
    for (std::size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) {
            os << ", ";
        }
        os << vec[i]; // Uses operator<< of T
    }
    return os;
}

[[nodiscard]]
constexpr Fixed_String8<2> special_escaped(char8_t c)
{
    switch (c) {
    case u8'\n': return u8"\\n"sv;
    case u8'\v': return u8"\\v"sv;
    case u8'\t': return u8"\\t"sv;
    case u8'\r': return u8"\\r"sv;
    default: return c;
    }
}

std::ostream& operator<<(std::ostream& out, const Node& node)
{
    switch (node.kind) {
    case Node_Kind::unit_literal: {
        return out << "UnitLiteral(" << node.name_or_text << ')';
    }

    case Node_Kind::null_literal: {
        return out << "NullLiteral(" << node.name_or_text << ')';
    }

    case Node_Kind::bool_literal: {
        return out << "BoolLiteral(" << node.name_or_text << ')';
    }

    case Node_Kind::int_literal: {
        return out << "IntLiteral(" << node.name_or_text << ')';
    }

    case Node_Kind::decimal_float_literal: {
        return out << "FloatLiteral(" << node.name_or_text << ')';
    }

    case Node_Kind::infinity: {
        return out << "Infinity(" << node.name_or_text << ')';
    }

    case Node_Kind::unquoted_member_name: {
        return out << "UnqMemberName(" << node.name_or_text << ')';
    }

    case Node_Kind::unquoted_string: {
        return out << "UnqString(" << node.name_or_text << ')';
    }

    case Node_Kind::bitwise_not: {
        return out << "BitwiseNot(" << node.children.front() << ')';
    }
    case Node_Kind::logical_not: {
        return out << "LogicalNot(" << node.children.front() << ')';
    }
    case Node_Kind::unary_plus: {
        return out << "UnaryPlus(" << node.children.front() << ')';
    }
    case Node_Kind::unary_minus: {
        return out << "UnaryMinus(" << node.children.front() << ')';
    }
    case Node_Kind::binary_logical_or: {
        return out << "LogicalOr(" << node.children.front() << ", " << node.children.back() << ')';
    }
    case Node_Kind::binary_logical_and: {
        return out << "LogicalAnd(" << node.children.front() << ", " << node.children.back() << ')';
    }
    case Node_Kind::binary_equals: {
        return out << "Equals(" << node.children.front() << ", " << node.children.back() << ')';
    }
    case Node_Kind::binary_not_equals: {
        return out << "NotEquals(" << node.children.front() << ", " << node.children.back() << ')';
    }
    case Node_Kind::binary_less_than: {
        return out << "LessThan(" << node.children.front() << ", " << node.children.back() << ')';
    }
    case Node_Kind::binary_greater_than: {
        return out << "GreaterThan(" << node.children.front() << ", " << node.children.back()
                   << ')';
    }
    case Node_Kind::binary_less_equal: {
        return out << "LessEqual(" << node.children.front() << ", " << node.children.back() << ')';
    }
    case Node_Kind::binary_greater_equal: {
        return out << "GreaterEqual(" << node.children.front() << ", " << node.children.back()
                   << ')';
    }
    case Node_Kind::binary_add: {
        return out << "Add(" << node.children.front() << ", " << node.children.back() << ')';
    }
    case Node_Kind::binary_subtract: {
        return out << "Subtract(" << node.children.front() << ", " << node.children.back() << ')';
    }
    case Node_Kind::binary_multiply: {
        return out << "Multiply(" << node.children.front() << ", " << node.children.back() << ')';
    }
    case Node_Kind::binary_divide: {
        return out << "Divide(" << node.children.front() << ", " << node.children.back() << ')';
    }
    case Node_Kind::binary_modulo: {
        return out << "Modulo(" << node.children.front() << ", " << node.children.back() << ')';
    }

    case Node_Kind::text: {
        out << "Text(";
        for (const char8_t c : node.name_or_text) {
            out << special_escaped(c).as_string();
        }
        out << ')';
        break;
    }

    case Node_Kind::line_comment: {
        return out << "Comment(" << node.name_or_text << ')';
    }

    case Node_Kind::directive: {
        out << '\\' << node.name_or_text << '(';
        for (const auto& c : node.children) {
            out << c;
        }
        return out << ')';
    }

    case Node_Kind::escape: {
        return out << "Escape(" << node.name_or_text << ')';
    }

    case Node_Kind::quoted_member_name: {
        return out << "QuotedMemberName(" << node.children << ')';
    }

    case Node_Kind::quoted_string: {
        return out << "QuotedString(" << node.children << ')';
    }

    case Node_Kind::group: {
        return out << "Group(" << node.children << ')';
    }

    case Node_Kind::block: {
        return out << "Block(" << node.children << ')';
    }

    case Node_Kind::named_argument: {
        return out << "NamedArg(" << node.name_or_text << "){" << node.children << '}';
    }

    case Node_Kind::positional_argument: {
        return out << "PosArg" << "{" << node.children << '}';
    }

    case Node_Kind::ellipsis: {
        return out << "Ellipsis(" << node.name_or_text << ')';
    }

    case Node_Kind::expression_splice: {
        return out << "ExpressionSplice(" << node.children.front() << ')';
    }
    }
    return out;
}

struct [[nodiscard]] Actual_Document {
    std::pmr::vector<char8_t> source;
    ast::Pmr_Vector<ast::Markup_Element> content;

    [[nodiscard]]
    std::u8string_view source_string() const
    {
        return { source.data(), source.size() };
    }

    [[nodiscard]]
    ast::Pmr_Vector<Node> to_expected() const
    {
        ast::Pmr_Vector<Node> result { content.get_allocator() };
        result.reserve(content.size());
        for (const auto& actual : content) {
            result.push_back(Node::from(actual));
        }
        return result;
    }
};

struct Parsed_File {
    std::pmr::vector<char8_t> source;
    std::pmr::vector<Token> tokens;
    std::pmr::vector<CST_Instruction> instructions;

    [[nodiscard]]
    std::u8string_view get_source_string() const
    {
        return { source.data(), source.size() };
    }
};

enum struct Parse_Error_Stage : Default_Underlying {
    /// @brief Loading the UTF-8 source failed.
    load,
    /// @brief Loading succeeded, but lexing failed.
    lex,
    /// @brief Lexing succeeded, but parsing failed.
    parse,
};

[[nodiscard]]
Result<Parsed_File, Parse_Error_Stage> parse_file(
    const std::u8string_view path,
    std::pmr::memory_resource* const memory,
    const bool silence_parse_error = false
)
{
    Parsed_File result {
        .source = std::pmr::vector<char8_t> { memory },
        .tokens = std::pmr::vector<Token> { memory },
        .instructions = std::pmr::vector<CST_Instruction> { memory },
    };

    const Result<void, cowel::IO_Error_Code> r = load_utf8_file(result.source, path);
    if (!r) {
        Diagnostic_String out { memory };
        print_io_error(out, path, r.error());
        print_code_string_stdout(out);
        return Parse_Error_Stage::load;
    }

    const std::convertible_to<Parse_Error_Consumer> auto on_error
        = [&](std::u8string_view /* id */, const Source_Span& location, Char_Sequence8 message) {
              Diagnostic_String out { memory };
              print_file_position(out, path, location);
              out.append(u8' ');
              append_char_sequence(out, message, Diagnostic_Highlight::text);
              out.append(u8'\n');
              print_code_string_stdout(out);
          };
    if (!lex(result.tokens, result.get_source_string(), on_error)) {
        return Parse_Error_Stage::lex;
    }
    if (!parse(
            result.instructions, result.tokens,
            silence_parse_error ? Parse_Error_Consumer {} : on_error
        )) {
        return Parse_Error_Stage::parse;
    }
    return result;
}

[[nodiscard]]
std::optional<Actual_Document>
parse_and_build_file(const std::u8string_view file_name, std::pmr::memory_resource* const memory)
{
    std::pmr::u8string path { u8"engine/test/files/parse/", memory };
    path += file_name;

    Result<Parsed_File, Parse_Error_Stage> parsed = parse_file(path, memory);
    if (!parsed) {
        return {};
    }
    const std::u8string_view source_string = parsed->get_source_string();

    ast::Pmr_Vector<ast::Markup_Element> content
        = build_ast(source_string, File_Id::main, parsed->tokens, parsed->instructions, memory);
    return Actual_Document { std::move(parsed->source), std::move(content) };
}

void append_instruction(Diagnostic_String& out, const CST_Instruction& ins)
{
    out.append(cst_instruction_kind_name(ins.kind), Diagnostic_Highlight::tag);
    if (cst_instruction_kind_has_operand(ins.kind)) {
        out.append(u8' ');
        out.append_integer(ins.n);
    }
    else if (ins.n != 0) {
        out.append(u8' ');
        out.append_integer(ins.n, Diagnostic_Highlight::error_text);
    }
}

void dump_instructions(
    Diagnostic_String& out,
    std::span<const CST_Instruction> instructions,
    std::u8string_view indent = {}
)
{
    for (const auto& i : instructions) {
        out.append(indent);
        append_instruction(out, i);
        out.append(u8'\n');
    }
}

std::optional<CST_Instruction_Kind> cst_instruction_kind_from_name(std::u8string_view name)
{
    struct Name_And_Instruction_Kind {
        std::u8string_view name;
        CST_Instruction_Kind kind;
    };

#define COWEL_CST_INSTRUCTION_KIND_TABLE_ENTRY(id, name) { u8##name##sv, CST_Instruction_Kind::id },

    static constexpr auto table = [] {
        auto result = std::to_array<Name_And_Instruction_Kind>(
            { COWEL_CST_INSTRUCTION_KIND_ENUM_DATA(COWEL_CST_INSTRUCTION_KIND_TABLE_ENTRY) }
        );
        std::ranges::sort(result, {}, &Name_And_Instruction_Kind::name);
        return result;
    }();
    static_assert(std::ranges::is_sorted(table, {}, &Name_And_Instruction_Kind::name));

    // NOLINTNEXTLINE(readability-qualified-auto)
    const auto result = std::ranges::lower_bound(table, name, {}, &Name_And_Instruction_Kind::name);
    if (result == table.end() || result->name != name) {
        return {};
    }
    return result->kind;
}

std::optional<std::vector<CST_Instruction>> load_parse_expectations(std::u8string_view path)
{
    std::pmr::monotonic_buffer_resource memory;
    std::pmr::vector<char8_t> raw { &memory };
    if (const Result<void, IO_Error_Code> r = load_utf8_file(raw, path); !r) {
        Diagnostic_String error;
        print_io_error(error, path, r.error());
        print_code_string_stdout(error);
        return std::nullopt;
    }

    std::vector<CST_Instruction> result;
    std::u8string_view remaining { raw.data(), raw.size() };

    while (!remaining.empty()) {
        const std::size_t newline = remaining.find(u8'\n');
        std::u8string_view line = trim_ascii_blank(remaining.substr(0, newline));
        remaining = newline == std::u8string_view::npos ? u8"" : remaining.substr(newline + 1);

        if (line.empty()) {
            continue;
        }

        const std::size_t space = line.find(u8' ');
        const std::u8string_view kind_name = line.substr(0, space);
        const std::optional<CST_Instruction_Kind> kind = cst_instruction_kind_from_name(kind_name);
        if (!kind) {
            Diagnostic_String error;
            error.append(u8"Unknown instruction kind: ", Diagnostic_Highlight::error_text);
            error.append(kind_name, Diagnostic_Highlight::tag);
            error.append(u8'\n');
            print_code_string_stdout(error);
            return std::nullopt;
        }

        std::size_t n = 0;
        if (space != std::u8string_view::npos) {
            const std::u8string_view operand = trim_ascii_blank_left(line.substr(space));
            if (from_characters(operand, n).ec != std::errc {}) {
                Diagnostic_String error;
                error.append(u8"Invalid operand: ", Diagnostic_Highlight::error_text);
                error.append(operand, Diagnostic_Highlight::tag);
                error.append(u8'\n');
                print_code_string_stdout(error);
                return std::nullopt;
            }
        }

        result.push_back({ *kind, n });
    }

    return result;
}

bool run_parse_test(
    const std::u8string_view file_name,
    const std::span<const CST_Instruction> expected
)
{
    for (const auto& e : expected) {
        COWEL_ASSERT(cst_instruction_kind_has_operand(e.kind) || e.n == 0);
    }

    constexpr std::u8string_view indent = u8"    ";

    std::pmr::monotonic_buffer_resource memory;
    std::pmr::u8string path { u8"engine/test/files/parse/", &memory };
    path += file_name;
    Result<Parsed_File, Parse_Error_Stage> actual = parse_file(path, &memory);
    if (!actual) {
        Diagnostic_String error;
        error.append(
            u8"Test failed because file couldn't be loaded and parsed.\n",
            Diagnostic_Highlight::error_text
        );
        print_code_string_stdout(error);
        return false;
    }
    if (!std::ranges::equal(expected, actual->instructions)) {
        Diagnostic_String error;
        error.append(
            u8"Test failed because expected parser output isn't matched.\n",
            Diagnostic_Highlight::error_text
        );
        error.append(u8"Expected:\n", Diagnostic_Highlight::text);
        Diagnostic_String expected_text;
        dump_instructions(error, expected, indent);
        dump_instructions(expected_text, expected);

        error.append(u8"Actual:\n", Diagnostic_Highlight::text);
        Diagnostic_String actual_text;
        dump_instructions(error, actual->instructions, indent);
        dump_instructions(actual_text, actual->instructions);

        error.append(
            u8"Test output instructions deviate from expected as follows:\n"sv,
            Diagnostic_Highlight::error_text
        );
        print_lines_diff(error, expected_text.get_text(), actual_text.get_text());

        print_code_string_stdout(error);
        return false;
    }
    return true;
}

[[nodiscard]]
bool run_parse_test(
    const std::u8string_view cow_file_name,
    const std::u8string_view expectations_file
)
{
    std::pmr::monotonic_buffer_resource memory;
    std::pmr::u8string path { u8"engine/test/files/parse/", &memory };
    path += expectations_file;

    const std::optional<std::vector<CST_Instruction>> expected = load_parse_expectations(path);
    if (!expected) {
        return false;
    }
    return run_parse_test(cow_file_name, *expected);
}

/// @brief Returns `true` if parsing `engine/test/files/parse/failures/${file_name}`
/// results in a parser error.
/// This is primarily useful for verifying that no invalid markup is accepted
/// and that the parser doesn't run into an infinite loop or crash on invalid input.
[[nodiscard]]
bool run_parse_fail_test(const std::u8string_view file_name)
{
    std::pmr::monotonic_buffer_resource memory;
    std::pmr::u8string path { u8"engine/test/files/parse/failures/", &memory };
    path += file_name;

    constexpr bool silence_parse_error = true;
    const Result<Parsed_File, Parse_Error_Stage> result
        = parse_file(path, &memory, silence_parse_error);
    return !result && result.error() == Parse_Error_Stage::parse;
}

// NOLINTBEGIN(bugprone-unchecked-optional-access)
#define COWEL_PARSE_AND_BUILD_BOILERPLATE(...)                                                     \
    std::optional<Actual_Document> parsed = parse_and_build_file(__VA_ARGS__, &memory);            \
    ASSERT_TRUE(parsed);                                                                           \
    const auto actual = parsed->to_expected();                                                     \
    ASSERT_EQ(expected, actual)
// NOLINTEND(bugprone-unchecked-optional-access)

TEST(Parse, file_tests)
{
    constexpr auto filter = [](const fs::directory_entry& entry) -> bool {
        const fs::path& path = entry.path();
        return path.native().ends_with(".cow") || path.native().ends_with(".cowel");
    };

    std::pmr::monotonic_buffer_resource memory;

    std::pmr::vector<fs::path> test_paths { &memory };
    find_files_recursively(test_paths, "engine/test/files/parse", filter);
    std::ranges::sort(test_paths);

    const fs::path parse_root = "engine/test/files/parse";

    bool overall_success = true;
    for (const fs::path& source_path : test_paths) {
        fs::path expectation_path = source_path;
        expectation_path.replace_extension(".expected");

        if (!fs::is_regular_file(expectation_path)) {
            continue;
        }

        const std::u8string source_test_path = source_path.generic_u8string();
        const std::u8string source_relative
            = fs::relative(source_path, parse_root).generic_u8string();
        const std::u8string expectation_relative
            = fs::relative(expectation_path, parse_root).generic_u8string();

        if (!run_parse_test(source_relative, expectation_relative)) {
            overall_success = false;
        }
        else {
            Diagnostic_String out;
            print_location_of_file(out, source_test_path);
            out.append(u8' ');
            out.append(u8"OK", Diagnostic_Highlight::success);
            out.append(u8'\n');
            print_code_string_stdout(out);
        }
    }

    EXPECT_TRUE(overall_success);
}

TEST(Parse_And_Build, empty)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected { &memory };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"empty.cow");
}

TEST(Parse_And_Build, arguments_comments_1)
{
    static std::pmr::monotonic_buffer_resource memory;

    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive_with_arguments(u8"a", Node::group()),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"arguments/comments_1.cow");
}

TEST(Parse_And_Build, arguments_comments_2)
{
    static std::pmr::monotonic_buffer_resource memory;

    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive_with_arguments(
                u8"b",
                Node::group(
                    {
                        Node::positional({ Node::unquoted_string(u8"text") }),
                        Node::named(u8"named", Node::unquoted_string(u8"arg")),
                    }
                )
            ),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"arguments/comments_2.cow");
}

TEST(Parse_And_Build, arguments_ellipsis)
{
    static std::pmr::monotonic_buffer_resource memory;

    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive_with_arguments(u8"x", Node::group({ Node::ellipsis() })),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"arguments/ellipsis.cow");
}

TEST(Parse_And_Build, group_1)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive_with_arguments(
                u8"d",
                Node::group(
                    {
                        Node::positional({ Node::unquoted_string(u8"x") }),
                        Node::positional({ Node::group({}) }),
                    }
                )
            ),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"arguments/group_1.cow");
}

TEST(Parse_And_Build, group_2)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive_with_arguments(
                u8"d",
                Node::group(
                    { Node::named(
                        u8"n",
                        Node::group(
                            {
                                Node::positional({ Node::unquoted_string(u8"x") }),
                                Node::positional({ Node::unquoted_string(u8"y") }),
                            }
                        )
                    ) }
                )
            ),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"arguments/group_2.cow");
}

TEST(Parse_And_Build, group_3)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive_with_arguments(
                u8"d", Node::group({ Node::positional({ Node::group({}) }) })
            ),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"arguments/group_3.cow");
}

TEST(Parse_And_Build, group_4)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive_with_arguments(
                u8"d",
                Node::group(
                    {
                        Node::positional({ Node::unquoted_string(u8"x") }),
                        Node::positional(
                            { Node::group({ Node::positional({ Node::unquoted_string(u8"x") }) }) }
                        ),
                        Node::positional(
                            { Node::group({ Node::named(u8"x", Node::integer(u8"0")) }) }
                        ),
                        Node::positional({ Node::unquoted_string(u8"x") }),
                    }
                )
            ),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"arguments/group_4.cow");
}

TEST(Parse_And_Build, expression_splice_contexts)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected {
        {
            Node::expression_splice(Node::integer(u8"3")),
            Node::text(u8"\n"),
            Node::directive(
                u8"d",
                Node::group(
                    {
                        Node::positional(
                            { Node::quoted_string(
                                {
                                    Node::text(u8"x "),
                                    Node::expression_splice(Node::integer(u8"1")),
                                    Node::text(u8" y"),
                                }
                            ) }
                        ),
                    }
                ),
                Node::block({ Node::expression_splice(Node::integer(u8"2")) })
            ),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"expression_splice/contexts.cow");
}

TEST(Parse_And_Build, expression_splice_nesting)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected {
        {
            Node::expression_splice(Node::group({})),
            Node::text(u8"\n"),
            Node::expression_splice(
                Node::group(
                    {
                        Node::positional(Node::integer(u8"1")),
                        Node::positional(Node::integer(u8"2")),
                    }
                )
            ),
            Node::text(u8"\n"),
            Node::expression_splice(Node::integer(u8"1")),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"expression_splice/nesting.cow");
}

TEST(Parse_And_Build, expression_splice_source_text)
{
    static std::pmr::monotonic_buffer_resource memory;

    std::optional<Actual_Document> parsed
        = parse_and_build_file(u8"expression_splice/source_text.cow", &memory);
    ASSERT_TRUE(parsed);

    std::vector<std::u8string_view> expression_sources;
    for (const auto& element : parsed->content) {
        if (const auto* const expression = element.try_as_expression()) {
            expression_sources.push_back(expression->get_source());
        }
    }

    ASSERT_EQ(expression_sources.size(), 2);
    EXPECT_EQ(expression_sources[0], u8"\\(~1)");
    EXPECT_EQ(expression_sources[1], u8"\\(1 + 2)");
}

TEST(Parse_And_Build, floats)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive_with_arguments(
                u8"d",
                Node::group(
                    {
                        Node::positional(Node::floating_point(u8"0."sv)),
                        Node::positional(Node::floating_point(u8".0"sv)),
                        Node::positional(Node::floating_point(u8"0.0"sv)),
                        Node::positional(Node::floating_point(u8"0e0"sv)),
                        Node::positional(Node::floating_point(u8"0.e0"sv)),
                        Node::positional(Node::floating_point(u8".0e0"sv)),
                        Node::positional(Node::floating_point(u8"0.0e0"sv)),
                        Node::positional(Node::floating_point(u8"0e0"sv)),
                        Node::positional(Node::floating_point(u8"0e+0"sv)),
                        Node::positional(Node::floating_point(u8"0e-0"sv)),
                        Node::positional(Node::floating_point(u8"0E0"sv)),
                        Node::positional(Node::floating_point(u8"0E+0"sv)),
                        Node::positional(Node::floating_point(u8"0E-0"sv)),
                        Node::positional(Node::floating_point(u8"123.456e789"sv)),
                        Node::positional(
                            Node::unary_minus(Node::floating_point(u8"123.456e789"sv))
                        ),
                    }
                )
            ),
            Node::text(u8"\n"),
        },
        &memory,
    };

    for (const auto& arg : expected.front().children.front().children) {
        COWEL_ASSERT(arg.kind == Node_Kind::positional_argument);
        const auto& float_node = [&] -> const Node& {
            const auto& child = arg.children.front();
            if (child.kind == Node_Kind::decimal_float_literal) {
                return child;
            }
            COWEL_ASSERT(child.children.front().kind == Node_Kind::decimal_float_literal);
            return child.children.front();
        }();
        const std::u8string_view literal = float_node.name_or_text;
        const std::optional<double> d = from_characters_or_inf<double>(literal);
        ASSERT_TRUE(d);
    }

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"floats.cow");
}

TEST(Parse_And_Build, binary_add)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive_with_arguments(
                u8"d",
                Node::group(
                    {
                        Node::positional(Node::add(Node::integer(u8"1"), Node::integer(u8"2"))),
                        Node::positional(Node::add(Node::integer(u8"1"), Node::integer(u8"2"))),
                        Node::positional(Node::add(Node::integer(u8"1"), Node::integer(u8"2"))),
                    }
                )
            ),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"binary_add.cow");
}

TEST(Parse_And_Build, binary_chaining)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive_with_arguments(
                u8"d",
                Node::group(
                    {
                        Node::positional(
                            Node::add(
                                Node::add(Node::integer(u8"1"), Node::integer(u8"2")),
                                Node::integer(u8"3")
                            )
                        ),
                        Node::positional(
                            Node::subtract(
                                Node::subtract(Node::integer(u8"1"), Node::integer(u8"2")),
                                Node::integer(u8"3")
                            )
                        ),
                        Node::positional(
                            Node::multiply(
                                Node::multiply(Node::integer(u8"1"), Node::integer(u8"2")),
                                Node::integer(u8"3")
                            )
                        ),
                        Node::positional(
                            Node::divide(
                                Node::divide(Node::integer(u8"1"), Node::integer(u8"2")),
                                Node::integer(u8"3")
                            )
                        ),
                        Node::positional(
                            Node::modulo(
                                Node::modulo(Node::integer(u8"1"), Node::integer(u8"2")),
                                Node::integer(u8"3")
                            )
                        ),
                    }
                )
            ),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"binary_chaining.cow");
}

TEST(Parse_And_Build, binary_logical)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive_with_arguments(
                u8"d",
                Node::group(
                    {
                        Node::positional(
                            Node::logical_and(Node::integer(u8"0"), Node::integer(u8"1"))
                        ),
                        Node::positional(
                            Node::logical_or(Node::integer(u8"0"), Node::integer(u8"1"))
                        ),
                        Node::positional(
                            Node::logical_and(
                                Node::logical_and(Node::integer(u8"0"), Node::integer(u8"1")),
                                Node::integer(u8"2")
                            )
                        ),
                        Node::positional(
                            Node::logical_or(
                                Node::logical_or(Node::integer(u8"0"), Node::integer(u8"1")),
                                Node::integer(u8"2")
                            )
                        ),
                    }
                )
            ),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"binary_logical.cow");
}

TEST(Parse_And_Build, binary_precedence_add_mult)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive_with_arguments(
                u8"d",
                Node::group(
                    {
                        Node::positional(
                            Node::add(
                                Node::integer(u8"1"),
                                Node::multiply(Node::integer(u8"2"), Node::integer(u8"3"))
                            )
                        ),
                        Node::positional(
                            Node::add(
                                Node::multiply(Node::integer(u8"1"), Node::integer(u8"2")),
                                Node::integer(u8"3")
                            )
                        ),
                        Node::positional(
                            Node::add(
                                Node::integer(u8"1"),
                                Node::divide(Node::integer(u8"2"), Node::integer(u8"3"))
                            )
                        ),
                        Node::positional(
                            Node::add(
                                Node::divide(Node::integer(u8"1"), Node::integer(u8"2")),
                                Node::integer(u8"3")
                            )
                        ),
                        Node::positional(
                            Node::add(
                                Node::integer(u8"1"),
                                Node::modulo(Node::integer(u8"2"), Node::integer(u8"3"))
                            )
                        ),
                        Node::positional(
                            Node::add(
                                Node::modulo(Node::integer(u8"1"), Node::integer(u8"2")),
                                Node::integer(u8"3")
                            )
                        ),
                    }
                )
            ),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"binary_precedence_add_mult.cow");
}

TEST(Parse_And_Build, binary_prefix_interaction)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive_with_arguments(
                u8"d",
                Node::group(
                    {
                        Node::positional(
                            Node::add(Node::integer(u8"1"), Node::unary_plus(Node::integer(u8"2")))
                        ),
                        Node::positional(
                            Node::add(Node::integer(u8"1"), Node::unary_minus(Node::integer(u8"2")))
                        ),
                        Node::positional(
                            Node::add(Node::integer(u8"1"), Node::logical_not(Node::integer(u8"0")))
                        ),
                        Node::positional(
                            Node::add(Node::integer(u8"1"), Node::bitwise_not(Node::integer(u8"0")))
                        ),
                        Node::positional(
                            Node::add(Node::unary_minus(Node::integer(u8"1")), Node::integer(u8"2"))
                        ),
                        Node::positional(
                            Node::add(Node::unary_plus(Node::integer(u8"1")), Node::integer(u8"2"))
                        ),
                    }
                )
            ),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"binary_prefix_interaction.cow");
}

TEST(Parse_And_Build, file_ends_in_brace)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected {
        { Node::directive_with_content(u8"d", Node::block({})) },
        &memory,
    };

    std::optional<Actual_Document> parsed
        = parse_and_build_file(u8"file_ends_in_brace.cow", &memory);
    ASSERT_TRUE(parsed);
    const auto actual = parsed->to_expected(); // NOLINT(bugprone-unchecked-optional-access)
    ASSERT_EQ(expected, actual);
}

TEST(Parse_And_Build, hello_code)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive_with_content(u8"c", Node::block({ Node::text(u8"/* awoo */") })),
            Node::text(u8"\n"),
        },
        &memory,
    };

    std::optional<Actual_Document> parsed = parse_and_build_file(u8"hello_code.cow", &memory);
    ASSERT_TRUE(parsed);
    const auto actual = parsed->to_expected(); // NOLINT(bugprone-unchecked-optional-access)
    ASSERT_EQ(expected, actual);
}

TEST(Parse_And_Build, hello_directive)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive(
                u8"b",
                Node::group(
                    {
                        Node::named(u8"hello", { Node::unquoted_string(u8"world") }),
                        Node::named(u8"x", { Node::integer(u8"0") }),
                    }
                ),
                Node::block({ Node::text(u8"test") })
            ),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"hello_directive.cow");
}

TEST(Parse_And_Build, directive_as_argument)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive_with_arguments(
                u8"a"sv,
                Node::group(
                    { Node::positional({ Node::directive_with_arguments(u8"x", Node::group()) }) }
                )
            ),
            Node::text(u8"\n"),

            Node::directive_with_arguments(
                u8"b"sv,
                Node::group(
                    { Node::positional({ Node::directive(u8"y", Node::group(), Node::block()) }) }
                )
            ),
            Node::text(u8"\n"),

            Node::directive_with_arguments(
                u8"c"sv,
                Node::group(
                    { Node::positional({ Node::directive(u8"z", Node::group(), Node::block()) }) }
                )
            ),
            Node::text(u8"\n"),

            Node::directive_with_arguments(
                u8"d"sv,
                Node::group(
                    { Node::positional({ Node::directive_with_content(u8"w", Node::block()) }) }
                )
            ),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"directive_as_argument.cow");
}

TEST(Parse_And_Build, escape_numeric)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected {
        {
            Node::escape(u8"\\+00A0"),
            Node::text(u8"\n"),
            Node::escape(u8"\\+00a0"),
            Node::text(u8"00\n"),
            Node::directive(
                u8"d",
                Node::group(
                    {
                        Node::positional({ Node::quoted_string({ Node::escape(u8"\\+00A0") }) }),
                    }
                ),
                Node::block({ Node::escape(u8"\\+00A0") })
            ),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"escape_numeric.cow");
}

TEST(Parse_And_Build, arguments_balanced_braces)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive_with_arguments(
                u8"d",
                Node::group(
                    {
                        Node::named(u8"x", { Node::block() }),
                        Node::positional({ Node::block() }),
                    }
                )
            ),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"arguments/balanced_braces.cow");
}

TEST(Parse_Fail, directive_splice_as_argument)
{
    ASSERT_TRUE(run_parse_fail_test(u8"directive_splice_as_argument.cow"));
}

TEST(Parse_Fail, escape_as_argument)
{
    ASSERT_TRUE(run_parse_fail_test(u8"escape_as_argument.cow"));
}

TEST(Parse_Fail, hyphen_arg_name)
{
    ASSERT_TRUE(run_parse_fail_test(u8"hyphen_arg_name.cow"));
}

TEST(Parse_Fail, parenthesized_named_member_missing_value)
{
    ASSERT_TRUE(run_parse_fail_test(u8"parenthesized_named_member_missing_value.cow"));
}

} // namespace
} // namespace cowel
