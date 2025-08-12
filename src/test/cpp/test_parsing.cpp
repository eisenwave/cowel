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

struct Expected_Content;

struct Expected_Argument {
    std::vector<Expected_Content> content;
    std::u8string_view name {};
    bool is_ellipsis;

    static Expected_Argument ellipsis();

private:
    Expected_Argument(
        std::u8string_view name,
        std::vector<Expected_Content>&& content,
        bool is_ellipsis
    );

public:
    Expected_Argument(std::u8string_view name, std::vector<Expected_Content>&& content)
        : Expected_Argument(name, std::move(content), false)
    {
    }

    Expected_Argument(std::vector<Expected_Content>&& content)
        : Expected_Argument({}, std::move(content), false)
    {
    }

    Expected_Argument(Expected_Argument&&) noexcept;
    Expected_Argument(const Expected_Argument&);
    Expected_Argument& operator=(Expected_Argument&&) noexcept;
    Expected_Argument& operator=(const Expected_Argument&);
    ~Expected_Argument();

    static Expected_Argument from(const ast::Argument& arg);

    [[maybe_unused]]
    friend bool operator==(const Expected_Argument&, const Expected_Argument&)
        = default;
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

    static Expected_Content from(const ast::Content& actual)
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

    static Expected_Content from(const ast::Directive& actual)
    {
        std::vector<Expected_Argument> arguments;
        arguments.reserve(actual.get_arguments().size());
        for (const ast::Argument& arg : actual.get_arguments()) {
            arguments.push_back(Expected_Argument::from(arg));
        }

        std::vector<Expected_Content> content;
        content.reserve(actual.get_content().size());
        for (const ast::Content& c : actual.get_content()) {
            content.push_back(from(c));
        }

        return directive(actual.get_name(), std::move(arguments), std::move(content));
    }

    [[nodiscard]]
    bool matches(const ast::Content& other, std::u8string_view source) const;

    [[nodiscard]]
    bool matches(const ast::Directive& actual, std::u8string_view source) const;

    [[maybe_unused]]
    friend bool operator==(const Expected_Content&, const Expected_Content&)
        = default;
};

Expected_Argument::Expected_Argument(
    std::u8string_view name,
    std::vector<Expected_Content>&& content,
    bool is_ellipsis
)
    : content { std::move(content) }
    , name { name }
    , is_ellipsis { is_ellipsis }
{
}

Expected_Argument Expected_Argument::ellipsis()
{
    return { {}, {}, true };
}

[[maybe_unused]]
Expected_Argument::Expected_Argument(Expected_Argument&&) noexcept
    = default;
[[maybe_unused]]
Expected_Argument::Expected_Argument(const Expected_Argument&)
    = default;
[[maybe_unused]]
Expected_Argument& Expected_Argument::operator=(Expected_Argument&&) noexcept
    = default;
[[maybe_unused]]
Expected_Argument& Expected_Argument::operator=(const Expected_Argument&)
    = default;
[[maybe_unused]]
Expected_Argument::~Expected_Argument()
    = default;

