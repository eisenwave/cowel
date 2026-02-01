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

#include "cowel/lex.hpp"
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
    unquoted_member_name,
    unquoted_string,
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
    static Node from(const ast::Member_Value& arg)
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

[[nodiscard]]
std::optional<Parsed_File> parse_file(std::u8string_view file, std::pmr::memory_resource* memory)
{
    std::pmr::u8string full_file { u8"test/parse/", memory };
    full_file += file;

    Parsed_File result {
        .source = std::pmr::vector<char8_t> { memory },
        .tokens = std::pmr::vector<Token> { memory },
        .instructions = std::pmr::vector<CST_Instruction> { memory },
    };

    const Result<void, cowel::IO_Error_Code> r = load_utf8_file(result.source, full_file);
    if (!r) {
        Diagnostic_String out { memory };
        print_io_error(out, full_file, r.error());
        print_code_string_stdout(out);
        return {};
    }

    const std::convertible_to<Parse_Error_Consumer> auto on_error
        = [&](std::u8string_view /* id */, const Source_Span& location, Char_Sequence8 message) {
              Diagnostic_String out { memory };
              print_file_position(out, file, location);
              out.append(u8' ');
              append_char_sequence(out, message, Diagnostic_Highlight::text);
              out.append(u8'\n');
              print_code_string_stdout(out);
          };
    if (!lex(result.tokens, result.get_source_string(), on_error)) {
        return {};
    }
    if (!parse(result.instructions, result.tokens, on_error)) {
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

bool run_parse_test(std::u8string_view file, std::span<const CST_Instruction> expected)
{
    for (const auto& e : expected) {
        COWEL_ASSERT(cst_instruction_kind_has_operand(e.kind) || e.n == 0);
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
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 0 },
        { CST_Instruction_Kind::pop_document, 0 },
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
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 2 },
        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_block, 4 },
        { CST_Instruction_Kind::escape },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::escape },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::pop_block },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"directive_brace_escape_2.cow", expected));
}

TEST(Parse, directive_multiline)
{
    // clang-format off
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 2 },
        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_named_member },
        { CST_Instruction_Kind::unquoted_member_name },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::equals },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::unquoted_string },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::pop_named_member },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"directive_multiline.cow", expected));
}

TEST(Parse, directive_multiline_trailing_comma)
{
    // clang-format off
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 2 },
        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_named_member },
        { CST_Instruction_Kind::unquoted_member_name },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::equals },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::unquoted_string },
        { CST_Instruction_Kind::pop_named_member },
        { CST_Instruction_Kind::comma },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"directive_multiline_trailing_comma.cow", expected));
}

TEST(Parse, comments)
{
    // clang-format off
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 17 },
        { CST_Instruction_Kind::line_comment },
        { CST_Instruction_Kind::line_comment },
        { CST_Instruction_Kind::line_comment },
        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::line_comment },
        { CST_Instruction_Kind::line_comment },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::line_comment },
        { CST_Instruction_Kind::line_comment },
        { CST_Instruction_Kind::block_comment },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::block_comment },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::block_comment },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::block_comment },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"comments.cow", expected));
}

TEST(Parse, arguments_comments_1)
{
    // clang-format off
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 2 },
        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 0 },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::pop_document },
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
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 2 },
        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 2 },

        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::unquoted_string },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_named_member },
        { CST_Instruction_Kind::unquoted_member_name },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::equals },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::unquoted_string },
        { CST_Instruction_Kind::pop_named_member },
        { CST_Instruction_Kind::comma },
        { CST_Instruction_Kind::skip },

        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::pop_document },
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

TEST(Parse, arguments_ellipsis)
{
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 2 },
        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::push_ellipsis_argument },
        { CST_Instruction_Kind::ellipsis },
        { CST_Instruction_Kind::pop_ellipsis_argument },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::pop_document },
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
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 2 },
        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 2 },

        { CST_Instruction_Kind::push_positional_member }, // (x)
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::unquoted_string },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_positional_member }, // ()
        { CST_Instruction_Kind::push_group },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_positional_member },

        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::pop_document },
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
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 2 },
        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 1 },

        { CST_Instruction_Kind::push_named_member }, // n =  (x, y)
        { CST_Instruction_Kind::unquoted_member_name },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::equals },
        { CST_Instruction_Kind::skip },
        
        { CST_Instruction_Kind::push_group, 2 }, // (x, y)
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::unquoted_string },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::unquoted_string },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::pop_group },

        { CST_Instruction_Kind::pop_named_member },

        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::pop_document },
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

