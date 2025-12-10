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

    [[nodiscard]]
    explicit HTML_Literal_Content_Policy(Text_Sink& parent)
        : Text_Sink { Output_Language::text }
        , Content_Policy { Output_Language::text }
        , HTML_Content_Policy { parent }
    {
    }

    bool write(Char_Sequence8 chars, Output_Language language) override
    {
        COWEL_ASSERT(language != Output_Language::none);
        return language == Output_Language::text && m_parent.write(chars, Output_Language::html);
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
            const std::u8string_view text = expand_escape(node);
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
        case ast::Primary_Kind::unquoted_string:
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
    Processing_Status consume(const ast::Generated& generated, Frame_Index, Context&) override
    {
        write(generated.as_string(), generated.get_type());
        return Processing_Status::ok;
    }
};

} // namespace cowel

#endif
