#include <algorithm>
#include <cstddef>
#include <iostream>
#include <memory_resource>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "cowel/util/annotated_string.hpp"
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
    text,
    escape,
    directive,
    group,
    named_argument,
    positional_argument,
    ellipsis,
};

struct Node {
    Node_Kind type;
    std::u8string_view name_or_text {};
    std::vector<Node> arguments {};
    std::vector<Node> children {};

    [[nodiscard]]
    static Node text(std::u8string_view text)
    {
        return { .type = Node_Kind::text, .name_or_text = text };
    }

    [[nodiscard]]
    static Node escape(std::u8string_view text)
    {
        return { .type = Node_Kind::escape, .name_or_text = text };
    }

    [[nodiscard]]
    static Node directive(std::u8string_view name)
    {
        return { .type = Node_Kind::directive, .name_or_text = name };
    }
    [[nodiscard]]
    static Node directive_with_arguments(std::u8string_view name, std::vector<Node>&& arguments)
    {
        return {
            .type = Node_Kind::directive,
            .name_or_text = name,
            .arguments = std::move(arguments),
        };
    }
    [[nodiscard]]
    static Node directive_with_content(std::u8string_view name, std::vector<Node>&& content)
    {
        return {
            .type = Node_Kind::directive,
            .name_or_text = name,
            .children = std::move(content),
        };
    }
    [[nodiscard]]
    static Node
    directive(std::u8string_view name, std::vector<Node>&& arguments, std::vector<Node>&& content)
    {
        return {
            .type = Node_Kind::directive,
            .name_or_text = name,
            .arguments = std::move(arguments),
            .children = std::move(content),
        };
    }

    [[nodiscard]]
    static Node group(std::vector<Node>&& arguments)
    {
        return {
            .type = Node_Kind::group,
            .arguments = std::move(arguments),
        };
    }

    [[nodiscard]]
    static Node named(std::u8string_view name, std::vector<Node>&& children)
    {
        return {
            .type = Node_Kind::named_argument,
            .name_or_text = name,
            .children = std::move(children),
        };
    }

    [[nodiscard]]
    static Node positional(std::vector<Node>&& children)
    {
        return {
            .type = Node_Kind::positional_argument,
            .children = std::move(children),
        };
    }

    [[nodiscard]]
    static Node ellipsis()
    {
        return { .type = Node_Kind::ellipsis };
    }

    [[nodiscard]]
    static Node from(const ast::Content& actual)
    {
        if (const auto* const d = get_if<ast::Directive>(&actual)) {
            return from(*d);
        }
        if (const auto* const e = get_if<ast::Escaped>(&actual)) {
            return escape(e->get_source());
        }
        const std::u8string_view result = get<ast::Text>(actual).get_source();
        return text(result);
    }

    [[nodiscard]]
    static Node from(const ast::Directive& actual)
    {
        std::vector<Node> arguments;
        arguments.reserve(actual.get_argument_span().size());
        for (const ast::Group_Member& arg : actual.get_argument_span()) {
            arguments.push_back(from(arg));
        }

        std::vector<Node> content;
        content.reserve(actual.get_content_span().size());
        for (const ast::Content& c : actual.get_content_span()) {
            content.push_back(from(c));
        }

        return directive(actual.get_name(), std::move(arguments), std::move(content));
    }

