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

#include "cowel/ast.hpp"
#include "cowel/diagnostic_highlight.hpp"
#include "cowel/fwd.hpp"
#include "cowel/parse.hpp"
#include "cowel/print.hpp"
#include "diff.hpp"

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
    unquoted_string,
    text,
    escape,
    comment,
    directive,
    string,
    block,
    group,
    named_argument,
    positional_argument,
    ellipsis,
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
    static Node comment(std::u8string_view text)
    {
        COWEL_ASSERT(text.starts_with(u8"\\:"));
        return { .kind = Node_Kind::comment, .name_or_text = text };
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
    static Node string(std::vector<Node>&& elements)
    {
        return {
            .kind = Node_Kind::string,
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
    static Node named(std::u8string_view name, std::vector<Node>&& children)
    {
        return {
            .kind = Node_Kind::named_argument,
            .name_or_text = name,
            .children = std::move(children),
        };
    }

    [[nodiscard]]
    static Node positional(std::vector<Node>&& children)
    {
        return {
            .kind = Node_Kind::positional_argument,
            .children = std::move(children),
        };
    }

    [[nodiscard]]
    static Node ellipsis()
    {
        return { .kind = Node_Kind::ellipsis };
    }

    [[nodiscard]]
    static Node from(const ast::Markup_Element& actual)
    {
        if (const auto* const d = get_if<ast::Directive>(&actual)) {
            return from(*d);
        }
        return from(std::get<ast::Primary>(actual));
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
            return comment(actual.get_source());
        }

        case ast::Primary_Kind::quoted_string:
        case ast::Primary_Kind::block: {
            std::vector<Node> children;
            children.reserve(actual.get_elements_size());
            for (const ast::Markup_Element& c : actual.get_elements()) {
                children.push_back(from(c));
            }
            return kind == ast::Primary_Kind::quoted_string ? string(std::move(children))
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
    static Node from(const ast::Group_Member& arg)
    {
        std::vector<Node> children;
        if (arg.has_value()) {
            children.push_back(from(arg.get_value()));
        }

        switch (arg.get_kind()) {
        case ast::Member_Kind::ellipsis: {
            COWEL_ASSERT(children.empty());
            return ellipsis();
        }
        case ast::Member_Kind::named: {
            return Node::named(arg.get_name(), std::move(children));
        }
        case ast::Member_Kind::positional: {
            return positional(std::move(children));
        }
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid argument type.");
    }

    [[nodiscard]]
    static Node from(const ast::Member_Value& arg)
    {
        const auto visitor = [&](const auto& v) -> Node { return from(v); };
        return std::visit(visitor, arg);
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
constexpr Static_String8<2> special_escaped(char8_t c)
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

    case Node_Kind::unquoted_string: {
        return out << "UnqString(" << node.name_or_text << ')';
    }

    case Node_Kind::text: {
        out << "Text(";
        for (const char8_t c : node.name_or_text) {
            out << special_escaped(c).as_string();
        }
        out << ')';
        break;
    }

    case Node_Kind::comment: {
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

    case Node_Kind::string: {
        return out << "String(" << node.children << ')';
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
    std::pmr::vector<AST_Instruction> instructions;

    [[nodiscard]]
    std::u8string_view get_source_string() const
    {
        return { source.data(), source.size() };
    }
};

[[nodiscard]]
std::optional<Parsed_File> parse_file(std::u8string_view file, std::pmr::memory_resource* memory)
{
    std::pmr::u8string full_file { u8"test/syntax/", memory };
    full_file += file;

    Parsed_File result { .source = std::pmr::vector<char8_t> { memory },
                         .instructions = std::pmr::vector<AST_Instruction> { memory } };

    const Result<void, cowel::IO_Error_Code> r = load_utf8_file(result.source, full_file);
    if (!r) {
        Diagnostic_String out { memory };
        print_io_error(out, full_file, r.error());
        print_code_string_stdout(out);
        return {};
    }

    const std::convertible_to<Parse_Error_Consumer> auto consumer
        = [&](std::u8string_view /* id */, const Source_Span& location, Char_Sequence8 message) {
              Diagnostic_String out { memory };
              print_file_position(out, file, location);
              out.append(u8' ');
              append_char_sequence(out, message, Diagnostic_Highlight::text);
              out.append(u8'\n');
              print_code_string_stdout(out);
          };
    if (!parse(result.instructions, result.get_source_string(), consumer)) {
        return {};
    }
    return result;
}

[[nodiscard]]
std::optional<Actual_Document>
parse_and_build_file(std::u8string_view file, std::pmr::memory_resource* memory)
{
    std::optional<Parsed_File> parsed = parse_file(file, memory);
    if (!parsed) {
        return {};
    }
    const std::u8string_view source_string = parsed->get_source_string();

    ast::Pmr_Vector<ast::Markup_Element> content
        = build_ast(source_string, File_Id::main, parsed->instructions, memory);
    return Actual_Document { std::move(parsed->source), std::move(content) };
}

void append_instruction(Diagnostic_String& out, const AST_Instruction& ins)
{
    out.append(ast_instruction_type_name(ins.type), Diagnostic_Highlight::tag);
    if (ast_instruction_type_has_operand(ins.type)) {
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
    std::span<const AST_Instruction> instructions,
    std::u8string_view indent = {}
)
{
    for (const auto& i : instructions) {
        out.append(indent);
        append_instruction(out, i);
        out.append(u8'\n');
    }
}

bool run_parse_test(std::u8string_view file, std::span<const AST_Instruction> expected)
{
    for (const auto& e : expected) {
        COWEL_ASSERT(ast_instruction_type_has_operand(e.type) || e.n == 0);
    }

    constexpr std::u8string_view indent = u8"    ";

    std::pmr::monotonic_buffer_resource memory;
    std::optional<Parsed_File> actual = parse_file(file, &memory);
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

// NOLINTBEGIN(bugprone-unchecked-optional-access)
#define COWEL_PARSE_AND_BUILD_BOILERPLATE(...)                                                     \
    std::optional<Actual_Document> parsed = parse_and_build_file(__VA_ARGS__, &memory);            \
    ASSERT_TRUE(parsed);                                                                           \
    const auto actual = parsed->to_expected();                                                     \
    ASSERT_EQ(expected, actual)
// NOLINTEND(bugprone-unchecked-optional-access)

TEST(Parse, empty)
{
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 0 },
        { AST_Instruction_Type::pop_document, 0 },
    };
    ASSERT_TRUE(run_parse_test(u8"empty.cow", expected));
}

TEST(Parse_And_Build, empty)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected { &memory };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"empty.cow");
}

TEST(Parse, directive_brace_escape_2)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 2 },
        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_block, 4 },
        { AST_Instruction_Type::escape, 2 },
        { AST_Instruction_Type::text, 4 },
        { AST_Instruction_Type::escape, 2 },
        { AST_Instruction_Type::text, 1 },
        { AST_Instruction_Type::pop_block },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },
        { AST_Instruction_Type::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"directive_brace_escape_2.cow", expected));
}

