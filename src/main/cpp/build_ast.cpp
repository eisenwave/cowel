#include <cstddef>
#include <memory_resource>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/source_position.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/ast.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/fwd.hpp"
#include "cowel/parse.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace ast {

Group::Group(
    File_Source_Span source_span,
    std::u8string_view source,
    Pmr_Vector<Group_Member>&& members
)
    : m_source_span { source_span }
    , m_source { source }
    , m_members { std::move(members) }
{
    COWEL_ASSERT(m_source_span.length == m_source.length());
}

Group::Group(File_Source_Span source_span, std::u8string_view source)
    : Group { source_span, source, {} }
{
}

Content_Sequence::Content_Sequence(
    File_Source_Span source_span,
    std::u8string_view source,
    Pmr_Vector<Content>&& elements
)
    : m_source_span { source_span }
    , m_source { source }
    , m_elements { std::move(elements) }
{
    COWEL_ASSERT(m_source_span.length == m_source.length());
}

Content_Sequence::Content_Sequence(File_Source_Span source_span, std::u8string_view source)
    : Content_Sequence { source_span, source, {} }
{
}

Group_Member Group_Member::ellipsis(File_Source_Span source_span, std::u8string_view source)
{
    // clang-format off
    return {
        source_span,
        source,
        source_span.with_length(0),
        {},
        Content_Sequence{source_span, source},
        Member_Kind::ellipsis,
    };
    // clang-format on
}

Group_Member
Group_Member::named(const File_Source_Span& name_span, std::u8string_view name, Value&& value)
{
    COWEL_DEBUG_ASSERT(is_html_attribute_name(name));
    // clang-format off
    return {
        value.get_source_span(),
        value.get_source(),
        name_span,
        name,
        std::move(value),
        Member_Kind::named,
    };
    // clang-format on
}

[[nodiscard]]
Group_Member Group_Member::positional(Value&& value)
{
    const File_Source_Span source_span = value.get_source_span();
    // clang-format off
    return {
        source_span,
        value.get_source(),
        source_span.with_length(0),
        {},
        std::move(value),
        Member_Kind::positional,
    };
    // clang-format on
}

Group_Member::Group_Member(
    const File_Source_Span& source_span,
    std::u8string_view source,
    const File_Source_Span& name_span,
    std::u8string_view name,
    Value&& value,
    Member_Kind type
)
    : m_source_span { source_span }
    , m_source { source }
    , m_value { std::move(value) }
    , m_name_span { name_span }
    , m_name { name }
    , m_kind { type }
{
    COWEL_ASSERT(m_source_span.length == m_source.length());
    COWEL_ASSERT(m_name_span.length == m_name.length());
}

Directive::Directive(
    const File_Source_Span& source_span,
    std::u8string_view source,
    std::u8string_view name,
    std::optional<Group>&& args,
    std::optional<Content_Sequence>&& content
)
    : m_source_span { source_span }
    , m_source { source }
    , m_name { name }
    , m_arguments { std::move(args) }
    , m_content { std::move(content) }
{
    COWEL_ASSERT(m_source_span.length == m_source.length());
    COWEL_ASSERT(!name.empty());
    COWEL_ASSERT(!name.starts_with(u8'\\'));
    COWEL_ASSERT(name.length() <= source_span.length);

    // this needs to be late-initialized here because it is declared after m_arguments
    m_has_ellipsis
        = m_arguments
        && std::ranges::contains(
              m_arguments->get_members(), ast::Member_Kind::ellipsis, &ast::Group_Member::get_kind
        );
}

Text::Text(const File_Source_Span& source_span, std::u8string_view source)
    : m_source_span { source_span }
    , m_source { source }
{
    COWEL_ASSERT(!source_span.empty());
    COWEL_ASSERT(source.length() == source_span.length);
}

Comment::Comment(const File_Source_Span& source_span, std::u8string_view source)
    : m_source_span { source_span }
    , m_source { source }
    , m_suffix_length { source.ends_with(u8"\r\n")     ? 2uz
                            : source.ends_with(u8'\n') ? 1uz
                                                       : 0uz }
{
    COWEL_ASSERT(source.length() == source_span.length);
    COWEL_ASSERT(source.length() >= 2);
    COWEL_ASSERT(source.starts_with(u8"\\:"));
}

Escaped::Escaped(const File_Source_Span& source_span, std::u8string_view source)
    : m_source_span { source_span }
    , m_source { source }
{
    COWEL_ASSERT(source.length() == source_span.length);
    COWEL_ASSERT(source.length() >= 2);
    COWEL_ASSERT(source.starts_with('\\'));
}

} // namespace ast

