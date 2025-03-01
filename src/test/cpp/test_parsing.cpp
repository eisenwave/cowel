#include <memory_resource>
#include <ostream>
#include <vector>

#include <gtest/gtest.h>

#include "mmml/ast.hpp"
#include "mmml/code_string.hpp"
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

struct Expected_Content {
    bool is_directive;
    std::string_view name_or_text;
    std::vector<Expected_Argument> arguments;
    std::vector<Expected_Content> content;

    static Expected_Content text(std::string_view text)
    {
        return { .is_directive = false, .name_or_text = text };
    }

    static Expected_Content directive(std::string_view name)
    {
        return { .is_directive = true, .name_or_text = name };
    }
    static Expected_Content
    directive(std::string_view name, std::vector<Expected_Argument>&& arguments)
    {
        return { .is_directive = true, .name_or_text = name, .arguments = std::move(arguments) };
    }
    static Expected_Content
    directive(std::string_view name, std::vector<Expected_Content>&& content)
    {
        return { .is_directive = true, .name_or_text = name, .content = std::move(content) };
    }
    static Expected_Content directive(
        std::string_view name,
        std::vector<Expected_Argument>&& arguments,
        std::vector<Expected_Content>&& content
    )
    {
        return { .is_directive = true,
                 .name_or_text = name,
                 .arguments = std::move(arguments),
                 .content = std::move(content) };
    }

    static Expected_Content from(const ast::Content& actual, std::string_view source)
    {
        if (const auto* const d = get_if<ast::Directive>(&actual)) {
            return from(*d, source);
        }
        std::string_view result = get<ast::Text>(actual).get_text(source);
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
    if (content.is_directive) {
        out << '\\' << content.name_or_text << '[' << content.arguments << "]{" << content.content
            << '}';
    }
    else {
        out << "Text(";
        for (char c : content.name_or_text) {
            write_special_escaped(out, c);
        }
        out << ')';
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
};

[[nodiscard]]
std::optional<Parsed_File> parse_file(std::string_view file, std::pmr::memory_resource* memory)
{
    std::pmr::string full_file { "test/", memory };
    full_file += file;

    Parsed_File result { .source { memory }, .instructions { memory } };

    std::pmr::vector<char> source { memory };
    if (Result<void, mmml::IO_Error_Code> r = file_to_bytes(source, full_file); !r) {
        Code_String out { memory };
        print_io_error(out, full_file, r.error());
        print_code_string(std::cout, out, is_stdout_tty);
        return {};
    }
    const std::string_view source_string { source.data(), source.size() };

    parse(result.instructions, source_string);
    return result;
}

[[nodiscard]]
std::optional<Actual_Document>
parse_and_build_file(std::string_view file, std::pmr::memory_resource* memory)
{
    std::optional<Parsed_File> parsed = parse_file(file, memory);
    const std::string_view source_string { parsed->source.data(), parsed->source.size() };

    std::pmr::vector<ast::Content> content = build_ast(source_string, parsed->instructions, memory);
    return Actual_Document { std::move(parsed->source), std::move(content) };
}

#define PARSING_TEST_BOILERPLATE(...)                                                              \
    std::optional<Actual_Document> parsed = parse_and_build_file(__VA_ARGS__, &memory);            \
    ASSERT_TRUE(parsed);                                                                           \
    const auto actual = parsed->to_expected();                                                     \
    ASSERT_EQ(expected, actual)

TEST(Parse_And_Build, empty)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const std::pmr::vector<Expected_Content> expected { &memory };

    PARSING_TEST_BOILERPLATE("empty.mmml");
}

TEST(Parse_And_Build, hello_code)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const std::pmr::vector<Expected_Content> expected {
        { Expected_Content::directive("c", { Expected_Content::text("/* awoo */") }),
          Expected_Content::text("\n") },
        &memory
    };

    PARSING_TEST_BOILERPLATE("hello_code.mmml");
}

TEST(Parse_And_Build, hello_directive)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const Expected_Argument arg0 { "hello", { Expected_Content::text(" world") } };
    static const Expected_Argument arg1 { "x", { Expected_Content::text(" 0") } };

    static const std::pmr::vector<Expected_Content> expected {
        { Expected_Content::directive("b", { arg0, arg1 }, { Expected_Content::text("test") }),
          Expected_Content::text("\n") },
        &memory
    };

    PARSING_TEST_BOILERPLATE("hello_directive.mmml");
}

TEST(Parse_And_Build, directive_arg_balanced_braces)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const Expected_Argument arg0 { "x", { Expected_Content::text("{}") } };
    static const Expected_Argument arg1 { { Expected_Content::text("{}") } };

    static const std::pmr::vector<Expected_Content> expected {
        { Expected_Content::directive("d", { arg0, arg1 }), Expected_Content::text("\n") }, &memory
    };

    PARSING_TEST_BOILERPLATE("directive_arg_balanced_braces.mmml");
}

TEST(Parse_And_Build, directive_arg_unbalanced_brace)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const std::pmr::vector<Expected_Content> expected {
        { Expected_Content::directive("d"), Expected_Content::text("[}]\n") }, &memory
    };

    PARSING_TEST_BOILERPLATE("directive_arg_unbalanced_brace.mmml");
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

    PARSING_TEST_BOILERPLATE("directive_arg_unbalanced_brace_2.mmml");
}

TEST(Parse_And_Build, directive_arg_unbalanced_through_brace_escape)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const std::pmr::vector<Expected_Content> expected {
        { Expected_Content::directive("d"), Expected_Content::text("[\\{}]\n") }, &memory
    };

    PARSING_TEST_BOILERPLATE("directive_arg_unbalanced_through_brace_escape.mmml");
}

TEST(Parse_And_Build, directive_brace_escape)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const std::pmr::vector<Expected_Content> expected {
        { Expected_Content::directive("d"), Expected_Content::text("[\\{}]\n") }, &memory
    };

    PARSING_TEST_BOILERPLATE("directive_arg_unbalanced_through_brace_escape.mmml");
}

} // namespace
} // namespace mmml
