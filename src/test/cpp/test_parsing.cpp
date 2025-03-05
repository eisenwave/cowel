#include <iostream>
#include <memory_resource>
#include <vector>

#include <gtest/gtest.h>

#include "mmml/util/annotated_string.hpp"
#include "mmml/util/io.hpp"
#include "mmml/util/tty.hpp"

#include "mmml/ast.hpp"
#include "mmml/diagnostics.hpp"
#include "mmml/parse.hpp"

namespace mmml {
namespace {

std::ostream& operator<<(std::ostream& out, std::u8string_view str)
{
    return out << as_string_view(str);
}

struct Expected_Content;

struct Expected_Argument {
    std::vector<Expected_Content> content;
    std::u8string_view name = u8"";

    Expected_Argument(std::u8string_view name, std::vector<Expected_Content>&& content);

    Expected_Argument(std::vector<Expected_Content>&& content);

    ~Expected_Argument();

    static Expected_Argument from(const ast::Argument& arg, std::u8string_view source);

    [[nodiscard]]
    bool matches(const ast::Argument& actual, std::u8string_view source) const;

    friend bool operator==(const Expected_Argument&, const Expected_Argument&) = default;
};

enum struct Expected_Content_Type : Default_Underlying {
    text,
    escape,
    directive,
};

struct Expected_Content {
    Expected_Content_Type type;
    std::u8string_view name_or_text;
    std::vector<Expected_Argument> arguments {};
    std::vector<Expected_Content> content {};

    static Expected_Content text(std::u8string_view text)
    {
        return { .type = Expected_Content_Type::text, .name_or_text = text };
    }

    static Expected_Content escape(std::u8string_view text)
    {
        return { .type = Expected_Content_Type::escape, .name_or_text = text };
    }

    static Expected_Content directive(std::u8string_view name)
    {
        return { .type = Expected_Content_Type::directive, .name_or_text = name };
    }
    static Expected_Content
    directive(std::u8string_view name, std::vector<Expected_Argument>&& arguments)
    {
        return { .type = Expected_Content_Type::directive,
                 .name_or_text = name,
                 .arguments = std::move(arguments) };
    }
    static Expected_Content
    directive(std::u8string_view name, std::vector<Expected_Content>&& content)
    {
        return { .type = Expected_Content_Type::directive,
                 .name_or_text = name,
                 .content = std::move(content) };
    }
    static Expected_Content directive(
        std::u8string_view name,
        std::vector<Expected_Argument>&& arguments,
        std::vector<Expected_Content>&& content
    )
    {
        return { .type = Expected_Content_Type::directive,
                 .name_or_text = name,
                 .arguments = std::move(arguments),
                 .content = std::move(content) };
    }

    static Expected_Content from(const ast::Content& actual, std::u8string_view source)
    {
        if (const auto* const d = get_if<ast::Directive>(&actual)) {
            return from(*d, source);
        }
        if (const auto* const e = get_if<ast::Escaped>(&actual)) {
            return escape(e->get_text(source));
        }
        const std::u8string_view result = get<ast::Text>(actual).get_text(source);
        return text(result);
    }

    static Expected_Content from(const ast::Directive& actual, std::u8string_view source)
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
    bool matches(const ast::Content& other, std::u8string_view source) const;

    [[nodiscard]]
    bool matches(const ast::Directive& actual, std::u8string_view source) const;

    friend bool operator==(const Expected_Content&, const Expected_Content&) = default;
};

Expected_Argument::Expected_Argument(
    std::u8string_view name,
    std::vector<Expected_Content>&& content
)
    : content { std::move(content) }
    , name { name }
{
}

Expected_Argument::Expected_Argument(std::vector<Expected_Content>&& content)
    : content { std::move(content) }
{
}

Expected_Argument::~Expected_Argument() = default;

Expected_Argument Expected_Argument::from(const ast::Argument& arg, std::u8string_view source)
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

void write_special_escaped(std::ostream& out, char8_t c)
{
    if (c == u8'\n') {
        out << "\\n";
    }
    else if (c == u8'\r') {
        out << "\\r";
    }
    else if (c == u8'\t') {
        out << "\\t";
    }
    else {
        out << char(c);
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
        for (char8_t c : content.name_or_text) {
            write_special_escaped(out, c);
        }
        out << ')';
        break;
    }

    case Expected_Content_Type::escape: {
        out << "Escape(" << std::u8string_view(content.name_or_text) << ')';
        break;
    }
    }
    return out;
}

struct [[nodiscard]] Actual_Document {
    std::pmr::vector<char8_t> source;
    std::pmr::vector<ast::Content> content;

    [[nodiscard]]
    std::u8string_view source_string() const
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
    const std::string_view full_file_name = as_string_view(full_file);

    Parsed_File result { .source = std::pmr::vector<char8_t> { memory },
                         .instructions = std::pmr::vector<AST_Instruction> { memory } };