TEST(Parse, directive_multiline)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 2 },
        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::skip, 3 },
        { AST_Instruction_Type::push_named_member },
        { AST_Instruction_Type::member_name, 2 },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::member_equal },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::unquoted_string, 1 },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::pop_named_member },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },
        { AST_Instruction_Type::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"directive_multiline.cow", expected));
}

TEST(Parse, directive_multiline_trailing_comma)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 2 },
        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::skip, 3 },
        { AST_Instruction_Type::push_named_member },
        { AST_Instruction_Type::member_name, 2 },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::member_equal },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::unquoted_string, 1 },
        { AST_Instruction_Type::pop_named_member },
        { AST_Instruction_Type::member_comma },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },
        { AST_Instruction_Type::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"directive_multiline_trailing_comma.cow", expected));
}

TEST(Parse, comments)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 9 },
        { AST_Instruction_Type::comment, 10 },
        { AST_Instruction_Type::comment, 7 },
        { AST_Instruction_Type::comment, 10 },
        { AST_Instruction_Type::push_directive, 4 },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::comment, 4 },
        { AST_Instruction_Type::comment, 21 },
        { AST_Instruction_Type::text, 9 },
        { AST_Instruction_Type::comment, 11 },
        { AST_Instruction_Type::comment, 12 },
        { AST_Instruction_Type::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"comments.cow", expected));
}

TEST(Parse, arguments_comments_1)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 2 },
        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 0 },
        { AST_Instruction_Type::skip, 21 },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },
        { AST_Instruction_Type::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"arguments/comments_1.cow", expected));
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

TEST(Parse, arguments_comments_2)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 2 },
        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 2 },

        { AST_Instruction_Type::skip, 13 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::unquoted_string, 4 },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 13 },
        { AST_Instruction_Type::push_named_member },
        { AST_Instruction_Type::member_name, 5 },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::member_equal },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::unquoted_string, 3 },
        { AST_Instruction_Type::pop_named_member },
        { AST_Instruction_Type::member_comma },
        { AST_Instruction_Type::skip, 1 },

        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },
        { AST_Instruction_Type::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"arguments/comments_2.cow", expected));
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
                        Node::named(u8"named", { Node::unquoted_string(u8"arg") }),
                    }
                )
            ),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"arguments/comments_2.cow");
}

