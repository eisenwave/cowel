#ifndef COWEL_POLICY_HTML_LITERAL
#define COWEL_POLICY_HTML_LITERAL

#include <string_view>

#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence.hpp"

#include "cowel/policy/content_policy.hpp"
#include "cowel/policy/html.hpp"

#include "cowel/content_status.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"
#include "cowel/output_language.hpp"

namespace cowel {

struct HTML_Literal_Content_Policy : virtual HTML_Content_Policy {
    static constexpr auto flags = Text_Sink_Flags::discard_html;

    [[nodiscard]]
    explicit HTML_Literal_Content_Policy(Text_Sink& parent) noexcept
        : Text_Sink { flags }
        , Content_Policy { flags }
        , HTML_Content_Policy { parent }
    {
    }

    void write(Char_Sequence8 chars, Output_Language language) override
    {
        COWEL_ASSERT(language != Output_Language::none);
        if (language == Output_Language::text) {
            m_parent.write(chars, Output_Language::html);
        }
    }

    [[nodiscard]]
    Processing_Status consume(const ast::Primary& node, Frame_Index, Context& context) override
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
            push_escape_hover(node, text, context);
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
};

} // namespace cowel

#endif