Expected_Argument Expected_Argument::from(const ast::Argument& arg)
{
    std::vector<Expected_Content> content;
    for (const ast::Content& c : arg.get_content()) {
        content.push_back(Expected_Content::from(c));
    }
    switch (arg.get_type()) {
    case ast::Argument_Type::ellipsis: {
        COWEL_ASSERT(content.empty());
        return Expected_Argument::ellipsis();
    }
    case ast::Argument_Type::named: {
        return Expected_Argument { arg.get_name(), std::move(content) };
    }
    case ast::Argument_Type::positional: {
        return auto(std::move(content));
    }
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid argument type.");
}

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

std::ostream& operator<<(std::ostream& out, const Expected_Argument& arg)
{
    if (arg.is_ellipsis) {
        out << "...";
    }
    else if (!arg.name.empty()) {
        out << "Arg(" << arg.name << "){" << arg.content << '}';
    }
    else {
        out << "Arg{" << arg.content << '}';
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
        for (const char8_t c : content.name_or_text) {
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
    ast::Pmr_Vector<ast::Content> content;

    [[nodiscard]]
    std::u8string_view source_string() const
    {
        return { source.data(), source.size() };
    }

    [[nodiscard]]
    ast::Pmr_Vector<Expected_Content> to_expected() const
    {
        ast::Pmr_Vector<Expected_Content> result { content.get_allocator() };
        result.reserve(content.size());
        for (const auto& actual : content) {
            result.push_back(Expected_Content::from(actual));
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
    static const ast::Pmr_Vector<Expected_Content> expected { &memory };

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

    static const ast::Pmr_Vector<Expected_Content> expected {
        {
            Expected_Content::directive(u8"a"),
            Expected_Content::text(u8"\n"),
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

        { AST_Instruction_Type::push_argument, 1 },
        { AST_Instruction_Type::skip, 13 },
        { AST_Instruction_Type::text, 4 },
        { AST_Instruction_Type::pop_argument },
        { AST_Instruction_Type::argument_comma },

        { AST_Instruction_Type::push_argument, 1 },
        { AST_Instruction_Type::skip, 13 },
        { AST_Instruction_Type::argument_name, 5 },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::argument_equal },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::text, 3 },
        { AST_Instruction_Type::pop_argument },
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

    static const ast::Pmr_Vector<Expected_Content> expected {
        {
            Expected_Content::directive(
                u8"b",
                {
                    Expected_Argument { { Expected_Content::text(u8"text") } },
                    Expected_Argument { u8"named", { Expected_Content::text(u8"arg") } },
                }
            ),
            Expected_Content::text(u8"\n"),
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
        { AST_Instruction_Type::push_argument, 0 },
        { AST_Instruction_Type::argument_ellipsis, 3 },
        { AST_Instruction_Type::pop_argument },
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

    static const ast::Pmr_Vector<Expected_Content> expected {
        {
            Expected_Content::directive(u8"x", { Expected_Argument::ellipsis() }),
            Expected_Content::text(u8"\n"),
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
        { AST_Instruction_Type::push_argument, 1 },
        { AST_Instruction_Type::text, 7 },
        { AST_Instruction_Type::pop_argument },
        { AST_Instruction_Type::pop_arguments },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::push_directive, 2 }, // \b
        { AST_Instruction_Type::push_arguments, 1 },
        { AST_Instruction_Type::push_argument, 1 },
        { AST_Instruction_Type::argument_name, 1 },
        { AST_Instruction_Type::argument_equal },
        { AST_Instruction_Type::text, 3 },
        { AST_Instruction_Type::pop_argument },
        { AST_Instruction_Type::pop_arguments },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::push_directive, 2 }, // \c
        { AST_Instruction_Type::push_arguments, 1 },
        { AST_Instruction_Type::push_argument, 1 },
        { AST_Instruction_Type::argument_ellipsis, 3 },
        { AST_Instruction_Type::text, 4 },
        { AST_Instruction_Type::pop_argument },
        { AST_Instruction_Type::pop_arguments },
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

        { AST_Instruction_Type::pop_document },
    };
    // clang-format on

    ASSERT_TRUE(run_parse_test(u8"arguments/not_ellipsis.cow", expected));
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

        { AST_Instruction_Type::push_directive, 3 }, // \-x
        { AST_Instruction_Type::pop_directive },
        { AST_Instruction_Type::text, 1 },

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
    static const ast::Pmr_Vector<Expected_Content> expected {
        {
            Expected_Content::directive(u8"c", { Expected_Content::text(u8"/* awoo */") }),
            Expected_Content::text(u8"\n"),
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

        { AST_Instruction_Type::push_argument, 1 },
        { AST_Instruction_Type::argument_name, 5 }, // "hello"
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::argument_equal },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::text, 5 },          // "world"
        { AST_Instruction_Type::pop_argument },

        { AST_Instruction_Type::argument_comma },

        { AST_Instruction_Type::push_argument, 1 },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::argument_name, 1 }, // "x"
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::argument_equal },
        { AST_Instruction_Type::skip, 1 },
        { AST_Instruction_Type::text, 1 },          // "0"
        { AST_Instruction_Type::pop_argument },

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
    static const Expected_Argument arg0 { u8"hello", { Expected_Content::text(u8"world") } };
    static const Expected_Argument arg1 { u8"x", { Expected_Content::text(u8"0") } };

    static const ast::Pmr_Vector<Expected_Content> expected {
        {
            Expected_Content::directive(
                u8"b", { arg0, arg1 }, { Expected_Content::text(u8"test") }
            ),
            Expected_Content::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"hello_directive.cow");
}

TEST(Parse_And_Build, arguments_balanced_braces)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const Expected_Argument arg0 { u8"x", { Expected_Content::text(u8"{}") } };
    static const Expected_Argument arg1 { { Expected_Content::text(u8"{}") } };

    static const ast::Pmr_Vector<Expected_Content> expected {
        {
            Expected_Content::directive(u8"d", { arg0, arg1 }),
            Expected_Content::text(u8"\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"arguments/balanced_braces.cow");
}

TEST(Parse_And_Build, arguments_unbalanced_brace_1)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Expected_Content> expected {
        {
            Expected_Content::directive(u8"d"),
            Expected_Content::text(u8"[}]\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"arguments/unbalanced_brace_1.cow");
}

TEST(Parse_And_Build, arguments_unbalanced_brace_2)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Expected_Content> expected {
        {
            Expected_Content::directive(
                u8"x", { Expected_Content::directive(u8"y"), Expected_Content::text(u8"[") }
            ),
            Expected_Content::text(u8"]\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"arguments/unbalanced_brace_2.cow");
}

TEST(Parse_And_Build, arguments_unbalanced_through_brace_escape)
{
    static std::pmr::monotonic_buffer_resource memory;
    static const ast::Pmr_Vector<Expected_Content> expected {
        {
            Expected_Content::directive(u8"d"),
            Expected_Content::text(u8"["),
            Expected_Content::escape(u8"\\{"),
            Expected_Content::text(u8"}]\n"),
        },
        &memory,
    };

    COWEL_PARSE_AND_BUILD_BOILERPLATE(u8"arguments/unbalanced_through_brace_escape.cow");
}

} // namespace
} // namespace cowel
