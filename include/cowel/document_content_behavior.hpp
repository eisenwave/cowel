#ifndef COWEL_DOCUMENT_CONTENT_BEHAVIOR_HPP
#define COWEL_DOCUMENT_CONTENT_BEHAVIOR_HPP

#include "cowel/context.hpp"
#include "cowel/fwd.hpp"

namespace cowel {

struct [[nodiscard]] Head_Body_Content_Behavior {

    virtual void operator()(Context&) const;

    virtual void generate_head(Context&) const = 0;
    virtual void generate_body(Context&) const = 0;
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

    void operator()(Context&) const final;
    void generate_head(Context&) const final;
    void generate_body(Context&) const final;
};

} // namespace cowel

#endif
