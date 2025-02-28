#include <optional>

#include "mmml/assert.hpp"
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

void advance(Local_Source_Position& pos, char c)
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

struct Blank {
    std::size_t length;
    bool is_paragraph_break;
};

struct [[nodiscard]] Parser {
private:
    std::string_view m_source;
    std::pmr::memory_resource* m_memory;
    Local_Source_Position m_pos = {};

    enum struct Blank_Type {
        /// Blank space between words.
        /// Whitespace or comment space which contains no empty line.
        word_break,
        /// Whitespace containing at least one empty line outside of block comments.
        paragraph_break,
    };

public:
    Parser(std::string_view source, std::pmr::memory_resource* memory)
        : m_source(source)
        , m_memory(memory)
    {
    }

    Parsed_Document operator()()
    {
        return { .source = m_source, .content = match_content_sequence(Content_Context::document) };
    }

private:
    // UTILITIES ===================================================================================

    void advance_position_by(std::size_t length)
    {
        MMML_ASSERT(m_pos.begin + length < m_source.length());
        for (std::size_t i = 0; i < length; ++i) {
            advance(m_pos, m_source[m_pos.begin]);
        }
    }

    /// @brief Returns all remaining text as a `std::string_view`, from the current parsing
    /// position to the end of the file.
    /// @return All remaining text.
    [[nodiscard]]
    std::string_view peek_all() const
    {
        return m_source.substr(m_pos.begin);
    }

    /// @brief Returns the next character and advances the parser position.
    /// @return The popped character.
    /// @throws Throws if `eof()`.
    char pop()
    {
        const char c = peek();
        advance(m_pos, c);
        return c;
    }

    /// @brief Returns the next character.
    /// @return The next character.
    /// @throws Throws if `eof()`.
    char peek() const
    {
        MMML_ASSERT(!eof());
        return m_source[m_pos.begin];
    }

    /// @return `true` if the parser is at the end of the file, `false` otherwise.
    bool eof() const
    {
        return m_pos.begin == m_source.length();
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
        return !eof() && m_source[m_pos.begin] == c;
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
        return !eof() && predicate(m_source[m_pos.begin]);
    }

    [[nodiscard]]
    bool expect(char c)
    {
        if (!peek(c)) {
            return false;
        }
        advance(m_pos, c);
        return true;
    }

    [[nodiscard]]
    bool expect(bool predicate(char))
    {
        if (m_pos.begin >= m_source.size()) {
            return false;
        }
        const char c = m_source[m_pos.begin];
        if (!predicate(c)) {
            return false;
        }
        advance(m_pos, c);
        return true;
    }

    [[nodiscard]]
    bool expect_literal(std::string_view text)
    {
        if (!peek(text)) {
            return false;
        }
        advance_position_by(text.length());
        return true;
    }

    /// @brief Matches a (possibly empty) sequence of characters matching the predicate.
    [[nodiscard]]
    Local_Source_Span match_char_sequence(bool predicate(char))
    {
        const Local_Source_Position initial = m_pos;
        while (m_pos.begin < m_source.size()) {
            const char c = m_source[m_pos.begin];
            if (!predicate(c)) {
                break;
            }
            advance(m_pos, c);
        }
        const std::size_t length = m_pos.begin - initial.begin;
        return { initial, length };
    }

    [[nodiscard]]
    Local_Source_Span match_directive_name()
    {
        if (peek(is_digit)) {
            return { m_pos, 0 };
        }
        return match_char_sequence(is_directive_name_character);
    }

    [[nodiscard]]
    Local_Source_Span match_argument_name()
    {
        if (peek(is_digit)) {
            return { m_pos, 0 };
        }
        return match_char_sequence(is_argument_name_character);
    }

    Local_Source_Span match_whitespace()
    {
        return match_char_sequence(is_whitespace);
    }

    enum struct Content_Context { document, argument_value, block };

    static bool is_terminated_by(Content_Context context, char c)
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

    [[nodiscard]]
    std::pmr::vector<ast::Content> match_content_sequence(Content_Context context)
    {
        std::pmr::vector<ast::Content> result { m_memory };
        Bracket_Levels levels {};

        while (!eof()) {
            if (is_terminated_by(context, peek())) {
                break;
            }
            result.push_back(match_content(context, levels));
        }
        return result;
    }

    struct Bracket_Levels {
        std::size_t square = 0;
        std::size_t brace = 0;
    };

