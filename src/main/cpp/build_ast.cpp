#include <algorithm>
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
#include "cowel/fwd.hpp"
#include "cowel/parse.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace ast {

[[nodiscard]]
Primary Primary::quoted_string(
    File_Source_Span source_span,
    std::u8string_view source,
    Pmr_Vector<Markup_Element>&& elements
)
{
    return Primary { Primary_Kind::quoted_string, source_span, source, std::move(elements) };
}

[[nodiscard]]
Primary Primary::block(
    File_Source_Span source_span,
    std::u8string_view source,
    Pmr_Vector<Markup_Element>&& elements
)
{
    return Primary { Primary_Kind::block, source_span, source, std::move(elements) };
}

[[nodiscard]]
Primary Primary::group(
    File_Source_Span source_span,
    std::u8string_view source,
    Pmr_Vector<Group_Member>&& members
)
{
    return Primary { Primary_Kind::group, source_span, source, std::move(members) };
}

Group_Member Group_Member::ellipsis(File_Source_Span source_span, std::u8string_view source)
{
    // clang-format off
    return {
        source_span,
        source,
        source_span.with_length(0),
        {},
        {},
        Member_Kind::ellipsis,
    };
    // clang-format on
}

Group_Member Group_Member::named(
    const File_Source_Span& name_span,
    std::u8string_view name,
    Member_Value&& value
)
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
Group_Member Group_Member::positional(Member_Value&& value)
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
    std::optional<Member_Value>&& value,
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
    std::optional<Primary>&& args,
    std::optional<Primary>&& content
)
    : m_source_span { source_span }
    , m_source { source }
    , m_name { name }
    , m_arguments { std::move(args) }
    , m_content { std::move(content) }
{
    COWEL_ASSERT(m_source_span.length == m_source.length());
    COWEL_ASSERT(!m_name.empty());
    COWEL_ASSERT(!m_name.starts_with(u8'\\'));
    COWEL_ASSERT(m_name.length() <= source_span.length);
    COWEL_ASSERT(!m_arguments || m_arguments->get_kind() == Primary_Kind::group);
    COWEL_ASSERT(!m_content || m_content->get_kind() == Primary_Kind::block);

    // this needs to be late-initialized here because it is declared after m_arguments
    m_has_ellipsis
        = m_arguments
        && std::ranges::contains(
              m_arguments->get_members(), ast::Member_Kind::ellipsis, &ast::Group_Member::get_kind
        );
}

void Primary::assert_validity() const
{
    COWEL_ASSERT(!m_source_span.empty());
    COWEL_ASSERT(m_source.length() == m_source_span.length);

    switch (m_kind) {
    case Primary_Kind::unit: {
        COWEL_ASSERT(m_source == u8"unit"sv);
        break;
    }
    case Primary_Kind::null: {
        COWEL_ASSERT(m_source == u8"null"sv);
        break;
    }
    case Primary_Kind::boolean: {
        COWEL_ASSERT(m_source == u8"true"sv || m_source == u8"false"sv);
        break;
    }
    case Primary_Kind::integer: {
        COWEL_ASSERT(is_ascii_digit(m_source.at(0 + std::size_t(m_source.starts_with(u8'-')))));
        break;
    }
    case Primary_Kind::floating_point:
    case Primary_Kind::unquoted_string:
    case Primary_Kind::text: {
        break;
    }
    case Primary_Kind::escape: {
        COWEL_ASSERT(m_source.length() >= 2);
        COWEL_ASSERT(m_source.starts_with('\\'));
        break;
    }
    case Primary_Kind::comment: {
        COWEL_ASSERT(m_source.length() >= 2);
        COWEL_ASSERT(m_source.starts_with(u8"\\:"));
        break;
    }
    case Primary_Kind::quoted_string: {
        COWEL_ASSERT(m_source.starts_with(u8'"'));
        COWEL_ASSERT(m_source.ends_with(u8'"'));
        break;
    }
    case Primary_Kind::block: {
        COWEL_ASSERT(m_source.starts_with(u8'{'));
        COWEL_ASSERT(m_source.ends_with(u8'}'));
        break;
    }
    case Primary_Kind::group: {
        COWEL_ASSERT(m_source.starts_with(u8'('));
        COWEL_ASSERT(m_source.ends_with(u8')'));
        break;
    }
    }
}