TEST(Parse, arguments_ellipsis)
{
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 2 },
        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::push_ellipsis_argument },
        { AST_Instruction_Type::ellipsis, 3 },
        { AST_Instruction_Type::pop_ellipsis_argument },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },
        { AST_Instruction_Type::pop_document },
    };
    ASSERT_TRUE(run_parse_test(u8"arguments/ellipsis.cow", expected));
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

TEST(Parse, group_1)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 2 },
        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 2 },

        { AST_Instruction_Type::push_positional_member }, // (x)
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::unquoted_string, 1 },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::push_positional_member }, // ()
        { AST_Instruction_Type::push_group, 0 },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_positional_member },

        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },
        { AST_Instruction_Type::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"arguments/group_1.cow", expected));
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
                        Node::positional(
                            { Node::group(
                                { Node::positional(
                                    {
                                        Node::unquoted_string(u8"x"),
                                    }
                                ) }
                            ) }
                        ),
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

TEST(Parse, group_2)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 2 },
        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 1 },

        { AST_Instruction_Type::push_named_member }, // n =  (x, y)
        { AST_Instruction_Type::member_name, 1 },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::member_equal },
        { AST_Instruction_Type::skip, 1 },
        
        { AST_Instruction_Type::push_group, 2 }, // (x, y)
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::unquoted_string, 1 },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::unquoted_string, 1 },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::pop_group },

        { AST_Instruction_Type::pop_named_member },

        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },
        { AST_Instruction_Type::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"arguments/group_2.cow", expected));
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
                        { Node::group(
                            {
                                Node::positional({ Node::unquoted_string(u8"x") }),
                                Node::positional({ Node::unquoted_string(u8"y") }),
                            }
                        ) }
                    ) }
                )
            ),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"arguments/group_2.cow");
}

TEST(Parse, group_3)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 2 },
        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::push_group, 0 },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },
        { AST_Instruction_Type::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"arguments/group_3.cow", expected));
}

TEST(Parse_And_Build, group_3)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive_with_arguments(
                u8"d",
                Node::group(
                    { Node::positional(
                        { Node::group(
                            { Node::positional(
                                { Node::group({ Node::positional({ Node::group({}) }) }) }
                            ) }
                        ) }
                    ) }
                )
            ),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"arguments/group_3.cow");
}

TEST(Parse, unquoted)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 10 },

        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::keyword_null, 4 },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::keyword_true, 4 },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::keyword_false, 5 },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::unquoted_string, 4 },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::unquoted_string, 6 },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"arguments/unquoted.cow", expected));
}

TEST(Parse, string)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 8 },

        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::push_quoted_string, 0 },
        { AST_Instruction_Type::pop_quoted_string },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::push_quoted_string, 1 },
        { AST_Instruction_Type::text, 4 },
        { AST_Instruction_Type::pop_quoted_string },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::push_quoted_string, 1 },
        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::pop_quoted_string },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::push_quoted_string, 3 },
        { AST_Instruction_Type::escape, 2 },
        { AST_Instruction_Type::text, 4 },
        { AST_Instruction_Type::escape, 2 },
        { AST_Instruction_Type::pop_quoted_string },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"arguments/string.cow", expected));
}

TEST(Parse, block)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 8 },

        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::push_block, 0 },
        { AST_Instruction_Type::pop_block },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::push_block, 1 },
        { AST_Instruction_Type::text, 4 },
        { AST_Instruction_Type::pop_block },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::push_block, 1 },
        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::pop_block },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::push_block, 5 },
        { AST_Instruction_Type::text, 1 },
        { AST_Instruction_Type::escape, 2 },
        { AST_Instruction_Type::text, 4 },
        { AST_Instruction_Type::escape, 2 },
        { AST_Instruction_Type::text, 1 },
        { AST_Instruction_Type::pop_block },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"arguments/block.cow", expected));
}

TEST(Parse, illegal_backslash)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 1 },
        { AST_Instruction_Type::text, 3 },
        { AST_Instruction_Type::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"illegal_backslash.cow", expected));
}