    [[nodiscard]]
    static Node from(const ast::Group_Member& arg)
    {
        std::vector<Node> children;
        if (arg.has_value()) {
            const auto* const arg_content = std::get_if<ast::Content_Sequence>(&arg.get_value());
            if (arg_content) {
                children.reserve(arg_content->size());
                for (const ast::Content& c : arg_content->get_elements()) {
                    children.push_back(from(c));
                }
            }
            else {
                const auto& group = std::get<ast::Group>(arg.get_value());
                std::vector<Node> group_members;
                group_members.reserve(group.size());
                for (const ast::Group_Member& member : group.get_members()) {
                    group_members.push_back(from(member));
                }
                children.push_back(Node::group(std::move(group_members)));
            }
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
    bool matches(const ast::Content& other, std::u8string_view source) const;

    [[nodiscard]]
    bool matches(const ast::Directive& actual, std::u8string_view source) const;

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

std::ostream& operator<<(std::ostream& out, const Node& content)
{
    switch (content.type) {
    case Node_Kind::directive: {
        out << '\\' << content.name_or_text //
            << '(' << content.arguments << ')' //
            << "{" << content.children << '}';
        break;
    }

    case Node_Kind::text: {
        out << "Text(";
        for (const char8_t c : content.name_or_text) {
            out << special_escaped(c).as_string();
        }
        out << ')';
        break;
    }

    case Node_Kind::escape: {
        out << "Escape(" << content.name_or_text << ')';
        break;
    }

    case Node_Kind::group: {
        out << "Group(" << content.arguments << ')';
        break;
    }

    case Node_Kind::named_argument: {
        out << "NamedArg(" << content.name_or_text << "){" << content.children << '}';
        break;
    }

    case Node_Kind::positional_argument: {
        out << "PosArg" << "{" << content.children << '}';
        break;
    }

    case Node_Kind::ellipsis: {
        out << "...";
        break;
    }
    }
    return out;
}

struct [[nodiscard]] Actual_Document {
    std::pmr::vector<char8_t> source;
    ast::Pmr_Vector<ast::Content> content;

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
    std::pmr::u8string full_file { u8"test/", memory };
    full_file += file;

    Parsed_File result { .source = std::pmr::vector<char8_t> { memory },
                         .instructions = std::pmr::vector<AST_Instruction> { memory } };

    Result<void, cowel::IO_Error_Code> r = load_utf8_file(result.source, full_file);
    if (!r) {
        Diagnostic_String out { memory };
        print_io_error(out, full_file, r.error());
        print_code_string(std::cout, out, is_stdout_tty);
        return {};
    }

    parse(result.instructions, result.get_source_string());
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

    ast::Pmr_Vector<ast::Content> content
        = build_ast(source_string, File_Id {}, parsed->instructions, memory);
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
    constexpr std::u8string_view indent = u8"    ";

    std::pmr::monotonic_buffer_resource memory;
    std::optional<Parsed_File> actual = parse_file(file, &memory);
    if (!actual) {
        Diagnostic_String error;
        error.append(
            u8"Test failed because file couldn't be loaded and parsed.\n",
            Diagnostic_Highlight::error_text
        );
        print_code_string(std::cout, error, is_stdout_tty);
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

        print_code_string(std::cout, error, is_stdout_tty);
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
        { AST_Instruction_Type::push_arguments, 0 },
        { AST_Instruction_Type::skip, 21 },
        { AST_Instruction_Type::pop_arguments },
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
            Node::directive(u8"a"),
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
        { AST_Instruction_Type::push_arguments, 2 },

        { AST_Instruction_Type::push_positional_argument, 1 },
        { AST_Instruction_Type::skip, 13 },
        { AST_Instruction_Type::text, 4 },
        { AST_Instruction_Type::pop_positional_argument },
        { AST_Instruction_Type::argument_comma },

        { AST_Instruction_Type::push_named_argument, 1 },
        { AST_Instruction_Type::skip, 13 },
        { AST_Instruction_Type::argument_name, 5 },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::argument_equal },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::text, 3 },
        { AST_Instruction_Type::pop_named_argument },
        { AST_Instruction_Type::argument_comma },
        { AST_Instruction_Type::skip, 1 },

        { AST_Instruction_Type::pop_arguments },
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
                {
                    Node::positional({ Node::text(u8"text") }),
                    Node::named(u8"named", { Node::text(u8"arg") }),
                }
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
        { AST_Instruction_Type::push_arguments, 1 },
        { AST_Instruction_Type::push_ellipsis_argument, 0 },
        { AST_Instruction_Type::argument_ellipsis, 3 },
        { AST_Instruction_Type::pop_ellipsis_argument },
        { AST_Instruction_Type::pop_arguments },
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
            Node::directive_with_arguments(u8"x", { Node::ellipsis() }),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"arguments/ellipsis.cow");
}

TEST(Parse, arguments_not_ellipsis)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 6 },

        { AST_Instruction_Type::push_directive, 2 }, // \a
        { AST_Instruction_Type::push_arguments, 1 },
        { AST_Instruction_Type::push_positional_argument, 1 },
        { AST_Instruction_Type::text, 7 },
        { AST_Instruction_Type::pop_positional_argument },
        { AST_Instruction_Type::pop_arguments },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::push_directive, 2 }, // \b
        { AST_Instruction_Type::push_arguments, 1 },
        { AST_Instruction_Type::push_named_argument, 1 },
        { AST_Instruction_Type::argument_name, 1 },
        { AST_Instruction_Type::argument_equal },
        { AST_Instruction_Type::text, 3 },
        { AST_Instruction_Type::pop_named_argument },
        { AST_Instruction_Type::pop_arguments },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::push_directive, 2 }, // \c
        { AST_Instruction_Type::push_arguments, 1 },
        { AST_Instruction_Type::push_ellipsis_argument, 1 },
        { AST_Instruction_Type::argument_ellipsis, 3 },
        { AST_Instruction_Type::text, 4 },
        { AST_Instruction_Type::pop_ellipsis_argument },
        { AST_Instruction_Type::pop_arguments },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::pop_document },
    };
    // clang-format on

    ASSERT_TRUE(run_parse_test(u8"arguments/not_ellipsis.cow", expected));
}

