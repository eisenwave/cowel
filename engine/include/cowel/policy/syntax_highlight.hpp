#ifndef COWEL_POLICY_SYNTAX_HIGHLIGHT_HPP
#define COWEL_POLICY_SYNTAX_HIGHLIGHT_HPP

#include <cstddef>
#include <memory_resource>
#include <string_view>
#include <vector>

#include "cowel/util/char_sequence.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"
#include "cowel/output_language.hpp"

#include "cowel/syntax/ast.hpp"

namespace cowel {

struct Syntax_Highlight_Policy : virtual Content_Policy {
private:
    [[nodiscard]]
    static Text_Sink_Flags flags_from_parent(const bool opaque, const Text_Sink_Flags parent_flags)
    {
        using enum Text_Sink_Flags;
        return !opaque                                ? parent_flags & discard
            : ((parent_flags & discard_html) != none) ? discard
                                                      : none;
    }

protected:
    enum struct Span_Type : Default_Underlying {
        html,
        highlight,
        phantom,
    };

    struct Output_Span {
        Span_Type type;
        std::size_t begin;
        std::size_t length;
    };

    std::pmr::vector<Output_Span> m_spans;
    std::pmr::vector<char8_t> m_html_text;
    std::pmr::vector<char8_t> m_highlighted_text;
    std::u8string_view m_suffix;
    bool m_opaque;

public:
    [[nodiscard]]
    explicit Syntax_Highlight_Policy(
        std::pmr::memory_resource* const memory,
        const bool opaque,
        const Text_Sink_Flags parent_flags
    )
        : Text_Sink { flags_from_parent(opaque, parent_flags) }
        , Content_Policy { flags_from_parent(opaque, parent_flags) }
        , m_spans { memory }
        , m_html_text { memory }
        , m_highlighted_text { memory }
        , m_opaque { opaque }
    {
        m_spans.reserve(16);
        m_highlighted_text.reserve(16);
    }

    void write(Char_Sequence8 chars, Output_Language language) override;

    void write_phantom(Char_Sequence8 chars)
    {
        if constexpr (enable_empty_string_assertions) {
            COWEL_ASSERT(!chars.empty());
        }
        write_highlighted_text(chars, Span_Type::phantom);
    }

    [[nodiscard]]
    Processing_Status consume(const ast::Primary& node, Frame_Index, Context&) override
    {
        switch (node.get_kind()) {
        case ast::Primary_Kind::text: {
            write(node.get_source(), Output_Language::text);
            break;
        }
        case ast::Primary_Kind::escape: {
            const std::u8string_view text = node.get_escaped_code_units();
            if (!text.empty()) {
                write(text, Output_Language::text);
            }
            return Processing_Status::ok;
        }
        case ast::Primary_Kind::comment: {
            break;
        }
        case ast::Primary_Kind::unit_literal:
        case ast::Primary_Kind::null_literal:
        case ast::Primary_Kind::bool_literal:
        case ast::Primary_Kind::int_literal:
        case ast::Primary_Kind::decimal_float_literal:
        case ast::Primary_Kind::infinity:
        case ast::Primary_Kind::unquoted_member_name:
        case ast::Primary_Kind::id_expression:
        case ast::Primary_Kind::quoted_string:
        case ast::Primary_Kind::block:
        case ast::Primary_Kind::group: {
            COWEL_ASSERT_UNREACHABLE(u8"Consuming non-markup element.");
        }
        }
        return Processing_Status::ok;
    }

    [[nodiscard]]
    Processing_Status
    consume(const ast::Directive& directive, Frame_Index frame, Context& context) override
    {
        return splice_directive_invocation(*this, directive, frame, context);
    }

    [[nodiscard]]
    Processing_Status
    consume(const ast::Expression& expression, Frame_Index frame, Context& context) override
    {
        return splice_expression(*this, expression, frame, context);
    }

    /// @brief Writes pure HTML content to `out`,
    /// consisting of the received HTML content,
    /// interleaved with syntax highlighting HTML (`<h->...</h->`)
    /// formed from any incoming plaintext.
    /// @param out The sink to write to.
    /// @param context The context.
    /// @param language The syntax highlighting language.
    /// Under the hood, ulight is used, so this needs to be one of the short names
    /// that ulight supports.
    Result<void, Syntax_Highlight_Error>
    dump_html_to(Text_Sink& out, Context& context, std::u8string_view language);

private:
    void write_highlighted_text(Char_Sequence8 chars, Span_Type type);
};

} // namespace cowel

#endif
