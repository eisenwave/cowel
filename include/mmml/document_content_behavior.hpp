#ifndef MMML_DOCUMENT_CONTENT_BEHAVIOR_HPP
#define MMML_DOCUMENT_CONTENT_BEHAVIOR_HPP

#include <span>
#include <vector>

#include "mmml/util/assert.hpp"

#include "mmml/content_behavior.hpp"
#include "mmml/fwd.hpp"

namespace mmml {

struct [[nodiscard]] Head_Body_Content_Behavior : Content_Behavior {

    void generate_plaintext(std::pmr::vector<char8_t>&, std::span<const ast::Content>, Context&)
        const final
    {
        MMML_ASSERT_UNREACHABLE(u8"Unimplemented.");
    }

    void generate_html(HTML_Writer& out, std::span<const ast::Content>, Context&) const final;

    virtual void generate_head(HTML_Writer& out, std::span<const ast::Content>, Context&) const = 0;
    virtual void generate_body(HTML_Writer& out, std::span<const ast::Content>, Context&) const = 0;
};

struct [[nodiscard]]
Document_Content_Behavior final : Head_Body_Content_Behavior {
    void generate_head(HTML_Writer& out, std::span<const ast::Content>, Context&) const final;
    void generate_body(HTML_Writer& out, std::span<const ast::Content>, Context&) const final;
};

} // namespace mmml

#endif
