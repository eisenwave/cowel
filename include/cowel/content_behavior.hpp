#ifndef COWEL_PROCESSING_HPP
#define COWEL_PROCESSING_HPP

#include <span>
#include <vector>

#include "cowel/context.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"

namespace cowel {

struct Content_Behavior {

    [[nodiscard]]
    constexpr Content_Behavior()
        = default;

    virtual void
    generate_plaintext(std::pmr::vector<char8_t>& out, std::span<const ast::Content>, Context&)
        const
        = 0;
    virtual void generate_html(HTML_Writer& out, std::span<const ast::Content>, Context&) const = 0;
};

struct Minimal_Content_Behavior final : Content_Behavior {
private:
    Macro_Name_Resolver m_macro_resolver;

public:
    constexpr explicit Minimal_Content_Behavior(Directive_Behavior& macro_behavior)
        : m_macro_resolver { macro_behavior }
    {
    }

    void generate_plaintext(
        std::pmr::vector<char8_t>& out,
        std::span<const ast::Content> content,
        Context& context
    ) const final
    {
        context.add_resolver(m_macro_resolver);
        to_plaintext(out, content, context);
    }

    void generate_html(HTML_Writer& out, std::span<const ast::Content> content, Context& context)
        const final
    {
        context.add_resolver(m_macro_resolver);
        to_html(out, content, context);
    }
};

} // namespace cowel

#endif
