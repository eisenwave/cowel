#ifndef COWEL_POLICY_ACTIONS_HPP
#define COWEL_POLICY_ACTIONS_HPP

#include "cowel/util/char_sequence.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/fwd.hpp"
#include "cowel/output_language.hpp"

namespace cowel {

struct Actions_Content_Policy : virtual Content_Policy {
protected:
    Content_Policy& m_parent;

public:
    [[nodiscard]]
    explicit Actions_Content_Policy(Content_Policy& parent)
        : Text_Sink { parent.get_language() }
        , Content_Policy { parent.get_language() }
        , m_parent { parent }
    {
    }

    bool write(Char_Sequence8 chars, Output_Language language) override
    {
        return m_parent.write(chars, language);
    }

    [[nodiscard]]
    Processing_Status consume(const ast::Primary&, Frame_Index, Context&) override
    {
        return Processing_Status::ok;
    }
    [[nodiscard]]
    Processing_Status
    consume(const ast::Directive& directive, Frame_Index frame, Context& context) override
    {
        return m_parent.consume(directive, frame, context);
    }
};

} // namespace cowel

#endif
