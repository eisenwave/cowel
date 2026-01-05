#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory_resource>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cowel/lex.hpp"
#include "cowel/util/assert.hpp"
#include "cowel/util/from_chars.hpp"
#include "cowel/util/html_names.hpp"
#include "cowel/util/source_position.hpp"

#include "cowel/ast.hpp"
#include "cowel/fwd.hpp"
#include "cowel/parse.hpp"
#include "cowel/string_kind.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace ast {

Primary Primary::basic(Primary_Kind kind, File_Source_Span source_span, std::u8string_view source)
{
    COWEL_DEBUG_ASSERT(!source.empty());
    using enum ast::Primary_Kind;
    switch (kind) {
    case unit_literal:
    case null_literal:
    case bool_literal:
    case text: return Primary { kind, source_span, source, std::monostate {} };

    case unquoted_string: {
        return Primary { kind, source_span, source, std::monostate {}, String_Kind::ascii };
    }

    case int_literal: return integer(source_span, source);
    case decimal_float_literal: return floating(source_span, source);

    case infinity: {
        const Float value = source.starts_with(u8'-') ? -std::numeric_limits<Float>::infinity()
                                                      : std::numeric_limits<Float>::infinity();
        return Primary { kind, source_span, source,
                         Parsed_Float { value, Float_Literal_Status::ok } };
    }

    case comment:
    case escape: {
        const std::size_t length = source.ends_with(u8"\r\n") ? 2uz
            : source.ends_with(u8'\n')                        ? 1uz
                                                              : 0uz;
        return Primary { kind, source_span, source, length };
    }

    case quoted_string:
    case block:
    case group: break;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Kind is not basic basic.");
}

Primary Primary::integer(File_Source_Span source_span, std::u8string_view source)
{
    COWEL_DEBUG_ASSERT(!source.empty() && (source[0] == u8'-' || is_ascii_digit(source[0])));
    COWEL_ASSERT(!source.empty());
    const bool is_negative = source.starts_with(u8'-');
    const auto base_id_index = std::size_t(1 + is_negative);
    const bool is_decimal
        = source.length() < std::size_t(2 + is_negative) || is_ascii_digit(source[base_id_index]);
    const int base = is_decimal          ? 10
        : source[base_id_index] == u8'b' ? 2
        : source[base_id_index] == u8'o' ? 8
        : source[base_id_index] == u8'x' ? 16
                                         : 0;
    COWEL_ASSERT(base != 0);
    COWEL_ASSERT(is_decimal || source.length() > 2);
    COWEL_ASSERT(is_decimal || source[std::size_t(is_negative)] == u8'0');

    Integer value = 0;
    const auto result = [&] -> std::from_chars_result {
        if (is_decimal) {
            return from_characters(source, value, base);
        }
        const std::size_t digits_start = is_negative ? 3 : 2;
        const std::u8string_view digits = source.substr(digits_start);
        if (!is_negative) {
            return from_characters(digits, value, base);
        }
        Uint128 v = 0;
        const auto r = from_characters(digits, v, base);
        if (r.ec != std::errc {}) {
            return r;
        }
        constexpr auto max_u128 = Uint128 { 1 } << 127;
        if (v > max_u128) {
            return { r.ptr, std::errc::result_out_of_range };
        }
        value = Int128(-v);
        return r;
    }();
    COWEL_ASSERT(result.ec != std::errc::invalid_argument);

    const auto parsed_int = result.ec == std::errc {}
        ? Parsed_Int { .value = value, .in_range = true }
        : Parsed_Int { .value = 0, .in_range = false };
    return Primary { Primary_Kind::int_literal, source_span, source, parsed_int };
}

Primary Primary::floating(File_Source_Span source_span, const std::u8string_view source)
{
    Float value = 0;
    const std::from_chars_result result = from_characters(source, value);
    COWEL_ASSERT(result.ec != std::errc::invalid_argument);
    COWEL_DEBUG_ASSERT(!std::isnan(value));

    const auto parsed_float = [&] -> Parsed_Float {
        if constexpr (Standard_Library::current == Standard_Library::libcxx) {
            const auto status = result.ec == std::errc {} ? Float_Literal_Status::ok
                : std::isinf(value)                       ? Float_Literal_Status::float_overflow
                                                          : Float_Literal_Status::float_underflow;
            return { .value = value, .status = status };
        }
        else {
            if (result.ec == std::errc {}) {
                return { .value = value, .status = Float_Literal_Status::ok };
            }
            const auto str = std::u8string { source };
            const double error_value
                = std::strtod(reinterpret_cast<const char*>(str.c_str()), nullptr);
            const auto status = std::isinf(value) ? Float_Literal_Status::float_overflow
                                                  : Float_Literal_Status::float_underflow;
            return { .value = error_value, .status = status };
        }
    }();
    return Primary { Primary_Kind::decimal_float_literal, source_span, source, parsed_float };
}

Primary Primary::quoted_string(
    File_Source_Span source_span,
    std::u8string_view source,
    Pmr_Vector<Markup_Element>&& elements
)
{
    COWEL_DEBUG_ASSERT(source.starts_with(u8'"'));
    COWEL_DEBUG_ASSERT(source.ends_with(u8'"'));
    return Primary { Primary_Kind::quoted_string, source_span, source, std::move(elements) };
}

Primary Primary::block(
    File_Source_Span source_span,
    std::u8string_view source,
    Pmr_Vector<Markup_Element>&& elements
)
{
    COWEL_DEBUG_ASSERT(source.starts_with(u8'{'));
    COWEL_DEBUG_ASSERT(source.ends_with(u8'}'));
    return Primary { Primary_Kind::block, source_span, source, std::move(elements) };
}

Primary Primary::group(
    File_Source_Span source_span,
    std::u8string_view source,
    Pmr_Vector<Group_Member>&& members
)
{
    COWEL_DEBUG_ASSERT(source.starts_with(u8'('));
    COWEL_DEBUG_ASSERT(source.ends_with(u8')'));
    return Primary { Primary_Kind::group, source_span, source, std::move(members) };
}

Group_Member Group_Member::ellipsis(File_Source_Span source_span, std::u8string_view source)
{
    COWEL_DEBUG_ASSERT(source == u8"...");
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
    case Primary_Kind::unit_literal: {
        COWEL_ASSERT(m_source == u8"unit"sv);
        break;
    }
    case Primary_Kind::null_literal: {
        COWEL_ASSERT(m_source == u8"null"sv);
        break;
    }
    case Primary_Kind::bool_literal: {
        COWEL_ASSERT(m_source == u8"true"sv || m_source == u8"false"sv);
        break;
    }
    case Primary_Kind::infinity: {
        COWEL_ASSERT(m_source == u8"-infinity"sv || m_source == u8"infinity"sv);
        break;
    }
    case Primary_Kind::int_literal: {
        COWEL_ASSERT(is_ascii_digit(m_source.at(0 + std::size_t(m_source.starts_with(u8'-')))));
        break;
    }
    case Primary_Kind::decimal_float_literal:
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
        COWEL_ASSERT(
            m_source.starts_with(u8"\\:")
            || (m_source.starts_with(u8"\\*") && m_source.ends_with(u8"*\\"))
        );
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

Primary::Primary(
    Primary_Kind kind,
    File_Source_Span source_span,
    std::u8string_view source,
    Extra_Variant&& extra,
    String_Kind string_kind
)
    : m_kind { kind }
    , m_string_kind { string_kind }
    , m_source_span { source_span }
    , m_source { source }
    , m_extra { std::move(extra) }
{
    assert_validity();
}

} // namespace ast

namespace {

[[nodiscard]]
constexpr std::optional<ast::Primary_Kind> instruction_type_primary_kind(CST_Instruction_Kind type)
{
    using enum CST_Instruction_Kind;
    switch (type) {
    case escape: return ast::Primary_Kind::escape;
    case text: return ast::Primary_Kind::text;
    case unquoted_string: return ast::Primary_Kind::unquoted_string;
    case binary_int:
    case octal_int:
    case decimal_int:
    case hexadecimal_int: return ast::Primary_Kind::int_literal;
    case decimal_float: return ast::Primary_Kind::decimal_float_literal;
    case keyword_unit: return ast::Primary_Kind::unit_literal;
    case keyword_null: return ast::Primary_Kind::null_literal;
    case keyword_true:
    case keyword_false: return ast::Primary_Kind::bool_literal;
    case keyword_infinity:
    case keyword_neg_infinity: return ast::Primary_Kind::infinity;
    case line_comment:
    case block_comment: return ast::Primary_Kind::comment;
    default: return {};
    }
}

struct [[nodiscard]] AST_Builder {
private:
    using char_type = char8_t;
    using string_view_type = std::u8string_view;

    const string_view_type m_source;
    const File_Id m_file;
    const std::span<const Token> m_tokens;
    const std::span<const CST_Instruction> m_instructions;
    std::pmr::memory_resource* const m_memory;

    std::size_t m_token_index = 0;
    std::size_t m_instruction_index = 0;

public:
    AST_Builder(
        string_view_type source,
        File_Id file,
        std::span<const Token> tokens,
        std::span<const CST_Instruction> instructions,
        std::pmr::memory_resource* memory
    )
        : m_source { source }
        , m_file { file }
        , m_tokens { tokens }
        , m_instructions { instructions }
        , m_memory { memory }
    {
        COWEL_ASSERT(!instructions.empty());
    }

    void build_document(ast::Pmr_Vector<ast::Markup_Element>& out)
    {
        const CST_Instruction push_doc = pop_instruction();
        COWEL_ASSERT(push_doc.kind == CST_Instruction_Kind::push_document);
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

    void advance_by_tokens(std::size_t n)
    {
        COWEL_ASSERT(m_token_index + n <= m_tokens.size());
        m_token_index += n;
    }

    [[nodiscard]]
    bool eof() const
    {
        return m_instruction_index == m_instructions.size();
    }

    [[nodiscard]]
    Token peek_token() const
    {
        COWEL_ASSERT(m_token_index < m_tokens.size());
        return m_tokens[m_token_index];
    }

    [[nodiscard]]
    CST_Instruction peek_instruction() const
    {
        COWEL_ASSERT(m_instruction_index < m_instructions.size());
        const auto result = m_instructions[m_instruction_index];
        if constexpr (is_debug_build) {
            const Token_Kind expected_token = cst_instruction_kind_fixed_token(result.kind);
            COWEL_ASSERT(
                expected_token == Token_Kind::error || expected_token == peek_token().kind
            );
        }
        return result;
    }

    CST_Instruction pop_instruction()
    {
        const auto result = peek_instruction();
        m_instruction_index++;
        return result;
    }

    void append_markup_element(ast::Pmr_Vector<ast::Markup_Element>& out)
    {
        const CST_Instruction instruction = peek_instruction();
        switch (instruction.kind) {
            using enum CST_Instruction_Kind;
        case escape:
        case text:
        case line_comment:
        case block_comment: {
            out.push_back(build_simple_primary());
            break;
        }
        case push_directive_splice: {
            out.push_back(build_directive(Directive_Kind::splice));
            break;
        }
        default: COWEL_ASSERT_UNREACHABLE(u8"Invalid markup element instruction.");
        }
    }

    [[nodiscard]]
    ast::Primary build_simple_primary()
    {
        const CST_Instruction instruction = pop_instruction();
        const std::optional<ast::Primary_Kind> kind
            = instruction_type_primary_kind(instruction.kind);
        COWEL_ASSERT(kind);

        const File_Source_Span span { peek_token().location, m_file };
        auto result = ast::Primary::basic(*kind, span, extract(span));
        COWEL_DEBUG_ASSERT(cst_instruction_kind_advances(instruction.kind));
        advance_by_tokens(1);
        return result;
    }

    enum struct Directive_Kind : bool {
        splice,
        call,
    };

    [[nodiscard]]
    ast::Directive build_directive(const Directive_Kind kind)
    {
        const CST_Instruction instruction = pop_instruction();
        COWEL_ASSERT(
            (kind == Directive_Kind::splice
             && instruction.kind == CST_Instruction_Kind::push_directive_splice)
            || (kind == Directive_Kind::call
                && instruction.kind == CST_Instruction_Kind::push_directive_call)
        );

        const auto initial_pos = peek_token().location;
        advance_by_tokens(1);

        if (kind == Directive_Kind::call) {
            ignore_skips();
        }
        std::optional<ast::Primary> arguments = try_build_group();

        if (kind == Directive_Kind::call) {
            ignore_skips();
        }
        std::optional<ast::Primary> block = try_build_block();

        const CST_Instruction pop_instruction = this->pop_instruction();
        COWEL_ASSERT(
            (kind == Directive_Kind::splice
             && pop_instruction.kind == CST_Instruction_Kind::pop_directive_splice)
            || (kind == Directive_Kind::call
                && pop_instruction.kind == CST_Instruction_Kind::pop_directive_call)
        );

        const File_Source_Span raw_name_span = { initial_pos, m_file };
        const auto name_span = [&] {
            auto result = raw_name_span;
            if (kind == Directive_Kind::splice) {
                // The leading backslash of a directive splice should not be part of the nam.e
                result.begin += 1;
                result.column += 1;
                result.length -= 1;
            }
            return result;
        }();
        const std::u8string_view name = extract(name_span);
        const std::size_t source_length = peek_token().location.begin - initial_pos.begin;
        const auto source_span = raw_name_span.with_length(source_length);
        const std::u8string_view source = extract(source_span);

        return { source_span, source, name, std::move(arguments), std::move(block) };
    }

    [[nodiscard]]
    std::optional<ast::Primary> try_build_group()
    {
        if (eof()) {
            {};
        }
        const CST_Instruction instruction = peek_instruction();
        if (instruction.kind != CST_Instruction_Kind::push_group) {
            return {};
        }

        const auto initial_pos = peek_token().location;
        pop_instruction();
        advance_by_tokens(1);

        ast::Pmr_Vector<ast::Group_Member> members { m_memory };

        while (!eof()) {
            const CST_Instruction next = peek_instruction();
            if (next.kind == CST_Instruction_Kind::skip) {
                pop_instruction();
                advance_by_tokens(1);
                continue;
            }
            if (next.kind == CST_Instruction_Kind::comma) {
                pop_instruction();
                advance_by_tokens(1);
                continue;
            }
            if (next.kind == CST_Instruction_Kind::equals) {
                pop_instruction();
                advance_by_tokens(1);
                continue;
            }
            if (next.kind == CST_Instruction_Kind::pop_group) {
                pop_instruction();
                advance_by_tokens(1);
                break;
            }
            members.push_back(build_group_member());
        }

        const File_Source_Span source_span {
            initial_pos,
            peek_token().location.begin - initial_pos.begin,
            m_file,
        };
        return ast::Primary::group(source_span, extract(source_span), std::move(members));
    }

    [[nodiscard]]
    ast::Group_Member build_group_member()
    {
        const CST_Instruction instruction = pop_instruction();
        ignore_skips();

        const auto initial_pos = peek_token().location;

        switch (instruction.kind) {
        case CST_Instruction_Kind::push_named_member: {
            const CST_Instruction name = pop_instruction();
            COWEL_ASSERT(name.kind == CST_Instruction_Kind::member_name);
            advance_by_tokens(1);
            const File_Source_Span name_span { initial_pos, m_file };
            ignore_skips();

            const CST_Instruction equal = pop_instruction();
            COWEL_ASSERT(equal.kind == CST_Instruction_Kind::equals);
            advance_by_tokens(1);
            ignore_skips();

            ast::Member_Value value = build_member_value();
            ignore_skips();
            const auto pop_instruction = this->pop_instruction();
            COWEL_ASSERT(pop_instruction.kind == CST_Instruction_Kind::pop_named_member);
            return ast::Group_Member::named(name_span, extract(name_span), std::move(value));
        }

        case CST_Instruction_Kind::push_positional_member: {
            ast::Member_Value value = build_member_value();
            ignore_skips();
            const auto pop_instruction = this->pop_instruction();
            COWEL_ASSERT(pop_instruction.kind == CST_Instruction_Kind::pop_positional_member);
            return ast::Group_Member::positional(std::move(value));
        }

        case CST_Instruction_Kind::push_ellipsis_argument: {
            std::optional<File_Source_Span> source_span;
            while (true) {
                const CST_Instruction instruction = pop_instruction();
                switch (instruction.kind) {
                case CST_Instruction_Kind::pop_ellipsis_argument: {
                    goto done;
                }
                case CST_Instruction_Kind::ellipsis: {
                    source_span = File_Source_Span { peek_token().location, m_file };
                    advance_by_tokens(1);
                    break;
                }
                case CST_Instruction_Kind::skip: {
                    advance_by_tokens(1);
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
        while (!eof() && peek_instruction().kind == CST_Instruction_Kind::skip) {
            pop_instruction();
            advance_by_tokens(1);
        }
    }

    [[nodiscard]]
    ast::Member_Value build_member_value()
    {
        const CST_Instruction instruction = peek_instruction();
        switch (instruction.kind) {
        case CST_Instruction_Kind::keyword_null:
        case CST_Instruction_Kind::keyword_unit:
        case CST_Instruction_Kind::keyword_true:
        case CST_Instruction_Kind::keyword_false:
        case CST_Instruction_Kind::keyword_infinity:
        case CST_Instruction_Kind::keyword_neg_infinity:
        case CST_Instruction_Kind::unquoted_string:
        case CST_Instruction_Kind::binary_int:
        case CST_Instruction_Kind::octal_int:
        case CST_Instruction_Kind::decimal_int:
        case CST_Instruction_Kind::hexadecimal_int:
        case CST_Instruction_Kind::decimal_float: {
            return build_simple_primary();
        }
        case CST_Instruction_Kind::push_group: {
            return try_build_group().value();
        }
        case CST_Instruction_Kind::push_block: {
            return try_build_block().value();
        }
        case CST_Instruction_Kind::push_quoted_string: {
            return try_build_quoted_string().value();
        }
        case CST_Instruction_Kind::push_directive_call: {
            return build_directive(Directive_Kind::call);
        }
        default: break;
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid member value.");
    }

    [[nodiscard]]
    std::optional<ast::Primary> try_build_block()
    {
        return try_build_block_or_string(
            CST_Instruction_Kind::push_block, CST_Instruction_Kind::pop_block
        );
    }

    [[nodiscard]]
    std::optional<ast::Primary> try_build_quoted_string()
    {
        return try_build_block_or_string(
            CST_Instruction_Kind::push_quoted_string, CST_Instruction_Kind::pop_quoted_string
        );
    }

    [[nodiscard]]
    std::optional<ast::Primary> try_build_block_or_string(
        CST_Instruction_Kind push_kind, //
        CST_Instruction_Kind pop_kind
    )
    {
        COWEL_DEBUG_ASSERT(
            push_kind == CST_Instruction_Kind::push_block
            || push_kind == CST_Instruction_Kind::push_quoted_string
        );
        COWEL_DEBUG_ASSERT(
            pop_kind == CST_Instruction_Kind::pop_block
            || pop_kind == CST_Instruction_Kind::pop_quoted_string
        );

        if (eof()) {
            return {};
        }
        const CST_Instruction instruction = peek_instruction();
        if (instruction.kind != push_kind) {
            return {};
        }

        const auto initial_pos = peek_token().location;
        pop_instruction();
        advance_by_tokens(1);

        ast::Pmr_Vector<ast::Markup_Element> content { m_memory };

        while (!eof()) {
            const CST_Instruction next = peek_instruction();
            if (next.kind == pop_kind) {
                pop_instruction();
                break;
            }
            append_markup_element(content);
        }
        advance_by_tokens(1);

        const File_Source_Span source_span {
            initial_pos,
            peek_token().location.begin - initial_pos.begin,
            m_file,
        };
        return push_kind == CST_Instruction_Kind::push_block
            ? ast::Primary::block(source_span, extract(source_span), std::move(content))
            : ast::Primary::quoted_string(source_span, extract(source_span), std::move(content));
    }
};

} // namespace

void build_ast(
    ast::Pmr_Vector<ast::Markup_Element>& out,
    const std::u8string_view source,
    const File_Id file,
    const std::span<const Token> tokens,
    const std::span<const CST_Instruction> instructions,
    std::pmr::memory_resource* const memory
)
{
    AST_Builder { source, file, tokens, instructions, memory }.build_document(out);
}

ast::Pmr_Vector<ast::Markup_Element> build_ast(
    const std::u8string_view source,
    const File_Id file,
    const std::span<const Token> tokens,
    const std::span<const CST_Instruction> instructions,
    std::pmr::memory_resource* const memory
)
{
    ast::Pmr_Vector<ast::Markup_Element> result { memory };
    build_ast(result, source, file, tokens, instructions, memory);
    return result;
}

bool parse_and_build(
    ast::Pmr_Vector<ast::Markup_Element>& out,
    const std::u8string_view source,
    const std::span<const Token> tokens,
    const File_Id file,
    std::pmr::memory_resource* const memory,
    const Parse_Error_Consumer on_error
)
{
    std::pmr::vector<CST_Instruction> instructions { memory };
    if (parse(instructions, tokens, on_error)) {
        build_ast(out, source, file, tokens, instructions, memory);
        return true;
    }
    return false;
}

bool lex_and_parse_and_build(
    ast::Pmr_Vector<ast::Markup_Element>& out,
    const std::u8string_view source,
    const File_Id file,
    std::pmr::memory_resource* const memory,
    const Parse_Error_Consumer on_error
)
{
    std::pmr::vector<Token> tokens { memory };
    if (!lex(tokens, source, on_error)) {
        return false;
    }
    return parse_and_build(out, source, tokens, file, memory, on_error);
}

} // namespace cowel
