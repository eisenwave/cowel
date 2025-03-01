#include <iostream>
#include <memory_resource>
#include <vector>

#include <gtest/gtest.h>

#include "mmml/annotated_string.hpp"
#include "mmml/ast.hpp"
#include "mmml/diagnostics.hpp"
#include "mmml/io.hpp"
#include "mmml/io_error.hpp"
#include "mmml/parse.hpp"
#include "mmml/tty.hpp"

namespace mmml {
namespace {

struct Expected_Content;

struct Expected_Argument {
    std::vector<Expected_Content> content;
    std::string_view name = "";

    Expected_Argument(std::string_view name, std::vector<Expected_Content>&& content);

    Expected_Argument(std::vector<Expected_Content>&& content);

    ~Expected_Argument();

    static Expected_Argument from(const ast::Argument& arg, std::string_view source);

    [[nodiscard]]
    bool matches(const ast::Argument& actual, std::string_view source) const;

    friend bool operator==(const Expected_Argument&, const Expected_Argument&) = default;
};

enum struct Expected_Content_Type : Default_Underlying {
    text,
    escape,
    directive,
};

struct Expected_Content {
    Expected_Content_Type type;
    std::string_view name_or_text;
    std::vector<Expected_Argument> arguments {};
    std::vector<Expected_Content> content {};

    static Expected_Content text(std::string_view text)
    {
        return { .type = Expected_Content_Type::text, .name_or_text = text };
    }

    static Expected_Content escape(std::string_view text)
    {
        return { .type = Expected_Content_Type::escape, .name_or_text = text };
    }

    static Expected_Content directive(std::string_view name)
    {
        return { .type = Expected_Content_Type::directive, .name_or_text = name };
    }
    static Expected_Content
    directive(std::string_view name, std::vector<Expected_Argument>&& arguments)
    {
        return { .type = Expected_Content_Type::directive,
                 .name_or_text = name,
                 .arguments = std::move(arguments) };
    }
    static Expected_Content
    directive(std::string_view name, std::vector<Expected_Content>&& content)
    {
        return { .type = Expected_Content_Type::directive,
                 .name_or_text = name,
                 .content = std::move(content) };
    }
    static Expected_Content directive(
        std::string_view name,
        std::vector<Expected_Argument>&& arguments,
        std::vector<Expected_Content>&& content
    )
    {
        return { .type = Expected_Content_Type::directive,
                 .name_or_text = name,
                 .arguments = std::move(arguments),
                 .content = std::move(content) };
    }

    static Expected_Content from(const ast::Content& actual, std::string_view source)
    {
        if (const auto* const d = get_if<ast::Directive>(&actual)) {
            return from(*d, source);
        }
        if (const auto* const e = get_if<ast::Escaped>(&actual)) {
            return escape(e->get_text(source));
        }
        const std::string_view result = get<ast::Text>(actual).get_text(source);
        return text(result);
    }

    static Expected_Content from(const ast::Directive& actual, std::string_view source)
    {
        std::vector<Expected_Argument> arguments;
        arguments.reserve(actual.get_arguments().size());
        for (const ast::Argument& arg : actual.get_arguments()) {
            arguments.push_back(Expected_Argument::from(arg, source));
        }

        std::vector<Expected_Content> content;
        content.reserve(actual.get_content().size());
        for (const ast::Content& c : actual.get_content()) {
            content.push_back(from(c, source));
        }

        return directive(actual.get_name(source), std::move(arguments), std::move(content));
    }

    [[nodiscard]]
    bool matches(const ast::Content& other, std::string_view source) const;

    [[nodiscard]]
    bool matches(const ast::Directive& actual, std::string_view source) const;

    friend bool operator==(const Expected_Content&, const Expected_Content&) = default;
};

Expected_Argument::Expected_Argument(std::string_view name, std::vector<Expected_Content>&& content)
    : content { std::move(content) }
    , name { name }
{
}

Expected_Argument::Expected_Argument(std::vector<Expected_Content>&& content)
    : content { std::move(content) }
{
}

Expected_Argument::~Expected_Argument() = default;

Expected_Argument Expected_Argument::from(const ast::Argument& arg, std::string_view source)
{
    std::vector<Expected_Content> content;
    for (const ast::Content& c : arg.get_content()) {
        content.push_back(Expected_Content::from(c, source));
    }
    if (arg.has_name()) {
        return Expected_Argument { arg.get_name(source), std::move(content) };
    }
    return Expected_Argument(std::move(content));
}

template <typename T, typename Alloc>
std::ostream& operator<<(std::ostream& os, const std::vector<T, Alloc>& vec)
{
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0)
            os << ", ";
        os << vec[i]; // Uses operator<< of T
    }
    return os;
}

std::ostream& operator<<(std::ostream& out, const Expected_Argument& arg)
{
    if (!arg.name.empty()) {
        out << "Arg(" << arg.name << "){" << arg.content << '}';
    }
    return out;
}

