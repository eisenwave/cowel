#include <optional>

#include "mmml/util/assert.hpp"

#include "mmml/ast.hpp"
#include "mmml/parse.hpp"

namespace mmml {
namespace ast {

Argument::Argument(
    const Local_Source_Span& pos,
    const Local_Source_Span& name,
    std::pmr::vector<ast::Content>&& children
)
    : detail::Base { pos }
    , m_content { std::move(children) }
    , m_name { name }
{
}

[[nodiscard]]
Argument::Argument(const Local_Source_Span& pos, std::pmr::vector<ast::Content>&& children)
    : detail::Base { pos }
    , m_content { std::move(children) }
    , m_name { pos, 0 }
{
}

Directive::Directive(
    const Local_Source_Span& pos,
    std::size_t name_length,
    std::pmr::vector<Argument>&& args,
    std::pmr::vector<Content>&& block
)
    : detail::Base { pos }
    , m_name_length { name_length }
    , m_arguments { std::move(args) }
    , m_content { std::move(block) }
{
    MMML_ASSERT(m_name_length != 0);
}

Text::Text(const Local_Source_Span& pos)
    : detail::Base { pos }
{
    MMML_ASSERT(!pos.empty());
}

Escaped::Escaped(const Local_Source_Span& pos)
    : detail::Base { pos }
{
    MMML_ASSERT(pos.length == 2);
}

} // namespace ast

namespace {

[[nodiscard]]
bool is_escapeable(char c)
{
    return c == '\\' || c == '}' || c == '{';
}

[[nodiscard]]
bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

[[nodiscard]]
bool is_latin(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

[[nodiscard]]
bool is_whitespace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

[[nodiscard]]
bool is_alphanumeric(char c)
{
    return is_digit(c) || is_latin(c);
}

[[nodiscard]]
bool is_argument_name_character(char c)
{
    return c == '-' || c == '_' || is_alphanumeric(c);
}

[[nodiscard]]
bool is_directive_name_character(char c)
{
    return c == '-' || is_alphanumeric(c);
}

enum struct Content_Context : Default_Underlying { document, argument_value, block };

[[nodiscard]]
constexpr bool is_terminated_by(Content_Context context, char c)
{
    switch (context) {
    case Content_Context::argument_value: //
        return c == ',' || c == ']' || c == '}';
    case Content_Context::block: //
        return c == '}';
    default: //
        return false;
    }
}

struct [[nodiscard]] Parser {
private:
    struct [[nodiscard]] Scoped_Attempt {
    private:
        Parser* m_self;
        const std::size_t m_initial_pos;
        const std::size_t m_initial_size;

    public:
        Scoped_Attempt(Parser& self)
            : m_self { &self }
            , m_initial_pos { self.m_pos }
            , m_initial_size { self.m_out.size() }
        {
        }

        Scoped_Attempt(const Scoped_Attempt&) = delete;
        Scoped_Attempt& operator=(const Scoped_Attempt&) = delete;

        void commit()
        {
            MMML_ASSERT(m_self);
            m_self = nullptr;
        }

        void abort()
        {
            MMML_ASSERT(m_self);
            MMML_ASSERT(m_self->m_out.size() >= m_initial_size);

            m_self->m_pos = m_initial_pos;
            m_self->m_out.resize(m_initial_size);

            m_self = nullptr;
        }

        ~Scoped_Attempt()
        {
            if (m_self) {
                abort();
            }
        }
    };

    std::pmr::vector<AST_Instruction>& m_out;
    std::string_view m_source;
    std::size_t m_pos = 0;

public:
    Parser(std::pmr::vector<AST_Instruction>& out, std::string_view source)
        : m_out { out }
        , m_source { source }
    {
    }

    void operator()()
    {
        const std::size_t document_instruction_index = m_out.size();
        m_out.push_back({ AST_Instruction_Type::push_document, 0 });
        const std::size_t content_amount = match_content_sequence(Content_Context::document);
        m_out[document_instruction_index].n = content_amount;
        m_out.push_back({ AST_Instruction_Type::pop_document, 0 });
    }

private:
    Scoped_Attempt attempt()
    {
        return Scoped_Attempt { *this };
    }

    /// @brief Returns all remaining text as a `std::string_view`, from the current parsing
    /// position to the end of the file.
    /// @return All remaining text.
    [[nodiscard]]
    std::string_view peek_all() const
    {
        return m_source.substr(m_pos);
    }

    /// @brief Returns the next character and advances the parser position.
    /// @return The popped character.
    /// @throws Throws if `eof()`.
    char pop()
    {
        const char c = peek();
        ++m_pos;
        return c;
    }

    /// @brief Returns the next character.
    /// @return The next character.
    /// @throws Throws if `eof()`.
    char peek() const
    {
        MMML_ASSERT(!eof());
        return m_source[m_pos];
    }

    /// @return `true` if the parser is at the end of the file, `false` otherwise.
    bool eof() const
    {
        return m_pos == m_source.length();
    }

    /// @return `peek_all().starts_with(text)`.
    [[nodiscard]]
    bool peek(std::string_view text) const
    {
        return peek_all().starts_with(text);
    }

    /// @brief Checks whether the next character matches an expected value without advancing
    /// the parser.
    /// @param c the character to test
    /// @return `true` if the next character equals `c`, `false` otherwise.
    [[nodiscard]]
    bool peek(char c) const
    {
        return !eof() && m_source[m_pos] == c;
    }

    /// @brief Checks whether the parser is at the start of a directive.
    /// Namely, has to be `\\` and not be the start of an escape sequence such as `\\\\` for this
    /// to be the case.
    /// This function can have false positives in the sense that if the subsequent directive is
    /// ill-formed, the guess was optimistic, and there isn't actually a directive there.
    /// However, it has no false negatives.
    /// @return `true` if the parser is at the start of a directive, `false` otherwise.
    [[nodiscard]]
    bool peek_possible_directive() const
    {
        const std::string_view rest = peek_all();
        return !rest.empty() //
            && rest[0] == '\\' //
            && (rest.length() <= 1 || !is_escapeable(rest[1]));
    }

    /// @brief Checks whether the next character satisfies a predicate without advancing
    /// the parser.
    /// @param predicate the predicate to test
    /// @return `true` if the next character satisfies `predicate`, `false` otherwise.
    [[nodiscard]]
    bool peek(bool predicate(char)) const
    {
        return !eof() && predicate(m_source[m_pos]);
    }

    [[nodiscard]]
    bool expect(char c)
    {
        if (!peek(c)) {
            return false;
        }
        ++m_pos;
        return true;
    }

    [[nodiscard]]
    bool expect(bool predicate(char))
    {
        if (eof()) {
            return false;
        }
        const char c = m_source[m_pos];
        if (!predicate(c)) {
            return false;
        }
        ++m_pos;
        return true;
    }

    [[nodiscard]]
    bool expect_literal(std::string_view text)
    {
        if (!peek(text)) {
            return false;
        }
        m_pos += text.length();
        return true;
    }

    /// @brief Matches a (possibly empty) sequence of characters matching the predicate.
    /// @return The amount of characters matched.
    [[nodiscard]]
    std::size_t match_char_sequence(bool predicate(char))
    {
        const std::size_t initial = m_pos;
        while (expect(predicate)) { }
        return m_pos - initial;
    }

    [[nodiscard]]
    std::size_t match_directive_name()
    {
        return peek(is_digit) ? 0 : match_char_sequence(is_directive_name_character);
    }

    [[nodiscard]]
    std::size_t match_argument_name()
    {
        return peek(is_digit) ? 0 : match_char_sequence(is_argument_name_character);
    }

    std::size_t match_whitespace()
    {
        return match_char_sequence(is_whitespace);
    }

    [[nodiscard]]
    std::size_t match_content_sequence(Content_Context context)
    {
        Bracket_Levels levels {};
        std::size_t elements = 0;

        for (; !eof(); ++elements) {
            if (is_terminated_by(context, peek())) {
                break;
            }
            // TODO: perhaps we could simplify this by making try_match_content
            //       the loop condition.
            //       After all, that function also checks for termination and EOF.
            const bool success = try_match_content(context, levels);
            MMML_ASSERT(success);
        }

        return elements;
    }

    struct Bracket_Levels {
        std::size_t square = 0;
        std::size_t brace = 0;
    };

    /// @brief Attempts to match the next piece of content,
    /// which is an escape sequence, directive, or plaintext.
    ///
    /// Returns `false` if none of these could be matched.
    /// This may happen because the parser is located at e.g. a `}` and the given `context`
    /// is terminated by `}`.
    /// It may also happen if the parser has already reached the EOF.
    [[nodiscard]]
    bool try_match_content(Content_Context context, Bracket_Levels& levels)
    {
        if (peek('\\') && (try_match_escaped() || try_match_directive())) {
            return true;
        }

        const std::size_t initial_pos = m_pos;

        for (; !eof(); ++m_pos) {
            const char c = m_source[m_pos];
            if (c == '\\') {
                // Trailing \ at the end of the file
                if (initial_pos + 1 == m_source.size()) {
                    continue;
                }
                const char next = m_source[m_pos + 1];
                // No matter what, a backslash followed by a directive name character forms a
                // directive because the remaining arguments and the block are optional.
                // Therefore, we must stop here because text content should not include directives.
                if (is_escapeable(next) || is_directive_name_character(next)) {
                    break;
                }
                continue;
            }
            // At the document level, we don't care about brace mismatches,
            // commas, etc.
            if (context == Content_Context::document) {
                continue;
            }
            if (context == Content_Context::argument_value) {
                if (c == ',') {
                    break;
                }
                if (c == '[') {
                    ++levels.square;
                }
                if (c == ']' && levels.square-- == 0) {
                    break;
                }
            }
            if (c == '{') {
                ++levels.brace;
            }
            if (c == '}' && levels.brace-- == 0) {
                break;
            }
        }

        MMML_ASSERT(m_pos >= initial_pos);
        if (m_pos == initial_pos) {
            return false;
        }

        m_out.push_back({ AST_Instruction_Type::text, m_pos - initial_pos });
        return true;
    }

    [[nodiscard]]
    bool try_match_directive()
    {
        Scoped_Attempt a = attempt();

        if (!expect('\\')) {
            return {};
        }
        const std::size_t name_length = match_directive_name();
        if (name_length == 0) {
            return false;
        }

        m_out.push_back({ AST_Instruction_Type::push_directive, name_length + 1 });

        try_match_argument_list();
        try_match_block();

        m_out.push_back({ AST_Instruction_Type::pop_directive, 0 });

        a.commit();
        return true;
    }

    // intentionally discardable
    bool try_match_argument_list()
    {
        Scoped_Attempt a = attempt();

        if (!expect('[')) {
            return {};
        }
        const std::size_t arguments_instruction_index = m_out.size();
        m_out.push_back({ AST_Instruction_Type::push_arguments, 0 });

        for (std::size_t i = 0; try_match_argument(); ++i) {
            if (expect(']')) {
                m_out[arguments_instruction_index].n = i + 1;
                m_out.push_back({ AST_Instruction_Type::pop_arguments, 0 });
                a.commit();
                return true;
            }
            if (expect(',')) {
                m_out.push_back({ AST_Instruction_Type::skip, 1 });
                continue;
            }
            MMML_ASSERT_UNREACHABLE("Successfully matched arguments must be followed by ']' or ','"
            );
        }

        return false;
    }

    [[nodiscard]]
    bool try_match_escaped()
    {
        constexpr std::size_t sequence_length = 2;

        if (m_pos + sequence_length < m_source.size() //
            && m_source[m_pos] == '\\' //
            && is_escapeable(m_source[m_pos + 1])) //
        {
            m_pos += sequence_length;
            m_out.push_back({ AST_Instruction_Type::escape, sequence_length });
            return true;
        }
        return false;
    }

    [[nodiscard]]
    bool try_match_argument()
    {
        if (eof()) {
            return false;
        }
        Scoped_Attempt a = attempt();

        const std::size_t argument_instruction_index = m_out.size();
        m_out.push_back({ AST_Instruction_Type::push_argument, 0 });

        try_match_argument_name();

        const std::optional<std::size_t> result = try_match_trimmed_argument_value();
        if (!result) {
            return false;
        }

        m_out[argument_instruction_index].n = *result;
        m_out.push_back({ AST_Instruction_Type::pop_argument, 0 });

        a.commit();
        return true;
    }

    /// @brief Matches the name of an argument, including any surrounding whitespace and the `=`
    /// character following it.
    /// If the argument couldn't be matched, returns `false` and keeps the parser state unchanged.
    bool try_match_argument_name()
    {
        Scoped_Attempt a = attempt();

        const std::size_t leading_whitespace = match_whitespace();
        if (leading_whitespace != 0) {
            m_out.push_back({ AST_Instruction_Type::skip, leading_whitespace });
        }

        if (eof()) {
            return false;
        }

        const std::size_t name_length = match_argument_name();
        m_out.push_back({ AST_Instruction_Type::argument_name, name_length });

        if (name_length == 0) {
            return false;
        }

        const std::size_t trailing_whitespace = match_whitespace();
        if (eof()) {
            return false;
        }

        if (!expect('=')) {
            return false;
        }

        m_out.push_back({ AST_Instruction_Type::skip, trailing_whitespace + 1 });
        a.commit();
        return true;
    }

    [[nodiscard]]
    std::optional<std::size_t> try_match_trimmed_argument_value()
    {
        Scoped_Attempt a = attempt();

        const std::size_t leading_whitespace = match_whitespace();
        if (leading_whitespace != 0) {
            m_out.push_back({ AST_Instruction_Type::skip, leading_whitespace });
        }

        const std::size_t content_amount = match_content_sequence(Content_Context::argument_value);
        if (eof() || peek('}')) {
            return {};
        }
        // match_content_sequence is very aggressive, so I think at this point,
        // we have to be at the end of an argument due to a comma separator or closing square.
        const char c = m_source[m_pos];
        MMML_ASSERT(c == ',' || c == ']');

        trim_trailing_whitespace_in_matched_content();

        a.commit();
        return content_amount;
    }

    /// @brief Trims trailing whitespace in just matched content.
    ///
    /// This is done by splitting the most recently written instruction
    /// into `text` and `skip` if that instruction is `text`.
    /// If the most recent instruction is entirely made of whitespace,
    /// it is simply replaced with `skip`.
    void trim_trailing_whitespace_in_matched_content()
    {
        MMML_ASSERT(!m_out.empty());

        AST_Instruction& latest = m_out.back();
        if (latest.type != AST_Instruction_Type::text) {
            return;
        }
        const std::size_t total_length = latest.n;
        MMML_ASSERT(total_length != 0);

        const std::size_t text_begin = m_pos - total_length;

        const std::string_view last_text = m_source.substr(text_begin, total_length);
        const std::size_t last_non_white = last_text.find_last_not_of(" \t\r\n");
        const std::size_t non_white_length = last_non_white + 1;

        if (last_non_white == std::string_view::npos) {
            latest.type = AST_Instruction_Type::skip;
        }
        else if (non_white_length < total_length) {
            latest.n = non_white_length;
            m_out.push_back({ AST_Instruction_Type::skip, total_length - non_white_length });
        }
        else {
            MMML_ASSERT(non_white_length == total_length);
        }
    }

    bool try_match_block()
    {
        if (!expect('{')) {
            return {};
        }

        Scoped_Attempt a = attempt();

        const std::size_t block_instruction_index = m_out.size();
        m_out.push_back({ AST_Instruction_Type::push_block, 0 });

        // A possible optimization should be to find the closing brace and then run the parser
        // on the brace-enclosed block.
        // This would prevent ever discarding any matched content, but might not be worth it.
        //
        // I suspect we only have to discard if we reach the EOF unexpectedly,
        // and that seems like a broken file anyway.
        const std::size_t elements = match_content_sequence(Content_Context::block);

        if (!expect('}')) {
            return {};
        }

        m_out[block_instruction_index].n = elements;
        m_out.push_back({ AST_Instruction_Type::pop_block, 0 });

        a.commit();
        return elements;
    }
};

} // namespace

void parse(std::pmr::vector<AST_Instruction>& out, std::string_view source)
{
    return Parser { out, source }();
}

} // namespace mmml
