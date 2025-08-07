#ifndef COWEL_POLICY_LITERALLY_HPP
#define COWEL_POLICY_LITERALLY_HPP

#include "cowel/util/assert.hpp"

#include "cowel/policy/plaintext.hpp"

#include "cowel/content_status.hpp"
#include "cowel/fwd.hpp"
#include "cowel/output_language.hpp"

namespace cowel {

struct To_Source_Content_Policy : virtual Plaintext_Content_Policy {
private:
public:
    [[nodiscard]]
    explicit To_Source_Content_Policy(Text_Sink& parent)
        : Text_Sink { Output_Language::text }
        , Content_Policy { Output_Language::text }
        , Plaintext_Content_Policy { parent }

    {
    }

    [[nodiscard]]
    Processing_Status consume(const ast::Text& text, Frame_Index, Context&) override
    {
        write(text.get_source(), Output_Language::text);
        return Processing_Status::ok;
    }
    [[nodiscard]]
    Processing_Status consume(const ast::Comment& comment, Frame_Index, Context&) override
    {
        write(comment.get_source(), Output_Language::text);
        return Processing_Status::ok;
    }
    [[nodiscard]]
    Processing_Status consume(const ast::Escaped& escaped, Frame_Index, Context&) override
    {
        write(escaped.get_source(), Output_Language::text);
        return Processing_Status::ok;
    }
    [[nodiscard]]
    Processing_Status consume(const ast::Directive& directive, Frame_Index, Context&) override
    {
        write(directive.get_source(), Output_Language::text);
        return Processing_Status::ok;
    }
    [[nodiscard]]
    Processing_Status consume(const ast::Generated&, Frame_Index, Context&) override
    {
        COWEL_ASSERT_UNREACHABLE(u8"Generated content within To_Source_Policy should be impossible."
        );
    }
};

} // namespace cowel

#endif
