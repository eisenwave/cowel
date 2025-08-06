#include <cstddef>
#include <memory_resource>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/source_position.hpp"

#include "cowel/ast.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/fwd.hpp"
#include "cowel/parse.hpp"

namespace cowel {

namespace ast {

Argument::Argument(
    const File_Source_Span& source_span,
    std::u8string_view source,
    const File_Source_Span& name_span,
    std::u8string_view name,
    Pmr_Vector<ast::Content>&& children
)
    : m_source_span { source_span }
    , m_source { source }
    , m_content { std::move(children) }
    , m_name_span { name_span }
    , m_name { name }
{
    COWEL_ASSERT(m_source_span.length == m_source.length());
    COWEL_ASSERT(m_name_span.length == m_name.length());
}

[[nodiscard]]
Argument::Argument(
    const File_Source_Span& source_span,
    std::u8string_view source,
    Pmr_Vector<ast::Content>&& children
)
    : m_source_span { source_span }
    , m_source { source }
    , m_content { std::move(children) }
    , m_name_span { source_span.with_length(0) } // NOLINT(cppcoreguidelines-slicing)
    , m_name {}
{
    COWEL_ASSERT(m_source_span.length == m_source.length());
}

Directive::Directive(
    const File_Source_Span& source_span,
    std::u8string_view source,
    std::u8string_view name,
    Pmr_Vector<Argument>&& args,
    Pmr_Vector<Content>&& block
)
    : m_source_span { source_span }
    , m_source { source }
    , m_name { name }
    , m_arguments { std::move(args) }
    , m_content { std::move(block) }
{
    COWEL_ASSERT(m_source_span.length == m_source.length());
    COWEL_ASSERT(!name.empty());
    COWEL_ASSERT(!name.starts_with(u8'\\'));
    COWEL_ASSERT(name.length() <= source_span.length);
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

        ast::Pmr_Vector<ast::Argument> arguments { m_memory };
        try_append_argument_sequence(arguments);

        ast::Pmr_Vector<ast::Content> block { m_memory };
        try_append_block(block);

        const AST_Instruction pop_instruction = pop();
        COWEL_ASSERT(pop_instruction.type == AST_Instruction_Type::pop_directive);

        const File_Source_Span source_span { initial_pos, m_pos.begin - initial_pos.begin, m_file };
        const File_Source_Span name_span = source_span.to_right(1).with_length(name_length);
        const std::u8string_view name = extract(name_span);

        return { source_span, extract(source_span), name, std::move(arguments), std::move(block) };
    }

    void try_append_argument_sequence(ast::Pmr_Vector<ast::Argument>& out)
    {
        if (eof()) {
            return;
        }
        const AST_Instruction instruction = peek();
        if (instruction.type != AST_Instruction_Type::push_arguments) {
            return;
        }
        pop();
        advance_by(1);

        while (!eof()) {
            const AST_Instruction next = peek();
            if (next.type == AST_Instruction_Type::skip) {
                advance_by(next.n);
                pop();
                continue;
            }
            if (next.type == AST_Instruction_Type::argument_comma
                || next.type == AST_Instruction_Type::argument_equal) {
                advance_by(1);
                pop();
                continue;
            }
            if (next.type == AST_Instruction_Type::pop_arguments) { // ]
                pop();
                break;
            }
            append_argument(out);
        }
        advance_by(1);
    }

    void append_argument(ast::Pmr_Vector<ast::Argument>& out)
    {
        const AST_Instruction instruction = pop();
        COWEL_ASSERT(instruction.type == AST_Instruction_Type::push_argument);

        const Source_Position initial_pos = m_pos;
        std::optional<Source_Span> name;

        ast::Pmr_Vector<ast::Content> children { m_memory };
        children.reserve(instruction.n);

        while (!eof()) {
            const AST_Instruction next = peek();
            if (next.type == AST_Instruction_Type::pop_argument) {
                pop();
                break;
            }
            if (next.type == AST_Instruction_Type::argument_name) {
                pop();
                name = { m_pos, next.n };
                advance_by(next.n);
                continue;
            }
            append_content(children);
        }

        const File_Source_Span source_span { initial_pos, m_pos.begin - initial_pos.begin, m_file };
        const std::u8string_view source = extract(source_span);
        if (name) {
            const File_Source_Span name_span { *name, m_file };
            out.push_back(ast::Argument {
                source_span, source, { *name, m_file }, extract(*name), std::move(children) });
        }
        else {
            out.push_back(ast::Argument { source_span, source, std::move(children) });
        }
    }

    void try_append_block(ast::Pmr_Vector<ast::Content>& out)
    {
        if (eof()) {
            return;
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
            return;
        }
        if (instruction.type != AST_Instruction_Type::push_block) {
            return;
        }
        pop();
        advance_by(1);

        while (!eof()) {
            const AST_Instruction next = peek();
            if (next.type == AST_Instruction_Type::pop_block) {
                pop();
                break;
            }
            append_content(out);
        }
        advance_by(1);
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