[[nodiscard]]
Primary::Primary(Primary_Kind kind, File_Source_Span source_span, std::u8string_view source)
    : m_kind { kind }
    , m_source_span { source_span }
    , m_source { source }
    , m_extra { kind != Primary_Kind::escape     ? 0uz
                    : source.ends_with(u8"\r\n") ? 2uz
                    : source.ends_with(u8'\n')   ? 1uz
                                                 : 0uz }
{
    assert_validity();
}

[[nodiscard]]
Primary::Primary(
    Primary_Kind kind,
    File_Source_Span source_span,
    std::u8string_view source,
    Pmr_Vector<Markup_Element>&& elements
)
    : m_kind { kind }
    , m_source_span { source_span }
    , m_source { source }
    , m_extra { std::move(elements) }
{
    assert_validity();
}

[[nodiscard]]
Primary::Primary(
    Primary_Kind kind,
    File_Source_Span source_span,
    std::u8string_view source,
    Pmr_Vector<Group_Member>&& members
)
    : m_kind { kind }
    , m_source_span { source_span }
    , m_source { source }
    , m_extra { std::move(members) }
{
    assert_validity();
}

} // namespace ast

namespace {

[[nodiscard]]
constexpr std::optional<ast::Primary_Kind> instruction_type_primary_kind(AST_Instruction_Type type)
{
    using enum AST_Instruction_Type;
    switch (type) {
    case escape: return ast::Primary_Kind::escape;
    case text: return ast::Primary_Kind::text;
    case unquoted_string: return ast::Primary_Kind::unquoted_string;
    case decimal_int_literal: return ast::Primary_Kind::integer;
    case float_literal: return ast::Primary_Kind::floating_point;
    case keyword_unit: return ast::Primary_Kind::unit;
    case keyword_null: return ast::Primary_Kind::null;
    case keyword_true:
    case keyword_false: return ast::Primary_Kind::boolean;
    case comment: return ast::Primary_Kind::comment;
    default: return {};
    }
}

struct [[nodiscard]] AST_Builder {
private:
    using char_type = char8_t;
    using string_view_type = std::u8string_view;

    const string_view_type m_source;
    const File_Id m_file;
    const std::span<const AST_Instruction> m_instructions;
    std::pmr::memory_resource* const m_memory;

    std::size_t m_index = 0;
    Source_Position m_pos {};

public:
    AST_Builder(
        string_view_type source,
        File_Id file,
        std::span<const AST_Instruction> instructions,
        std::pmr::memory_resource* memory
    )
        : m_source { source }
        , m_file { file }
        , m_instructions { instructions }
        , m_memory { memory }
    {
        COWEL_ASSERT(!instructions.empty());
    }