namespace {

void advance(Source_Position& pos, char8_t c)
{
    switch (c) {
    case '\r': pos.column = 0; break;
    case '\n':
        pos.column = 0;
        pos.line += 1;
        break;
    default: pos.column += 1;
    }
    pos.begin += 1;
}

struct [[nodiscard]] AST_Builder {
private:
    using char_type = char8_t;
    using string_view_type = std::u8string_view;

    const string_view_type m_source;
    const File_Id m_file;
    const std::span<const AST_Instruction> m_instructions;
    std::pmr::memory_resource* const m_memory;
    Parse_Error_Consumer m_on_error;

    std::size_t m_index = 0;
    Source_Position m_pos {};

public:
    AST_Builder(
        string_view_type source,
        File_Id file,
        std::span<const AST_Instruction> instructions,
        std::pmr::memory_resource* memory,
        Parse_Error_Consumer on_error
    )
        : m_source { source }
        , m_file { file }
        , m_instructions { instructions }
        , m_memory { memory }
        , m_on_error { on_error }
    {
        COWEL_ASSERT(!instructions.empty());
    }

    void build_document(ast::Pmr_Vector<ast::Content>& out)
    {
        const AST_Instruction push_doc = pop();
        COWEL_ASSERT(push_doc.type == AST_Instruction_Type::push_document);
        out.clear();
        out.reserve(push_doc.n);

        for (std::size_t i = 0; i < push_doc.n; ++i) {
            append_content(out);
        }
    }

private:
    [[nodiscard]]
    std::u8string_view extract(const Source_Span& span) const
    {
        return m_source.substr(span.begin, span.length);
    }

    void advance_by(std::size_t n)
    {
        COWEL_ASSERT(m_pos.begin + n <= m_source.size());

        for (std::size_t i = 0; i < n; ++i) {
            advance(m_pos, m_source[m_pos.begin]);
        }
    }

    [[nodiscard]]
    bool eof() const
    {
        return m_index == m_instructions.size();
    }

    [[nodiscard]]
    AST_Instruction peek()
    {
        COWEL_ASSERT(m_index < m_instructions.size());
        return m_instructions[m_index];
    }

    AST_Instruction pop()
    {
        COWEL_ASSERT(m_index < m_instructions.size());
        return m_instructions[m_index++];
    }

    void append_content(ast::Pmr_Vector<ast::Content>& out)
    {
        const AST_Instruction instruction = peek();
        switch (instruction.type) {
            using enum AST_Instruction_Type;
        case skip: {
            advance_by(instruction.n);
            pop();
            break;
        }
        case argument_comma:
        case argument_equal: {
            advance_by(1);
            pop();
            break;
        }
        case escape: {
            out.push_back(build_escape());
            break;
        }
        case text: {
            out.push_back(build_text());
            break;
        }
        case comment: {
            out.push_back(build_comment());
            break;
        }
        case push_directive: {
            out.push_back(build_directive());
            break;
        }
        default: COWEL_ASSERT_UNREACHABLE(u8"Invalid content creating instruction.");
        }
    }

    [[nodiscard]]
    ast::Escaped build_escape()
    {
        const AST_Instruction instruction = pop();
        COWEL_ASSERT(instruction.type == AST_Instruction_Type::escape);

        const File_Source_Span span { m_pos, instruction.n, m_file };
        ast::Escaped result { span, extract(span) };
        advance_by(instruction.n);
        return result;
    }

    [[nodiscard]]
    ast::Text build_text()
    {
        const AST_Instruction instruction = pop();
        COWEL_ASSERT(instruction.type == AST_Instruction_Type::text);

        const File_Source_Span span { m_pos, instruction.n, m_file };
        ast::Text result { span, extract(span) };
        advance_by(instruction.n);
        return result;
    }

    [[nodiscard]]
    ast::Comment build_comment()
    {
        const AST_Instruction instruction = pop();
        COWEL_ASSERT(instruction.type == AST_Instruction_Type::comment);

        const File_Source_Span span { m_pos, instruction.n, m_file };
        ast::Comment result { span, extract(span) };
        advance_by(instruction.n);
        return result;
    }

