#include <cstddef>
#include <memory_resource>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "mmml/util/annotation_span.hpp"
#include "mmml/util/assert.hpp"
#include "mmml/util/hljs_scope.hpp"
#include "mmml/util/source_position.hpp"

#include "mmml/ast.hpp"
#include "mmml/parse.hpp"

namespace mmml {

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
    const std::span<const AST_Instruction> m_instructions;
    std::pmr::memory_resource* const m_memory;

    std::size_t m_index = 0;
    Source_Position m_pos {};

public:
    AST_Builder(
        string_view_type source,
        std::span<const AST_Instruction> instructions,
        std::pmr::memory_resource* memory
    )
        : m_source { source }
        , m_instructions { instructions }
        , m_memory { memory }
    {
        MMML_ASSERT(!instructions.empty());
    }

    void advance_by(std::size_t n)
    {
        MMML_ASSERT(m_pos.begin + n <= m_source.size());

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
        MMML_ASSERT(m_index < m_instructions.size());
        return m_instructions[m_index];
    }

    AST_Instruction pop()
    {
        MMML_ASSERT(m_index < m_instructions.size());
        return m_instructions[m_index++];
    }

    [[nodiscard]]
    std::pmr::vector<ast::Content> build_document()
    {
        const AST_Instruction push_doc = pop();
        MMML_ASSERT(push_doc.type == AST_Instruction_Type::push_document);

        std::pmr::vector<ast::Content> result { m_memory };
        result.reserve(push_doc.n);
        for (std::size_t i = 0; i < push_doc.n; ++i) {
            append_content(result);
        }

        return result;
    }

    void append_content(std::pmr::vector<ast::Content>& out)
    {
        const AST_Instruction instruction = peek();
        switch (instruction.type) {
            using enum AST_Instruction_Type;
        case skip: //
            advance_by(instruction.n);
            pop();
            break;
        case argument_comma:
        case argument_equal: //
            advance_by(1);
            pop();
            break;
        case escape: //
            out.push_back(build_escape());
            break;
        case text: //
            out.push_back(build_text());
            break;
        case push_directive: //
            out.push_back(build_directive());
            break;

        default: MMML_ASSERT_UNREACHABLE(u8"Invalid content creating instruction.");
        }
    }

    [[nodiscard]]
    ast::Escaped build_escape()
    {
        const AST_Instruction instruction = pop();
        MMML_ASSERT(instruction.type == AST_Instruction_Type::escape);

        ast::Escaped result { Source_Span { m_pos, instruction.n } };
        advance_by(instruction.n);
        return result;
    }

    [[nodiscard]]
    ast::Text build_text()
    {
        const AST_Instruction instruction = pop();
        MMML_ASSERT(instruction.type == AST_Instruction_Type::text);

        ast::Text result { Source_Span { m_pos, instruction.n } };
        advance_by(instruction.n);
        return result;
    }

    [[nodiscard]]
    ast::Directive build_directive()
    {
        const AST_Instruction instruction = pop();
        MMML_ASSERT(instruction.type == AST_Instruction_Type::push_directive);
        MMML_ASSERT(instruction.n >= 2);

        const Source_Position initial_pos = m_pos;
        const std::size_t name_length = instruction.n - 1;
        advance_by(instruction.n);

        std::pmr::vector<ast::Argument> arguments { m_memory };
        try_append_argument_sequence(arguments);

        std::pmr::vector<ast::Content> block { m_memory };
        try_append_block(block);

        const AST_Instruction pop_instruction = pop();
        MMML_ASSERT(pop_instruction.type == AST_Instruction_Type::pop_directive);

        const Source_Span span { initial_pos, m_pos.begin - initial_pos.begin };
        return { span, name_length, std::move(arguments), std::move(block) };
    }

    void try_append_argument_sequence(std::pmr::vector<ast::Argument>& out)
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

    void append_argument(std::pmr::vector<ast::Argument>& out)
    {
        const AST_Instruction instruction = pop();
        MMML_ASSERT(instruction.type == AST_Instruction_Type::push_argument);

        const Source_Position initial_pos = m_pos;
        std::optional<Source_Span> name;

        std::pmr::vector<ast::Content> children { m_memory };
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

        Source_Span span { initial_pos, m_pos.begin - initial_pos.begin };
        out.push_back(
            name ? ast::Argument { span, *name, std::move(children) }
                 : ast::Argument { span, std::move(children) }
        );
    }

    void try_append_block(std::pmr::vector<ast::Content>& out)
    {
        if (eof()) {
            return;
        }
        const AST_Instruction instruction = peek();
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

std::pmr::vector<ast::Content> build_ast(
    std::u8string_view source,
    std::span<const AST_Instruction> instructions,
    std::pmr::memory_resource* memory
)
{
    return AST_Builder { source, instructions, memory }.build_document();
}

void build_highlight(
    std::pmr::vector<Annotation_Span<HLJS_Scope>>& out,
    std::span<const AST_Instruction> instructions
)
{
    std::size_t index = 0;
    const auto emit = [&](std::size_t length, HLJS_Scope scope) {
        out.push_back({ .begin = index, .length = length, .value = scope });
        index += length;
    };

    for (const auto& i : instructions) {
        switch (i.type) {
            using enum AST_Instruction_Type;
        case skip: //
            index += i.n;
            break;
        case escape: //
            emit(i.n, HLJS_Scope::char_escape);
            break;
        case text: //
            index += i.n;
            break;
        case argument_name: //
            emit(i.n, HLJS_Scope::attribute);
            break;
        case push_directive: //
            emit(i.n, HLJS_Scope::tag);
            break;

        case argument_equal: // =
        case argument_comma: // ,
        case push_arguments: // [
        case pop_arguments: // ]
        case push_block: // {
        case pop_block: // }
            emit(i.n, HLJS_Scope::punctuation);
            break;

        case push_document:
        case pop_document:
        case pop_directive:
        case push_argument:
        case pop_argument: break;
        }
    }
}

/// @brief Parses a document and runs `build_ast` on the results.
std::pmr::vector<ast::Content>
parse_and_build(std::u8string_view source, std::pmr::memory_resource* memory)
{
    std::pmr::vector<AST_Instruction> instructions { memory };
    parse(instructions, source);
    return build_ast(source, instructions, memory);
}

} // namespace mmml
