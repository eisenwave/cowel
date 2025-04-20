#ifndef MMML_PROCESSING_HPP
#define MMML_PROCESSING_HPP

#include <span>
#include <vector>

#include "mmml/context.hpp"
#include "mmml/fwd.hpp"

namespace mmml {

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

} // namespace mmml

#endif