    [[nodiscard]]
    ast::Directive build_directive()
    {
        const AST_Instruction instruction = pop();
        COWEL_ASSERT(instruction.type == AST_Instruction_Type::push_directive);
        COWEL_ASSERT(instruction.n >= 2);

        const Source_Position initial_pos = m_pos;
        const std::size_t name_length = instruction.n - 1;
        advance_by(instruction.n);

        std::optional<ast::Group> arguments = try_build_group();
        std::optional<ast::Content_Sequence> block = try_build_block();

        const AST_Instruction pop_instruction = pop();
        COWEL_ASSERT(pop_instruction.type == AST_Instruction_Type::pop_directive);

        const File_Source_Span source_span { initial_pos, m_pos.begin - initial_pos.begin, m_file };
        const File_Source_Span name_span = source_span.to_right(1).with_length(name_length);
        const std::u8string_view name = extract(name_span);

        return { source_span, extract(source_span), name, std::move(arguments), std::move(block) };
    }

    [[nodiscard]]
    std::optional<ast::Group> try_build_group()
    {
        if (eof()) {
            {};
        }
        const AST_Instruction instruction = peek();
        if (instruction.type != AST_Instruction_Type::push_arguments) {
            return {};
        }
        COWEL_DEBUG_ASSERT(m_source[m_pos.begin] == u8'(');

        const Source_Position initial_pos = m_pos;
        pop();
        advance_by(1);

        ast::Pmr_Vector<ast::Group_Member> members { m_memory };

        while (!eof()) {
            const AST_Instruction next = peek();
            if (next.type == AST_Instruction_Type::skip) {
                advance_by(next.n);
                pop();
                continue;
            }
            if (next.type == AST_Instruction_Type::argument_comma) {
                COWEL_DEBUG_ASSERT(m_source[m_pos.begin] == u8',');
                advance_by(1);
                pop();
                continue;
            }
            if (next.type == AST_Instruction_Type::argument_equal) {
                COWEL_DEBUG_ASSERT(m_source[m_pos.begin] == u8'=');
                advance_by(1);
                pop();
                continue;
            }
            if (next.type == AST_Instruction_Type::pop_arguments) {
                COWEL_DEBUG_ASSERT(m_source[m_pos.begin] == u8')');
                advance_by(1);
                pop();
                break;
            }
            members.push_back(build_group_member());
        }

        const File_Source_Span source_span {
            initial_pos,
            m_pos.begin - initial_pos.begin,
            m_file,
        };
        return ast::Group {
            source_span,
            extract(source_span),
            std::move(members),
        };
    }

    [[nodiscard]]
    ast::Group_Member build_group_member()
    {
        const AST_Instruction instruction = pop();
        ignore_skips();

        const Source_Position initial_pos = m_pos;

        switch (instruction.type) {
        case AST_Instruction_Type::push_named_argument: {
            const AST_Instruction name = pop();
            COWEL_ASSERT(name.type == AST_Instruction_Type::argument_name);
            advance_by(name.n);
            const File_Source_Span name_span { initial_pos, name.n, m_file };
            ignore_skips();

            const AST_Instruction equal = pop();
            COWEL_ASSERT(equal.type == AST_Instruction_Type::argument_equal);
            advance_by(1);
            ignore_skips();

            ast::Value value = build_value(instruction.n);
            ignore_skips();
            const auto pop_instruction = pop();
            COWEL_ASSERT(pop_instruction.type == AST_Instruction_Type::pop_named_argument);
            return ast::Group_Member::named(name_span, extract(name_span), std::move(value));
        }

        case AST_Instruction_Type::push_positional_argument: {
            ast::Value value = build_value(instruction.n);
            ignore_skips();
            const auto pop_instruction = pop();
            COWEL_ASSERT(pop_instruction.type == AST_Instruction_Type::pop_positional_argument);
            return ast::Group_Member::positional(std::move(value));
        }

        case AST_Instruction_Type::push_ellipsis_argument: {
            const AST_Instruction ellipsis = pop();
            advance_by(ellipsis.n);
            COWEL_ASSERT(ellipsis.type == AST_Instruction_Type::argument_ellipsis);

            ast::Value value = build_value(0);
            if (!m_on_error) { }
            else if (const auto* const group = std::get_if<ast::Group>(&value)) {
                constexpr std::u8string_view message
                    = u8"A group following an ellipsis is not allowed. "
                      u8"Use '\\.' to specify a literal dot instead of an ellipsis."sv;
                m_on_error(diagnostic::parse_block_unclosed, group->get_source_span(), message);
            }
            else if (const auto* const content = std::get_if<ast::Content_Sequence>(&value);
                     content && !content->empty()) {
                constexpr std::u8string_view message
                    = u8"Content following an ellipsis is not allowed. "
                      u8"Use '\\.' to specify a literal dot instead of an ellipsis."sv;
                m_on_error(diagnostic::parse_block_unclosed, content->get_source_span(), message);
            }

            const File_Source_Span source_span { initial_pos, ellipsis.n, m_file };
            ignore_skips();
            const auto pop_instruction = pop();
            COWEL_ASSERT(pop_instruction.type == AST_Instruction_Type::pop_ellipsis_argument);
            return ast::Group_Member::ellipsis(source_span, extract(source_span));
        }

        default: break;
        }

        COWEL_ASSERT_UNREACHABLE(u8"Bad use of build_group_member()");
    }

