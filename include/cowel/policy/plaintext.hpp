#ifndef COWEL_POLICY_PLAINTEXT_HPP
#define COWEL_POLICY_PLAINTEXT_HPP

#include <string_view>

#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"
#include "cowel/output_language.hpp"

namespace cowel {

struct Plaintext_Content_Policy : virtual Content_Policy {
protected:
    Text_Sink& m_parent;

public:
    [[nodiscard]]
    explicit Plaintext_Content_Policy(Text_Sink& parent)
        : Text_Sink { Output_Language::text }
        , Content_Policy { Output_Language::text }
        , m_parent { parent }
    {
    }

    bool write(Char_Sequence8 chars, Output_Language language) override
    {
        COWEL_ASSERT(language != Output_Language::none);
        return language == Output_Language::text && m_parent.write(chars, language);
    }

    [[nodiscard]]
    Processing_Status consume(const ast::Text& text, Frame_Index, Context&) override
    {
        write(text.get_source(), Output_Language::text);
        return Processing_Status::ok;
    }
    [[nodiscard]]
    Processing_Status consume(const ast::Comment&, Frame_Index, Context&) override
    {
        return Processing_Status::ok;
    }
    [[nodiscard]]
    Processing_Status consume(const ast::Escaped& escape, Frame_Index, Context&) override
    {
        const std::u8string_view text = expand_escape(escape);
        if (!text.empty()) {
            write(text, Output_Language::text);
        }
        return Processing_Status::ok;
    }
    [[nodiscard]]
    Processing_Status
    consume(const ast::Directive& directive, Frame_Index frame, Context& context) override
    {
        return invoke_directive(*this, directive, frame, context);
    }
    [[nodiscard]]
    Processing_Status consume(const ast::Generated& generated, Frame_Index, Context&) override
    {
        write(generated.as_string(), generated.get_type());
        return Processing_Status::ok;
    }
};

} // namespace cowel

#endif