void write_special_escaped(std::ostream& out, char c)
{
    if (c == '\n') {
        out << "\\n";
    }
    else if (c == '\r') {
        out << "\\r";
    }
    else if (c == '\t') {
        out << "\\t";
    }
    else {
        out << c;
    }
}

std::ostream& operator<<(std::ostream& out, const Expected_Content& content)
{
    switch (content.type) {
    case Expected_Content_Type::directive: {
        out << '\\' << content.name_or_text << '[' << content.arguments << "]{" << content.content
            << '}';
        break;
    }

    case Expected_Content_Type::text: {
        out << "Text(";
        for (char c : content.name_or_text) {
            write_special_escaped(out, c);
        }
        out << ')';
        break;
    }

    case Expected_Content_Type::escape: {
        out << "Escape(" << content.name_or_text << ')';
        break;
    }
    }
    return out;
}

struct [[nodiscard]] Actual_Document {
    std::pmr::vector<char> source;
    std::pmr::vector<ast::Content> content;

    [[nodiscard]]
    std::string_view source_string() const
    {
        return { source.data(), source.size() };
    }

    std::pmr::vector<Expected_Content> to_expected() const
    {
        std::pmr::vector<Expected_Content> result { content.get_allocator() };
        result.reserve(content.size());
        for (const auto& actual : content) {
            result.push_back(Expected_Content::from(actual, source_string()));
        }
        return result;
    }
};

struct Parsed_File {
    std::pmr::vector<char> source;
    std::pmr::vector<AST_Instruction> instructions;

    [[nodiscard]]
    std::string_view get_source_string() const
    {
        return { source.data(), source.size() };
    }
};

[[nodiscard]]
std::optional<Parsed_File> parse_file(std::string_view file, std::pmr::memory_resource* memory)
{
    std::pmr::string full_file { "test/", memory };
    full_file += file;

    Parsed_File result { .source = std::pmr::vector<char> { memory },
                         .instructions = std::pmr::vector<AST_Instruction> { memory } };

    if (Result<void, mmml::IO_Error_Code> r = file_to_bytes(result.source, full_file); !r) {
        Annotated_String out { memory };
        print_io_error(out, full_file, r.error());
        print_code_string(std::cout, out, is_stdout_tty);
        return {};
    }

    parse(result.instructions, result.get_source_string());
    return result;
}

[[nodiscard]]
std::optional<Actual_Document>
parse_and_build_file(std::string_view file, std::pmr::memory_resource* memory)
{
    std::optional<Parsed_File> parsed = parse_file(file, memory);
    if (!parsed) {
        return {};
    }
    const std::string_view source_string = parsed->get_source_string();

    std::pmr::vector<ast::Content> content = build_ast(source_string, parsed->instructions, memory);
    return Actual_Document { std::move(parsed->source), std::move(content) };
}

void append_instruction(Annotated_String& out, const AST_Instruction& ins)
{
    out.append(ast_instruction_type_name(ins.type), Annotation_Type::diagnostic_tag);
    if (ast_instruction_type_has_operand(ins.type)) {
        out.append(' ');
        out.append_integer(ins.n);
    }
    else if (ins.n != 0) {
        out.append(' ');
        out.append_integer(ins.n, Annotation_Type::diagnostic_error_text);
    }
}

void dump_instructions(Annotated_String& out, std::span<const AST_Instruction> instructions)
{
    for (const auto& i : instructions) {
        out.append("    ");
        append_instruction(out, i);
        out.append('\n');
    }
}

bool run_parse_test(std::string_view file, std::span<const AST_Instruction> expected)
{
    std::pmr::monotonic_buffer_resource memory;
    std::optional<Parsed_File> actual = parse_file(file, &memory);
    if (!actual) {
        Annotated_String error;
        error.append(
            "Test failed because file couldn't be loaded and parsed.\n",
            Annotation_Type::diagnostic_error_text
        );
        print_code_string(std::cout, error, is_stdout_tty);
        return false;
    }
    if (!std::ranges::equal(expected, actual->instructions)) {
        Annotated_String error;
        error.append(
            "Test failed because expected parser output isn't matched.\n",
            Annotation_Type::diagnostic_error_text
        );
        error.append("Expected:\n", Annotation_Type::diagnostic_text);
        dump_instructions(error, expected);
        error.append("Actual:\n", Annotation_Type::diagnostic_text);
        dump_instructions(error, actual->instructions);
        print_code_string(std::cout, error, is_stdout_tty);
        return false;
    }
    return true;
}

#define MMML_PARSE_AND_BUILD_BOILERPLATE(...)                                                      \
    std::optional<Actual_Document> parsed = parse_and_build_file(__VA_ARGS__, &memory);            \
    ASSERT_TRUE(parsed);                                                                           \
    const auto actual = parsed->to_expected();                                                     \
    ASSERT_EQ(expected, actual)