TEST(Parse, group_1)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 2 },
        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_arguments, 2 },

        { AST_Instruction_Type::push_positional_argument, 0 }, // (x)
        { AST_Instruction_Type::push_arguments, 1 },
        { AST_Instruction_Type::push_positional_argument, 1 },
        { AST_Instruction_Type::text, 1 },
        { AST_Instruction_Type::pop_positional_argument },
        { AST_Instruction_Type::pop_arguments },
        { AST_Instruction_Type::pop_positional_argument },
        { AST_Instruction_Type::argument_comma },

        { AST_Instruction_Type::push_positional_argument }, // ()
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::push_arguments, 0 },
        { AST_Instruction_Type::pop_arguments },
        { AST_Instruction_Type::pop_positional_argument },

        { AST_Instruction_Type::pop_arguments },
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
                {
                    Node::positional({ Node::group({ Node::positional({ Node::text(u8"x") }) }) }),
                    Node::positional({ Node::group({}) }),
                }
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
        { AST_Instruction_Type::push_arguments, 1 },

        { AST_Instruction_Type::push_named_argument, 0 }, // n =  (x, y)
        { AST_Instruction_Type::argument_name, 1 },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::argument_equal },
        { AST_Instruction_Type::skip, 1 },
        
        { AST_Instruction_Type::push_arguments, 2 }, // (x, y)
        { AST_Instruction_Type::push_positional_argument, 1 },
        { AST_Instruction_Type::text, 1 },
        { AST_Instruction_Type::pop_positional_argument },
        { AST_Instruction_Type::argument_comma },
        { AST_Instruction_Type::push_positional_argument, 1 },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::text, 1 },
        { AST_Instruction_Type::pop_positional_argument },
        { AST_Instruction_Type::pop_arguments },

        { AST_Instruction_Type::pop_named_argument },

        { AST_Instruction_Type::pop_arguments },
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
                { Node::named(
                    u8"n",
                    { Node::group({
                        Node::positional({ Node::text(u8"x") }),
                        Node::positional({ Node::text(u8"y") }),
                    }) }
                ) }
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
        { AST_Instruction_Type::push_arguments, 1 },
        { AST_Instruction_Type::push_positional_argument, 0 },
        { AST_Instruction_Type::push_arguments, 1 },
        { AST_Instruction_Type::push_positional_argument, 0 },
        { AST_Instruction_Type::push_arguments, 1 },
        { AST_Instruction_Type::push_positional_argument, 0 },
        { AST_Instruction_Type::push_arguments, 0 },
        { AST_Instruction_Type::pop_arguments },
        { AST_Instruction_Type::pop_positional_argument },
        { AST_Instruction_Type::pop_arguments },
        { AST_Instruction_Type::pop_positional_argument },
        { AST_Instruction_Type::pop_arguments },
        { AST_Instruction_Type::pop_positional_argument },
        { AST_Instruction_Type::pop_arguments },
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
                { Node::positional({
                    Node::group({
                        Node::positional({
                            Node::group({
                                Node::positional({
                                    Node::group({}),
                                }),
                            }),
                        }),
                    }),
                }) }
            ),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"arguments/group_3.cow");
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

TEST(Parse, directive_names)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 14 },

        { AST_Instruction_Type::push_directive, 2 }, // \x
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::push_directive, 4 }, // \x_y
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::escape, 2 }, // \-x
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
            Node::directive_with_content(u8"c", { Node::text(u8"/* awoo */") }),
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
        { AST_Instruction_Type::push_arguments, 2 },

        { AST_Instruction_Type::push_named_argument, 1 },
        { AST_Instruction_Type::argument_name, 5 }, // "hello"
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::argument_equal },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::text, 5 },          // "world"
        { AST_Instruction_Type::pop_named_argument },

        { AST_Instruction_Type::argument_comma },

        { AST_Instruction_Type::push_named_argument, 1 },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::argument_name, 1 }, // "x"
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::argument_equal },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::text, 1 },          // "0"
        { AST_Instruction_Type::pop_named_argument },

        { AST_Instruction_Type::pop_arguments },
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
    static const auto arg0 = Node::named(u8"hello", { Node::text(u8"world") });
    static const auto arg1 = Node::named(u8"x", { Node::text(u8"0") });

    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive(u8"b", { arg0, arg1 }, { Node::text(u8"test") }),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"hello_directive.cow");
}

TEST(Parse_And_Build, arguments_balanced_braces)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const auto arg0 = Node::named(u8"x", { Node::text(u8"{}") });
    static const auto arg1 = Node::positional({ Node::text(u8"{}") });

    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive_with_arguments(u8"d", { arg0, arg1 }),
            Node::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"arguments/balanced_braces.cow");
}

TEST(Parse_And_Build, arguments_unbalanced_brace_1)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive(u8"d"),
            Node::text(u8"(})\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"arguments/unbalanced_brace_1.cow");
}

TEST(Parse_And_Build, arguments_unbalanced_brace_2)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive_with_content(u8"x", { Node::directive(u8"y"), Node::text(u8"(") }),
            Node::text(u8")\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"arguments/unbalanced_brace_2.cow");
}

TEST(Parse_And_Build, arguments_unbalanced_through_brace_escape)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Node> expected {
        {
            Node::directive(u8"d"),
            Node::text(u8"("),
            Node::escape(u8"\\{"),
            Node::text(u8"})\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"arguments/unbalanced_through_brace_escape.cow");
}

} // namespace
} // namespace cowel
