#ifndef COWEL_POLICY_PHANTOM_HPP
#define COWEL_POLICY_PHANTOM_HPP

#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence.hpp"

#include "cowel/policy/content_policy.hpp"
#include "cowel/policy/plaintext.hpp"
#include "cowel/policy/syntax_highlight.hpp"

#include "cowel/fwd.hpp"
#include "cowel/output_language.hpp"

namespace cowel {

struct Phantom_Content_Policy : virtual Text_Only_Policy {

    [[nodiscard]]
    explicit Phantom_Content_Policy(Text_Sink& parent) noexcept
        : Text_Sink { flags_from_parent(parent) }
        , Content_Policy { flags_from_parent(parent) }
        , Text_Only_Policy { parent }
    {
    }

    void write(Char_Sequence8 chars, const Output_Language language) override
    {
        COWEL_ASSERT(language != Output_Language::none);
        if (language == Output_Language::text) {
            if (auto* const highlight_policy = dynamic_cast<Syntax_Highlight_Policy*>(&m_parent)) {
                highlight_policy->write_phantom(chars);
            }
        }
    }
};

} // namespace cowel

#endif