TEST(Parse, integers)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 2 },
        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 4 },

        { AST_Instruction_Type::push_positional_member }, // 0
        { AST_Instruction_Type::decimal_int_literal, 1 },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },
        { AST_Instruction_Type::skip, 1 },

        { AST_Instruction_Type::push_positional_member }, // 123
        { AST_Instruction_Type::decimal_int_literal, 3 },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },
        { AST_Instruction_Type::skip, 1 },

        { AST_Instruction_Type::push_positional_member }, // -123
        { AST_Instruction_Type::decimal_int_literal, 4 },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },
        { AST_Instruction_Type::skip, 1 },

        { AST_Instruction_Type::push_positional_member }, // 0xff
        { AST_Instruction_Type::hexadecimal_int_literal, 4 },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },
        { AST_Instruction_Type::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"integers.cow", expected));
}

TEST(Parse, literals)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 2 },
        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 7 },

        { AST_Instruction_Type::skip, 3 }, // unit
        { AST_Instruction_Type::push_positional_member }, 
        { AST_Instruction_Type::keyword_unit, 4 }, 
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 3 }, // null
        { AST_Instruction_Type::push_positional_member }, 
        { AST_Instruction_Type::keyword_null, 4 }, 
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 3 }, // true
        { AST_Instruction_Type::push_positional_member }, 
        { AST_Instruction_Type::keyword_true, 4 }, 
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 3 }, // false
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::keyword_false, 5 }, 
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 3 }, // 0
        { AST_Instruction_Type::push_positional_member }, 
        { AST_Instruction_Type::decimal_int_literal, 1 }, 
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 3 }, // ""
        { AST_Instruction_Type::push_positional_member }, 
        { AST_Instruction_Type::push_quoted_string, 0 }, 
        { AST_Instruction_Type::pop_quoted_string }, 
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 3 }, // awoo
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::unquoted_string, 4 }, 
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },
        { AST_Instruction_Type::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"literals.cow", expected));
}

TEST(Parse, floats)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 2 },
        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 15 },

        { AST_Instruction_Type::skip, 3 },
        { AST_Instruction_Type::push_positional_member }, 
        { AST_Instruction_Type::float_literal, 2 }, 
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 3 },
        { AST_Instruction_Type::push_positional_member }, 
        { AST_Instruction_Type::float_literal, 2 }, 
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 3 },
        { AST_Instruction_Type::push_positional_member }, 
        { AST_Instruction_Type::float_literal, 3 }, 
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 3 },
        { AST_Instruction_Type::push_positional_member }, 
        { AST_Instruction_Type::float_literal, 3 }, 
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 3 },
        { AST_Instruction_Type::push_positional_member }, 
        { AST_Instruction_Type::float_literal, 4 }, 
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 3 },
        { AST_Instruction_Type::push_positional_member }, 
        { AST_Instruction_Type::float_literal, 4 }, 
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 3 },
        { AST_Instruction_Type::push_positional_member }, 
        { AST_Instruction_Type::float_literal, 5 }, 
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 3 },
        { AST_Instruction_Type::push_positional_member }, 
        { AST_Instruction_Type::float_literal, 3 }, 
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 3 },
        { AST_Instruction_Type::push_positional_member }, 
        { AST_Instruction_Type::float_literal, 4 }, 
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 3 },
        { AST_Instruction_Type::push_positional_member }, 
        { AST_Instruction_Type::float_literal, 4 }, 
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 3 },
        { AST_Instruction_Type::push_positional_member }, 
        { AST_Instruction_Type::float_literal, 3 }, 
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 3 },
        { AST_Instruction_Type::push_positional_member }, 
        { AST_Instruction_Type::float_literal, 4 }, 
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 3 },
        { AST_Instruction_Type::push_positional_member }, 
        { AST_Instruction_Type::float_literal, 4 }, 
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 3 },
        { AST_Instruction_Type::push_positional_member }, 
        { AST_Instruction_Type::float_literal, 11 }, 
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 3 },
        { AST_Instruction_Type::push_positional_member }, 
        { AST_Instruction_Type::float_literal, 12 }, 
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },
        { AST_Instruction_Type::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"floats.cow", expected));
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
                        Node::positional({ Node::floating_point(u8"0."sv) }),
                        Node::positional({ Node::floating_point(u8".0"sv) }),
                        Node::positional({ Node::floating_point(u8"0.0"sv) }),
                        Node::positional({ Node::floating_point(u8"0e0"sv) }),
                        Node::positional({ Node::floating_point(u8"0.e0"sv) }),
                        Node::positional({ Node::floating_point(u8".0e0"sv) }),
                        Node::positional({ Node::floating_point(u8"0.0e0"sv) }),
                        Node::positional({ Node::floating_point(u8"0e0"sv) }),
                        Node::positional({ Node::floating_point(u8"0e+0"sv) }),
                        Node::positional({ Node::floating_point(u8"0e-0"sv) }),
                        Node::positional({ Node::floating_point(u8"0E0"sv) }),
                        Node::positional({ Node::floating_point(u8"0E+0"sv) }),
                        Node::positional({ Node::floating_point(u8"0E-0"sv) }),
                        Node::positional({ Node::floating_point(u8"123.456e789"sv) }),
                        Node::positional({ Node::floating_point(u8"-123.456e789"sv) }),
                    }
                )
            ),
            Node::text(u8"\n"),
        },
        &memory,
    };

    for (const auto& arg : expected.front().children.front().children) {
        COWEL_ASSERT(arg.kind == Node_Kind::positional_argument);
        const std::u8string_view literal = arg.children.front().name_or_text;
        const std::optional<double> d = from_characters_or_inf<double>(literal);
        ASSERT_TRUE(d);
    }

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"floats.cow");
}