TEST(Parse, empty)
{
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 0 },
        { AST_Instruction_Type::pop_document, 0 },
    };
    ASSERT_TRUE(run_parse_test("empty.mmml", expected));
}

TEST(Parse_And_Build, empty)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const std::pmr::vector<Expected_Content> expected { &memory };

    MMML_PARSE_AND_BUILD_BOILERPLATE("empty.mmml");
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
    ASSERT_TRUE(run_parse_test("hello_code.mmml", expected));
}

TEST(Parse_And_Build, hello_code)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const std::pmr::vector<Expected_Content> expected {
        { Expected_Content::directive("c", { Expected_Content::text("/* awoo */") }),
          Expected_Content::text("\n") },
        &memory
    };

    MMML_PARSE_AND_BUILD_BOILERPLATE("hello_code.mmml");
}

TEST(Parse, hello_directive)
{
    // clang-format off
    static constexpr AST_Instruction expected[] {
        { AST_Instruction_Type::push_document, 2 },
        { AST_Instruction_Type::push_directive, 2 },
        { AST_Instruction_Type::push_arguments, 2 },

        { AST_Instruction_Type::push_argument, 1 },
        { AST_Instruction_Type::argument_name, 5 }, // "hello"
        { AST_Instruction_Type::skip, 2 },          // " ="
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::text, 5 },          // "world"
        { AST_Instruction_Type::pop_argument },

        { AST_Instruction_Type::skip, 1 },          // ,

        { AST_Instruction_Type::push_argument, 1 },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::argument_name, 1 }, // "x"
        { AST_Instruction_Type::skip, 2 },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::text, 1 },          // "0"
        { AST_Instruction_Type::pop_argument },

        { AST_Instruction_Type::pop_arguments },
        { AST_Instruction_Type::push_block, 1 },    // {
        { AST_Instruction_Type::text, 4 },          // "test"
        { AST_Instruction_Type::pop_block },     // }
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },          // \n
        { AST_Instruction_Type::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test("hello_directive.mmml", expected));
}

TEST(Parse_And_Build, hello_directive)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const Expected_Argument arg0 { "hello", { Expected_Content::text("world") } };
    static const Expected_Argument arg1 { "x", { Expected_Content::text("0") } };

    static const std::pmr::vector<Expected_Content> expected {
        { Expected_Content::directive("b", { arg0, arg1 }, { Expected_Content::text("test") }),
          Expected_Content::text("\n") },
        &memory
    };

    MMML_PARSE_AND_BUILD_BOILERPLATE("hello_directive.mmml");
}

TEST(Parse_And_Build, directive_arg_balanced_braces)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const Expected_Argument arg0 { "x", { Expected_Content::text("{}") } };
    static const Expected_Argument arg1 { { Expected_Content::text("{}") } };

    static const std::pmr::vector<Expected_Content> expected {
        { Expected_Content::directive("d", { arg0, arg1 }), Expected_Content::text("\n") }, &memory
    };

    MMML_PARSE_AND_BUILD_BOILERPLATE("directive_arg_balanced_braces.mmml");
}

TEST(Parse_And_Build, directive_arg_unbalanced_brace)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const std::pmr::vector<Expected_Content> expected {
        { Expected_Content::directive("d"), Expected_Content::text("[}]\n") }, &memory
    };

    MMML_PARSE_AND_BUILD_BOILERPLATE("directive_arg_unbalanced_brace.mmml");
}

TEST(Parse_And_Build, directive_arg_unbalanced_brace_2)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const std::pmr::vector<Expected_Content> expected {
        { Expected_Content::directive(
              "x", { Expected_Content::directive("y"), Expected_Content::text("[") }
          ),
          Expected_Content::text("]\n") },
        &memory
    };

    MMML_PARSE_AND_BUILD_BOILERPLATE("directive_arg_unbalanced_brace_2.mmml");
}

TEST(Parse_And_Build, directive_arg_unbalanced_through_brace_escape)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const std::pmr::vector<Expected_Content> expected {
        { Expected_Content::directive("d"), Expected_Content::text("["),
          Expected_Content::escape("\\{"), Expected_Content::text("}]\n") },
        &memory
    };

    MMML_PARSE_AND_BUILD_BOILERPLATE("directive_arg_unbalanced_through_brace_escape.mmml");
}

TEST(Parse_And_Build, directive_brace_escape)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const std::pmr::vector<Expected_Content> expected {
        { Expected_Content::directive("d"), Expected_Content::text("["),
          Expected_Content::escape("\\{"), Expected_Content::text("}]\n") },
        &memory
    };

    MMML_PARSE_AND_BUILD_BOILERPLATE("directive_arg_unbalanced_through_brace_escape.mmml");
}

} // namespace
} // namespace mmml