TEST(Parse, group_3)
{
    // clang-format off
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 2 },
        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::push_group, 0 },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::pop_document },
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
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 10 },

        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::keyword_null },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },

        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::keyword_true },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },

        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::keyword_false },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },

        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::unquoted_string },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },

        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::unquoted_string },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },

        { CST_Instruction_Kind::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"arguments/unquoted.cow", expected));
}

TEST(Parse, string)
{
    // clang-format off
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 8 },

        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::push_quoted_string },
        { CST_Instruction_Kind::pop_quoted_string },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },

        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::push_quoted_string, 1 },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::pop_quoted_string },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },

        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::push_quoted_string, 1 },
        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::pop_quoted_string },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },

        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::push_quoted_string, 3 },
        { CST_Instruction_Kind::escape },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::escape },
        { CST_Instruction_Kind::pop_quoted_string },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },

        { CST_Instruction_Kind::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"arguments/string.cow", expected));
}

TEST(Parse, block)
{
    // clang-format off
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 8 },

        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::push_block, 0 },
        { CST_Instruction_Kind::pop_block },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },

        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::push_block, 1 },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::pop_block },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },

        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::push_block, 1 },
        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::pop_block },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },

        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::push_block, 5 },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::escape },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::escape },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::pop_block },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },

        { CST_Instruction_Kind::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"arguments/block.cow", expected));
}

TEST(Parse, integers)
{
    // clang-format off
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 2 },
        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 4 },

        { CST_Instruction_Kind::push_positional_member }, // 0
        { CST_Instruction_Kind::decimal_int },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },
        { CST_Instruction_Kind::skip },

        { CST_Instruction_Kind::push_positional_member }, // 123
        { CST_Instruction_Kind::decimal_int },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },
        { CST_Instruction_Kind::skip },

        { CST_Instruction_Kind::push_positional_member }, // -123
        { CST_Instruction_Kind::decimal_int },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },
        { CST_Instruction_Kind::skip },

        { CST_Instruction_Kind::push_positional_member }, // 0xff
        { CST_Instruction_Kind::hexadecimal_int },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"integers.cow", expected));
}

TEST(Parse, literals)
{
    // clang-format off
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 2 },
        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 7 },

        { CST_Instruction_Kind::skip }, // unit
        { CST_Instruction_Kind::push_positional_member }, 
        { CST_Instruction_Kind::keyword_unit }, 
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip }, // null
        { CST_Instruction_Kind::push_positional_member }, 
        { CST_Instruction_Kind::keyword_null }, 
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip }, // true
        { CST_Instruction_Kind::push_positional_member }, 
        { CST_Instruction_Kind::keyword_true }, 
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip }, // false
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::keyword_false }, 
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip }, // 0
        { CST_Instruction_Kind::push_positional_member }, 
        { CST_Instruction_Kind::decimal_int }, 
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip }, // ""
        { CST_Instruction_Kind::push_positional_member }, 
        { CST_Instruction_Kind::push_quoted_string }, 
        { CST_Instruction_Kind::pop_quoted_string }, 
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip }, // awoo
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::unquoted_string }, 
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"literals.cow", expected));
}

