#ifndef COWEL_DOCUMENT_CONTENT_BEHAVIOR_HPP
#define COWEL_DOCUMENT_CONTENT_BEHAVIOR_HPP

#include <span>
#include <vector>

#include "cowel/util/assert.hpp"

#include "cowel/content_behavior.hpp"
#include "cowel/fwd.hpp"

namespace cowel {

struct [[nodiscard]] Head_Body_Content_Behavior : Content_Behavior {
    void generate_plaintext(std::pmr::vector<char8_t>&, std::span<const ast::Content>, Context&)
        const final
    {
        COWEL_ASSERT_UNREACHABLE(u8"Unimplemented.");
    }

    void generate_html(HTML_Writer& out, std::span<const ast::Content>, Context&) const override;

    virtual void generate_head(HTML_Writer& out, std::span<const ast::Content>, Context&) const = 0;
    virtual void generate_body(HTML_Writer& out, std::span<const ast::Content>, Context&) const = 0;
};

struct [[nodiscard]]
Document_Content_Behavior final : Head_Body_Content_Behavior {
private:
    Macro_Name_Resolver m_macro_resolver;

public:
    constexpr explicit Document_Content_Behavior(Directive_Behavior& macro_behavior)
        : m_macro_resolver { macro_behavior }
    {
    }

    void generate_html(HTML_Writer& out, std::span<const ast::Content>, Context&) const final;
    void generate_head(HTML_Writer& out, std::span<const ast::Content>, Context&) const final;
    void generate_body(HTML_Writer& out, std::span<const ast::Content>, Context&) const final;
};

} // namespace cowel

#endif