    void build_document(ast::Pmr_Vector<ast::Markup_Element>& out)
    {
        const AST_Instruction push_doc = pop();
        COWEL_ASSERT(push_doc.type == AST_Instruction_Type::push_document);
        out.clear();
        out.reserve(push_doc.n);

        for (std::size_t i = 0; i < push_doc.n; ++i) {
            append_markup_element(out);
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
        COWEL_DEBUG_ASSERT(m_pos.begin + n <= m_source.size());
        advance(m_pos, m_source.substr(m_pos.begin, n));
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

    void append_markup_element(ast::Pmr_Vector<ast::Markup_Element>& out)
    {
        const AST_Instruction instruction = peek();
        switch (instruction.type) {
            using enum AST_Instruction_Type;
        case escape:
        case text:
        case comment: {
            out.push_back(build_simple_primary());
            break;
        }
        case push_directive: {
            out.push_back(build_directive(Directive_Context::markup));
            break;
        }
        default: COWEL_ASSERT_UNREACHABLE(u8"Invalid markup element instruction.");
        }
    }

    [[nodiscard]]
    ast::Primary build_simple_primary()
    {
        const AST_Instruction instruction = pop();
        const std::optional<ast::Primary_Kind> kind
            = instruction_type_primary_kind(instruction.type);
        COWEL_ASSERT(kind);

        const File_Source_Span span { m_pos, instruction.n, m_file };
        ast::Primary result { *kind, span, extract(span) };
        advance_by(instruction.n);
        return result;
    }

    enum struct Directive_Context : bool {
        markup,
        member_value,
    };

    [[nodiscard]]
    ast::Directive build_directive(Directive_Context context)
    {
        const std::size_t name_advancement = context == Directive_Context::markup ? 1 : 0;

        const AST_Instruction instruction = pop();
        COWEL_ASSERT(instruction.type == AST_Instruction_Type::push_directive);
        COWEL_ASSERT(instruction.n >= name_advancement + 1);

        const Source_Position initial_pos = m_pos;
        const std::size_t name_length = instruction.n - name_advancement;
        advance_by(instruction.n);

        if (context == Directive_Context::member_value) {
            ignore_skips();
        }
        std::optional<ast::Primary> arguments = try_build_group();

        if (context == Directive_Context::member_value) {
            ignore_skips();
        }
        std::optional<ast::Primary> block = try_build_block();

        const AST_Instruction pop_instruction = pop();
        COWEL_ASSERT(pop_instruction.type == AST_Instruction_Type::pop_directive);

        const File_Source_Span source_span { initial_pos, m_pos.begin - initial_pos.begin, m_file };
        const File_Source_Span name_span
            = source_span.to_right(name_advancement).with_length(name_length);
        const std::u8string_view name = extract(name_span);

        return { source_span, extract(source_span), name, std::move(arguments), std::move(block) };
    }

    [[nodiscard]]
    std::optional<ast::Primary> try_build_group()
    {
        if (eof()) {
            {};
        }
        const AST_Instruction instruction = peek();
        if (instruction.type != AST_Instruction_Type::push_group) {
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
            if (next.type == AST_Instruction_Type::member_comma) {
                COWEL_DEBUG_ASSERT(m_source[m_pos.begin] == u8',');
                advance_by(1);
                pop();
                continue;
            }
            if (next.type == AST_Instruction_Type::member_equal) {
                COWEL_DEBUG_ASSERT(m_source[m_pos.begin] == u8'=');
                advance_by(1);
                pop();
                continue;
            }
            if (next.type == AST_Instruction_Type::pop_group) {
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
        return ast::Primary::group(source_span, extract(source_span), std::move(members));
    }

    [[nodiscard]]
    ast::Group_Member build_group_member()
    {
        const AST_Instruction instruction = pop();
        ignore_skips();

        const Source_Position initial_pos = m_pos;

        switch (instruction.type) {
        case AST_Instruction_Type::push_named_member: {
            const AST_Instruction name = pop();
            COWEL_ASSERT(name.type == AST_Instruction_Type::member_name);
            advance_by(name.n);
            const File_Source_Span name_span { initial_pos, name.n, m_file };
            ignore_skips();

            const AST_Instruction equal = pop();
            COWEL_ASSERT(equal.type == AST_Instruction_Type::member_equal);
            advance_by(1);
            ignore_skips();

            ast::Member_Value value = build_member_value();
            ignore_skips();
            const auto pop_instruction = pop();
            COWEL_ASSERT(pop_instruction.type == AST_Instruction_Type::pop_named_member);
            return ast::Group_Member::named(name_span, extract(name_span), std::move(value));
        }

        case AST_Instruction_Type::push_positional_member: {
            ast::Member_Value value = build_member_value();
            ignore_skips();
            const auto pop_instruction = pop();
            COWEL_ASSERT(pop_instruction.type == AST_Instruction_Type::pop_positional_member);
            return ast::Group_Member::positional(std::move(value));
        }

        case AST_Instruction_Type::push_ellipsis_argument: {
            std::optional<File_Source_Span> source_span;
            while (true) {
                const AST_Instruction instruction = pop();
                switch (instruction.type) {
                case AST_Instruction_Type::pop_ellipsis_argument: {
                    goto done;
                }
                case AST_Instruction_Type::ellipsis: {
                    source_span = File_Source_Span { m_pos, instruction.n, m_file };
                    advance_by(instruction.n);
                    break;
                }
                case AST_Instruction_Type::skip: {
                    advance_by(instruction.n);
                    break;
                }
                default: {
                    COWEL_ASSERT_UNREACHABLE(u8"Unexpected instruction inside ellipsis argument.");
                }
                }
            }
        done:
            // ellipsis argument without ellipsis instructin inside?!
            COWEL_ASSERT(source_span);
            return ast::Group_Member::ellipsis(*source_span, extract(*source_span));
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
    ast::Member_Value build_member_value()
    {
        const AST_Instruction instruction = peek();
        switch (instruction.type) {
        case AST_Instruction_Type::keyword_null:
        case AST_Instruction_Type::keyword_true:
        case AST_Instruction_Type::keyword_false:
        case AST_Instruction_Type::unquoted_string:
        case AST_Instruction_Type::decimal_int_literal:
        case AST_Instruction_Type::float_literal: {
            return build_simple_primary();
        }
        case AST_Instruction_Type::push_group: {
            return try_build_group().value();
        }
        case AST_Instruction_Type::push_block: {
            return try_build_block().value();
        }
        case AST_Instruction_Type::push_quoted_string: {
            return try_build_quoted_string().value();
        }
        case AST_Instruction_Type::push_directive: {
            return build_directive(Directive_Context::member_value);
        }
        default: break;
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid member value.");
    }

    [[nodiscard]]
    std::optional<ast::Primary> try_build_block()
    {
        return try_build_block_or_string(
            AST_Instruction_Type::push_block, AST_Instruction_Type::pop_block
        );
    }

    [[nodiscard]]
    std::optional<ast::Primary> try_build_quoted_string()
    {
        return try_build_block_or_string(
            AST_Instruction_Type::push_quoted_string, AST_Instruction_Type::pop_quoted_string
        );
    }

    [[nodiscard]]
    std::optional<ast::Primary>
    try_build_block_or_string(AST_Instruction_Type push_type, AST_Instruction_Type pop_type)
    {
        COWEL_ASSERT(
            push_type == AST_Instruction_Type::push_block
            || push_type == AST_Instruction_Type::push_quoted_string
        );
        COWEL_ASSERT(
            pop_type == AST_Instruction_Type::pop_block
            || pop_type == AST_Instruction_Type::pop_quoted_string
        );

        if (eof()) {
            return {};
        }
        const AST_Instruction instruction = peek();
        if (instruction.type != push_type) {
            return {};
        }

        const Source_Position initial_pos = m_pos;
        pop();
        advance_by(1);

        ast::Pmr_Vector<ast::Markup_Element> content { m_memory };

        while (!eof()) {
            const AST_Instruction next = peek();
            if (next.type == pop_type) {
                pop();
                break;
            }
            append_markup_element(content);
        }
        advance_by(1);

        const File_Source_Span source_span {
            initial_pos,
            m_pos.begin - initial_pos.begin,
            m_file,
        };
        return push_type == AST_Instruction_Type::push_block
            ? ast::Primary::block(source_span, extract(source_span), std::move(content))
            : ast::Primary::quoted_string(source_span, extract(source_span), std::move(content));
    }
};

} // namespace

void build_ast(
    ast::Pmr_Vector<ast::Markup_Element>& out,
    std::u8string_view source,
    File_Id file,
    std::span<const AST_Instruction> instructions,
    std::pmr::memory_resource* memory
)
{
    AST_Builder { source, file, instructions, memory }.build_document(out);
}

ast::Pmr_Vector<ast::Markup_Element> build_ast(
    std::u8string_view source,
    File_Id file,
    std::span<const AST_Instruction> instructions,
    std::pmr::memory_resource* memory
)
{
    ast::Pmr_Vector<ast::Markup_Element> result { memory };
    build_ast(result, source, file, instructions, memory);
    return result;
}

bool parse_and_build(
    ast::Pmr_Vector<ast::Markup_Element>& out,
    std::u8string_view source,
    File_Id file,
    std::pmr::memory_resource* memory,
    Parse_Error_Consumer on_error
)
{
    std::pmr::vector<AST_Instruction> instructions { memory };
    if (parse(instructions, source, on_error)) {
        build_ast(out, source, file, instructions, memory);
        return true;
    }
    return false;
}

} // namespace cowel
