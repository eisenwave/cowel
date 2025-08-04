#ifndef COWEL_POLICY_HTML_HPP
#define COWEL_POLICY_HTML_HPP

#include <string_view>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/html.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/ast.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/output_language.hpp"

namespace cowel {

namespace detail {

static constexpr auto is_html_escaped
    = [](char8_t c) { return c == u8'&' || c == u8'<' || c == u8'>'; };

inline bool write_as_html(Text_Sink& out, Char_Sequence8 chars)
{
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

    bool write(Char_Sequence8 chars, Output_Language language) override
    {
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
    Processing_Status consume(const ast::Text& text, Context&) override
    {
        write(text.get_source(), Output_Language::text);
        return Processing_Status::ok;
    }
    [[nodiscard]]
    Processing_Status consume(const ast::Comment&, Context&) override
    {
        return Processing_Status::ok;
    }
    [[nodiscard]]
    Processing_Status consume(const ast::Escaped& escape, Context&) override
    {
        const std::u8string_view text = expand_escape(escape);
        write(text, Output_Language::text);
        return Processing_Status::ok;
    }
    [[nodiscard]]
    Processing_Status consume(const ast::Directive& directive, Context& context) override
    {
        return apply_behavior(*this, directive, context);
    }
    [[nodiscard]]
    Processing_Status consume(const ast::Generated& generated, Context&) override
    {
        write(generated.as_string(), generated.get_type());
        return Processing_Status::ok;
    }
};

} // namespace cowel

#endif