    Result<void, mmml::IO_Error_Code> r = file_to_bytes(result.source, full_file_name);
    if (!r) {
        Annotated_String8 out { memory };
        print_io_error(out, full_file_name, r.error());
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

    std::pmr::vector<ast::Content> content = build_ast(source_string, parsed->instructions, memory);
    return Actual_Document { std::move(parsed->source), std::move(content) };
}

void append_instruction(Annotated_String8& out, const AST_Instruction& ins)
{
    out.append(ast_instruction_type_name(ins.type), Annotation_Type::diagnostic_tag);
    if (ast_instruction_type_has_operand(ins.type)) {
        out.append(u8' ');
        out.append_integer(ins.n);
    }
    else if (ins.n != 0) {
        out.append(u8' ');
        out.append_integer(ins.n, Annotation_Type::diagnostic_error_text);
    }
}

void dump_instructions(Annotated_String8& out, std::span<const AST_Instruction> instructions)
{
    for (const auto& i : instructions) {
        out.append(u8"    ");
        append_instruction(out, i);
        out.append(u8'\n');
    }
}

bool run_parse_test(std::u8string_view file, std::span<const AST_Instruction> expected)
{
    std::pmr::monotonic_buffer_resource memory;
    std::optional<Parsed_File> actual = parse_file(file, &memory);
    if (!actual) {
        Annotated_String8 error;
        error.append(
            u8"Test failed because file couldn't be loaded and parsed.\n",
            Annotation_Type::diagnostic_error_text
        );
        print_code_string(std::cout, error, is_stdout_tty);
        return false;
    }
    if (!std::ranges::equal(expected, actual->instructions)) {
        Annotated_String8 error;
        error.append(
            u8"Test failed because expected parser output isn't matched.\n",
            Annotation_Type::diagnostic_error_text
        );
        error.append(u8"Expected:\n", Annotation_Type::diagnostic_text);
        dump_instructions(error, expected);
        error.append(u8"Actual:\n", Annotation_Type::diagnostic_text);
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
    ASSERT_TRUE(run_parse_test(u8"empty.mmml", expected));
}

TEST(Parse_And_Build, empty)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const std::pmr::vector<Expected_Content> expected { &memory };

    MMML_PARSE_AND_BUILD_BOILERPLATE(u8"empty.mmml");
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
    ASSERT_TRUE(run_parse_test(u8"hello_code.mmml", expected));
}

TEST(Parse_And_Build, hello_code)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const std::pmr::vector<Expected_Content> expected {
        { Expected_Content::directive(u8"c", { Expected_Content::text(u8"/* awoo */") }),
          Expected_Content::text(u8"\n") },
        &memory
    };

    MMML_PARSE_AND_BUILD_BOILERPLATE(u8"hello_code.mmml");
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
    ASSERT_TRUE(run_parse_test(u8"hello_directive.mmml", expected));
}

TEST(Parse_And_Build, hello_directive)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const Expected_Argument arg0 { u8"hello", { Expected_Content::text(u8"world") } };
    static const Expected_Argument arg1 { u8"x", { Expected_Content::text(u8"0") } };

    static const std::pmr::vector<Expected_Content> expected {
        { Expected_Content::directive(u8"b", { arg0, arg1 }, { Expected_Content::text(u8"test") }),
          Expected_Content::text(u8"\n") },
        &memory
    };

    MMML_PARSE_AND_BUILD_BOILERPLATE(u8"hello_directive.mmml");
}

TEST(Parse_And_Build, directive_arg_balanced_braces)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const Expected_Argument arg0 { u8"x", { Expected_Content::text(u8"{}") } };
    static const Expected_Argument arg1 { { Expected_Content::text(u8"{}") } };

    static const std::pmr::vector<Expected_Content> expected {
        { Expected_Content::directive(u8"d", { arg0, arg1 }), Expected_Content::text(u8"\n") },
        &memory
    };

    MMML_PARSE_AND_BUILD_BOILERPLATE(u8"directive_arg_balanced_braces.mmml");
}

TEST(Parse_And_Build, directive_arg_unbalanced_brace)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const std::pmr::vector<Expected_Content> expected {
        { Expected_Content::directive(u8"d"), Expected_Content::text(u8"[}]\n") }, &memory
    };

    MMML_PARSE_AND_BUILD_BOILERPLATE(u8"directive_arg_unbalanced_brace.mmml");
}

TEST(Parse_And_Build, directive_arg_unbalanced_brace_2)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const std::pmr::vector<Expected_Content> expected {
        { Expected_Content::directive(
              u8"x", { Expected_Content::directive(u8"y"), Expected_Content::text(u8"[") }
          ),
          Expected_Content::text(u8"]\n") },
        &memory
    };

    MMML_PARSE_AND_BUILD_BOILERPLATE(u8"directive_arg_unbalanced_brace_2.mmml");
}

TEST(Parse_And_Build, directive_arg_unbalanced_through_brace_escape)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const std::pmr::vector<Expected_Content> expected {
        { Expected_Content::directive(u8"d"), Expected_Content::text(u8"["),
          Expected_Content::escape(u8"\\{"), Expected_Content::text(u8"}]\n") },
        &memory
    };

    MMML_PARSE_AND_BUILD_BOILERPLATE(u8"directive_arg_unbalanced_through_brace_escape.mmml");
}

TEST(Parse_And_Build, directive_brace_escape)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const std::pmr::vector<Expected_Content> expected {
        { Expected_Content::directive(u8"d"), Expected_Content::text(u8"["),
          Expected_Content::escape(u8"\\{"), Expected_Content::text(u8"}]\n") },
        &memory
    };

    MMML_PARSE_AND_BUILD_BOILERPLATE(u8"directive_arg_unbalanced_through_brace_escape.mmml");
}

} // namespace
} // namespace mmml