    void ignore_skips()
    {
        while (!eof() && peek().type == AST_Instruction_Type::skip) {
            const AST_Instruction skip = pop();
            advance_by(skip.n);
        }
    }

    [[nodiscard]]
    ast::Value build_value(std::size_t size_hint)
    {
        const Source_Position initial_pos = m_pos;
        const AST_Instruction instruction = peek();
        if (instruction.type == AST_Instruction_Type::push_arguments) {
            return try_build_group().value();
        }

        ast::Pmr_Vector<ast::Content> children { m_memory };
        children.reserve(size_hint);

        while (!eof() && !ast_instruction_type_is_pop_argument(peek().type)) {
            append_content(children);
        }

        const File_Source_Span source_span {
            initial_pos,
            m_pos.begin - initial_pos.begin,
            m_file,
        };
        return ast::Content_Sequence {
            source_span,
            extract(source_span),
            std::move(children),
        };
    }

    [[nodiscard]]
    std::optional<ast::Content_Sequence> try_build_block()
    {
        if (eof()) {
            return {};
        }
        const AST_Instruction instruction = peek();
        if (instruction.type == AST_Instruction_Type::error_unclosed_block) {
            if (m_on_error) {
                constexpr std::u8string_view message = u8"Unclosed block belonging to a directive.";
                const File_Source_Span span { m_pos, 1, m_file };
                m_on_error(diagnostic::parse_block_unclosed, span, message);
            }
            pop();
            advance_by(1);
            return {};
        }
        if (instruction.type != AST_Instruction_Type::push_block) {
            return {};
        }

        const Source_Position initial_pos = m_pos;
        pop();
        advance_by(1);

        ast::Pmr_Vector<ast::Content> content { m_memory };

        while (!eof()) {
            const AST_Instruction next = peek();
            if (next.type == AST_Instruction_Type::pop_block) {
                pop();
                break;
            }
            append_content(content);
        }
        advance_by(1);

        const File_Source_Span source_span {
            initial_pos,
            m_pos.begin - initial_pos.begin,
            m_file,
        };
        return ast::Content_Sequence {
            source_span,
            extract(source_span),
            std::move(content),
        };
    }
};

} // namespace

void build_ast(
    ast::Pmr_Vector<ast::Content>& out,
    std::u8string_view source,
    File_Id file,
    std::span<const AST_Instruction> instructions,
    std::pmr::memory_resource* memory,
    Parse_Error_Consumer on_error
)
{
    AST_Builder { source, file, instructions, memory, on_error }.build_document(out);
}

ast::Pmr_Vector<ast::Content> build_ast(
    std::u8string_view source,
    File_Id file,
    std::span<const AST_Instruction> instructions,
    std::pmr::memory_resource* memory,
    Parse_Error_Consumer on_error
)
{
    ast::Pmr_Vector<ast::Content> result { memory };
    build_ast(result, source, file, instructions, memory, on_error);
    return result;
}

void parse_and_build(
    ast::Pmr_Vector<ast::Content>& out,
    std::u8string_view source,
    File_Id file,
    std::pmr::memory_resource* memory,
    Parse_Error_Consumer on_error
)
{
    std::pmr::vector<AST_Instruction> instructions { memory };
    parse(instructions, source);
    build_ast(out, source, file, instructions, memory, on_error);
}

/// @brief Parses a document and runs `build_ast` on the results.
ast::Pmr_Vector<ast::Content> parse_and_build(
    std::u8string_view source,
    File_Id file,
    std::pmr::memory_resource* memory,
    Parse_Error_Consumer on_error
)
{
    std::pmr::vector<AST_Instruction> instructions { memory };
    parse(instructions, source);
    return build_ast(source, file, instructions, memory, on_error);
}

} // namespace cowel