    [[nodiscard]]
    ast::Content match_content(Content_Context context, Bracket_Levels& levels)
    {
        if (peek('\\')) {
            if (std::optional<ast::Escaped> e = try_match_escaped()) {
                return std::move(*e);
            }
            if (std::optional<ast::Directive> d = try_match_directive()) {
                return std::move(*d);
            }
        }

        const Local_Source_Position start = m_pos;

        for (; !eof(); advance_position_by(1)) {
            const char c = m_source[m_pos.begin];
            if (c == '\\') {
                // Trailing \ at the end of the file
                if (m_pos.begin + 1 == m_source.size()) {
                    continue;
                }
                const char next = m_source[m_pos.begin + 1];
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

        return ast::Text { Local_Source_Span { start, m_pos.begin - start.begin } };
    }

    /// @brief Attempts to match a directive.
    /// If this attempt fails, the parser position remains the same.
    [[nodiscard]]
    std::optional<ast::Directive> try_match_directive()
    {
        const Local_Source_Position initial = m_pos;
        if (!expect('\\')) {
            return {};
        }
        const Local_Source_Span name = match_directive_name();
        if (name.empty()) {
            m_pos = initial;
            return {};
        }

        std::optional<std::pmr::vector<ast::Argument>> arguments = try_match_argument_list();
        std::optional<std::pmr::vector<ast::Content>> content = try_match_block();
        const Local_Source_Span span { initial, m_pos.begin - initial.begin };

        return ast::Directive {
            span,
            name.length,
            arguments.value_or(std::pmr::vector<ast::Argument> {}),
            content.value_or(std::pmr::vector<ast::Content> {}),
        };
    }

    [[nodiscard]]
    std::optional<std::pmr::vector<ast::Argument>> try_match_argument_list()
    {
        const Local_Source_Position initial = m_pos;

        if (!expect('[')) {
            return {};
        }

        std::pmr::vector<ast::Argument> result { m_memory };
        while (std::optional<ast::Argument> arg = try_match_argument()) {
            result.push_back(std::move(*arg));
            if (expect(']')) {
                return result;
            }
            if (!expect(',')) {
                MMML_ASSERT_UNREACHABLE(
                    "Successfully matched arguments must be followed by ']' or ','"
                );
            }
        }

        m_pos = initial;
        return {};
    }

    [[nodiscard]]
    std::optional<ast::Escaped> try_match_escaped()
    {
        if (m_pos.begin + 2 < m_source.size() //
            && m_source[m_pos.begin] == '\\' //
            && is_escapeable(m_source[m_pos.begin + 1])) {
            const Local_Source_Span result = { m_pos, 2 };
            m_pos.begin += 2;
            m_pos.column += 2;
            return ast::Escaped { result };
        }
        return {};
    }

    [[nodiscard]]
    std::optional<ast::Argument> try_match_argument()
    {
        const Local_Source_Position initial = m_pos;

        const auto skip_whitespace = [&]() {
            while (m_pos.begin < m_source.size()) {
                const char c = m_source[m_pos.begin];
                if (!is_whitespace(c)) {
                    break;
                }
                advance(m_pos, c);
            }
        };

        // 1. We skip any leading whitespace because arguments lists are still considered
        //    to use named arguments with surrounding whitespace.
        //    For example, \awoo[ x = y ] provides a named argument for x.
        skip_whitespace();

        if (eof()) {
            m_pos = initial;
            return {};
        }

        // 2. We match the name of the argument, assuming that the argument is named.
        const Local_Source_Span name = match_argument_name();

        // 3. If the name couldn't be matched, we have a positional argument.
        if (name.empty()) {
            m_pos = initial;
            return try_match_argument_value();
        }

        // 4. We skip additional whitespace after the name, which may precede a '=' character.
        skip_whitespace();
        if (eof()) {
            m_pos = initial;
            return {};
        }

        // 5. Once again, if there is no '=', this is not a named argument.
        if (!expect('=')) {
            m_pos = initial;
            return try_match_argument_value();
        }

        // 6. Match the remaining content as if it was a positional argument.
        std::optional<ast::Argument> result = try_match_argument_value();
        if (!result) {
            return result;
        }

        // 7. Convert the positional argument into a named argument.
        return ast::Argument { Local_Source_Span { initial, m_pos.begin - initial.begin }, name,
                               std::move(*result).get_content() };
    }

    [[nodiscard]]
    std::optional<ast::Argument> try_match_argument_value()
    {
        const Local_Source_Position initial = m_pos;

        std::pmr::vector<ast::Content> content
            = match_content_sequence(Content_Context::argument_value);
        if (eof() || peek('}')) {
            m_pos = initial;
            return {};
        }

        // match_content_sequence is very aggressive, so I think at this point,
        // we have to be at the end of an argument due to a comma separator or closing square.
        const char c = m_source[m_pos.begin];
        MMML_ASSERT(c == ',' || c == ']');

        const Local_Source_Span span = { initial, m_pos.begin - initial.begin };
        return ast::Argument { span, std::move(content) };
    }

    std::optional<std::pmr::vector<ast::Content>> try_match_block()
    {
        if (!expect('{')) {
            return {};
        }

        // A possible optimization should be to find the closing brace and then run the parser
        // on the brace-enclosed block.
        // This would prevent ever discarding any matched content, but might not be worth it.
        //
        // I suspect we only have to discard if we reach the EOF unexpectedly,
        // and that seems like a broken file anyway.
        std::pmr::vector<ast::Content> content = match_content_sequence(Content_Context::block);

        if (!expect('}')) {
            return {};
        }

        return content;
    }
};

} // namespace

Parsed_Document parse(std::string_view source, std::pmr::memory_resource* memory)
{
    return Parser { source, memory }();
}

} // namespace mmml
