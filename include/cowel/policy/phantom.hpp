#ifndef COWEL_POLICY_PHANTOM_HPP
#define COWEL_POLICY_PHANTOM_HPP

#include "cowel/util/char_sequence.hpp"

#include "cowel/policy/content_policy.hpp"
#include "cowel/policy/plaintext.hpp"
#include "cowel/policy/syntax_highlight.hpp"

#include "cowel/fwd.hpp"

namespace cowel {

struct Phantom_Content_Policy : virtual Plaintext_Content_Policy {

    [[nodiscard]]
    explicit Phantom_Content_Policy(Text_Sink& parent)
        : Text_Sink { Output_Language::text }
        , Content_Policy { Output_Language::text }
        , Plaintext_Content_Policy { parent }

    {
    }

    bool write(Char_Sequence8 chars, Output_Language language) override
    {
        COWEL_ASSERT(language != Output_Language::none);
        if (language != Output_Language::text) {
            return false;
        }
        auto* const highlight_policy = dynamic_cast<Syntax_Highlight_Policy*>(&m_parent);
        return highlight_policy && highlight_policy->write_phantom(chars);
    }
};

} // namespace cowel

#endif
