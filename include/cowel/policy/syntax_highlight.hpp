#ifndef COWEL_POLICY_SYNTAX_HIGHLIGHT_HPP
#define COWEL_POLICY_SYNTAX_HIGHLIGHT_HPP

#include <cstddef>
#include <memory_resource>
#include <string_view>
#include <vector>

#include "cowel/util/char_sequence.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/ast.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"
#include "cowel/output_language.hpp"
#include "cowel/services.hpp"

namespace cowel {

struct Syntax_Highlight_Policy : virtual Content_Policy {
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

public:
    [[nodiscard]]
    explicit Syntax_Highlight_Policy(std::pmr::memory_resource* memory)
        : Text_Sink { Output_Language::html }
        , Content_Policy { Output_Language::html }
        , m_spans { memory }
        , m_html_text { memory }
        , m_highlighted_text { memory }
    {
        m_spans.reserve(16);
        m_highlighted_text.reserve(16);
    }

    bool write(Char_Sequence8 chars, Output_Language language) override;

    bool write_phantom(Char_Sequence8 chars)
    {
        return write_highlighted_text(chars, Span_Type::phantom);
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
    bool write_highlighted_text(Char_Sequence8 chars, Span_Type type);
};

} // namespace cowel

#endif
