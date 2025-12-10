#ifndef COWEL_POLICY_HTML_HPP
#define COWEL_POLICY_HTML_HPP

#include <string_view>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/html.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/ast.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/output_language.hpp"
#include "cowel/settings.hpp"

namespace cowel {

namespace detail {

static constexpr auto is_html_escaped
    = [](char8_t c) { return c == u8'&' || c == u8'<' || c == u8'>'; };

inline bool write_as_html(Text_Sink& out, Char_Sequence8 chars)
{
    if constexpr (enable_empty_string_assertions) {
        COWEL_ASSERT(!chars.empty());
    }
    COWEL_ASSERT(out.get_language() == Output_Language::html);
    const auto adapter = [&](auto x) {
        using T = decltype(x);
        static_assert(std::is_same_v<char8_t, T> || std::is_same_v<std::u8string_view, T>);
        out.write(x, Output_Language::html);
    };

    append_html_escaped(adapter, chars, is_html_escaped);
    return true;
}

} // namespace detail

struct HTML_Content_Policy : virtual Content_Policy {
protected:
    Text_Sink& m_parent;

public:
    [[nodiscard]]
    explicit HTML_Content_Policy(Text_Sink& parent)
        : Text_Sink { Output_Language::html }
        , Content_Policy { Output_Language::html }
        , m_parent { parent }
    {
    }

    [[nodiscard]]
    Text_Sink& parent() & noexcept
    {
        return m_parent;
    }

    bool write(Char_Sequence8 chars, Output_Language language) override
    {
        if constexpr (enable_empty_string_assertions) {
            COWEL_ASSERT(!chars.empty());
        }

        switch (language) {
        case Output_Language::none: {
            COWEL_ASSERT_UNREACHABLE(u8"None input.");
        }
        case Output_Language::text: {
            return detail::write_as_html(m_parent, chars);
        }
        case Output_Language::html: {
            return m_parent.write(chars, language);
        }
        default: {
            return false;
        }
        }
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