TEST(Parse, floats)
{
    // clang-format off
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 2 },
        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 15 },

        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_positional_member }, 
        { CST_Instruction_Kind::decimal_float }, 
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_positional_member }, 
        { CST_Instruction_Kind::decimal_float }, 
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_positional_member }, 
        { CST_Instruction_Kind::decimal_float }, 
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_positional_member }, 
        { CST_Instruction_Kind::decimal_float }, 
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_positional_member }, 
        { CST_Instruction_Kind::decimal_float }, 
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_positional_member }, 
        { CST_Instruction_Kind::decimal_float }, 
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_positional_member }, 
        { CST_Instruction_Kind::decimal_float }, 
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_positional_member }, 
        { CST_Instruction_Kind::decimal_float }, 
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_positional_member }, 
        { CST_Instruction_Kind::decimal_float }, 
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_positional_member }, 
        { CST_Instruction_Kind::decimal_float }, 
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_positional_member }, 
        { CST_Instruction_Kind::decimal_float }, 
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_positional_member }, 
        { CST_Instruction_Kind::decimal_float }, 
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_positional_member }, 
        { CST_Instruction_Kind::decimal_float }, 
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_positional_member }, 
        { CST_Instruction_Kind::decimal_float }, 
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_positional_member }, 
        { CST_Instruction_Kind::decimal_float }, 
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::pop_document },
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

TEST(Parse, escape_lf)
{
    // clang-format off
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 3 },
        { CST_Instruction_Kind::escape },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::escape },
        { CST_Instruction_Kind::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"escape_lf.cow", expected));
}

TEST(Parse, escape_crlf)
{
    // clang-format off
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 3 },
        { CST_Instruction_Kind::escape },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::escape },
        { CST_Instruction_Kind::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"escape_crlf.cow", expected));
}

TEST(Parse, file_ends_in_brace)
{
    // clang-format off
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 1 },
        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_block },
        { CST_Instruction_Kind::pop_block },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::pop_document },
    };
    // clang-format on
    ASSERT_TRUE(run_parse_test(u8"file_ends_in_brace.cow", expected));
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

TEST(Parse, hello_code)
{
    // clang-format off
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 2 },
        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_block, 1 },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::pop_block },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },
        { CST_Instruction_Kind::pop_document, 0 },
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
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 2 },
        { CST_Instruction_Kind::push_directive_splice },
        { CST_Instruction_Kind::push_group, 2 },

        { CST_Instruction_Kind::push_named_member },
        { CST_Instruction_Kind::unquoted_member_name }, // "hello"
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::equals },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::unquoted_string },          // "world"
        { CST_Instruction_Kind::pop_named_member },

        { CST_Instruction_Kind::comma },

        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_named_member },
        { CST_Instruction_Kind::unquoted_member_name }, // "x"
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::equals },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::decimal_int },          // "0"
        { CST_Instruction_Kind::pop_named_member },

        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::push_block, 1 },    // {
        { CST_Instruction_Kind::text },          // "test"
        { CST_Instruction_Kind::pop_block },        // }
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text },          // \n
        { CST_Instruction_Kind::pop_document },
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
    static constexpr CST_Instruction expected[] {
        { CST_Instruction_Kind::push_document, 8 },
        
        { CST_Instruction_Kind::push_directive_splice }, // \a(x())
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::push_directive_call },
        { CST_Instruction_Kind::push_group, 0 },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_call },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text }, // \n

        { CST_Instruction_Kind::push_directive_splice }, // \b(x(){})
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::push_directive_call },
        { CST_Instruction_Kind::push_group, 0 },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::push_block, 0 },
        { CST_Instruction_Kind::pop_block },
        { CST_Instruction_Kind::pop_directive_call },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text }, // \n

        { CST_Instruction_Kind::push_directive_splice }, // \c(x () {})
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::push_directive_call },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_group, 0 },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::skip },
        { CST_Instruction_Kind::push_block, 0 },
        { CST_Instruction_Kind::pop_block },
        { CST_Instruction_Kind::pop_directive_call },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text }, // \n

        { CST_Instruction_Kind::push_directive_splice }, // \d(x{})
        { CST_Instruction_Kind::push_group, 1 },
        { CST_Instruction_Kind::push_positional_member },
        { CST_Instruction_Kind::push_directive_call },
        { CST_Instruction_Kind::push_block, 0 },
        { CST_Instruction_Kind::pop_block },
        { CST_Instruction_Kind::pop_directive_call },
        { CST_Instruction_Kind::pop_positional_member },
        { CST_Instruction_Kind::pop_group },
        { CST_Instruction_Kind::pop_directive_splice },
        { CST_Instruction_Kind::text }, // \n

        { CST_Instruction_Kind::pop_document },
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