TEST(Parse, directive_names)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 16 },

        { AST_Instruction_Type::push_directive, 2 }, // \x
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::push_directive, 4 }, // \x_y
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::escape, 2 }, // \-x
        { AST_Instruction_Type::text, 2 },

        { AST_Instruction_Type::push_directive, 2 }, // \x-
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 2 },

        { AST_Instruction_Type::push_directive, 3 }, // \_x
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::push_directive, 2 }, // \x.y
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 3 },

        { AST_Instruction_Type::push_directive, 3 }, // \xy
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::push_directive, 4 }, // \xy0
        { AST_Instruction_Type::pop_directive },

        { AST_Instruction_Type::text, 6 }, // \0xy
        
        { AST_Instruction_Type::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"directive_names.cow", expected));
}

TEST(Parse, escape_lf)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 3 },
        { AST_Instruction_Type::escape, 2 },
        { AST_Instruction_Type::text, 5 },
        { AST_Instruction_Type::escape, 2 },
        { AST_Instruction_Type::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"escape_lf.cow", expected));
}

TEST(Parse, escape_crlf)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 3 },
        { AST_Instruction_Type::escape, 3 },
        { AST_Instruction_Type::text, 5 },
        { AST_Instruction_Type::escape, 3 },
        { AST_Instruction_Type::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"escape_crlf.cow", expected));
}

TEST(Parse, hello_code)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 2 },
        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_block, 1 },
        { AST_Instruction_Type::text, 10 },
        { AST_Instruction_Type::pop_block },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },
        { AST_Instruction_Type::pop_document, 0 },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"hello_code.cow", expected));
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

TEST(Parse, hello_directive)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 2 },
        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_group, 2 },

        { AST_Instruction_Type::push_named_member },
        { AST_Instruction_Type::member_name, 5 }, // "hello"
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::member_equal },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::unquoted_string, 5 },          // "world"
        { AST_Instruction_Type::pop_named_member },

        { AST_Instruction_Type::member_comma },

        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::push_named_member },
        { AST_Instruction_Type::member_name, 1 }, // "x"
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::member_equal },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::decimal_int_literal, 1 },          // "0"
        { AST_Instruction_Type::pop_named_member },

        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::push_block, 1 },    // {
        { AST_Instruction_Type::text, 4 },          // "test"
        { AST_Instruction_Type::pop_block },        // }
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },          // \n
        { AST_Instruction_Type::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"hello_directive.cow", expected));
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

TEST(Parse, directive_as_argument)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 8 },
        
        { AST_Instruction_Type::push_directive, 2 }, // \a(x())
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::push_directive, 1 },
        { AST_Instruction_Type::push_group, 0 },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 }, // \n

        { AST_Instruction_Type::push_directive, 2 }, // \b(x(){})
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::push_directive, 1 },
        { AST_Instruction_Type::push_group, 0 },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::push_block, 0 },
        { AST_Instruction_Type::pop_block },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 }, // \n

        { AST_Instruction_Type::push_directive, 2 }, // \c(x () {})
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::push_directive, 1 },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::push_group, 0 },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::push_block, 0 },
        { AST_Instruction_Type::pop_block },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 }, // \n

        { AST_Instruction_Type::push_directive, 2 }, // \d(x{})
        { AST_Instruction_Type::push_group, 1 },
        { AST_Instruction_Type::push_positional_member },
        { AST_Instruction_Type::push_directive, 1 },
        { AST_Instruction_Type::push_block, 0 },
        { AST_Instruction_Type::pop_block },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::pop_positional_member },
        { AST_Instruction_Type::pop_group },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 }, // \n

        { AST_Instruction_Type::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"directive_as_argument.cow", expected));
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

} // namespace
} // namespace cowel
