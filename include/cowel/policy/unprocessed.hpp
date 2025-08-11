#ifndef COWEL_POLICY_UNPROCESSED_HPP
#define COWEL_POLICY_UNPROCESSED_HPP

#include <string_view>

#include "cowel/util/assert.hpp"

#include "cowel/policy/content_policy.hpp"
#include "cowel/policy/plaintext.hpp"

#include "cowel/content_status.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"
#include "cowel/output_language.hpp"

namespace cowel {

struct Unprocessed_Content_Policy : virtual Plaintext_Content_Policy {
private:
public:
    [[nodiscard]]
    explicit Unprocessed_Content_Policy(Text_Sink& parent)
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
    Processing_Status consume(const ast::Directive& directive, Frame_Index, Context&) override
    {
        write(directive.get_source(), Output_Language::text);
        return Processing_Status::ok;
    }
    [[nodiscard]]
    Processing_Status consume(const ast::Generated&, Frame_Index, Context&) override
    {
        COWEL_ASSERT_UNREACHABLE(
            u8"Generated content within Unprocessed_Content_Policy should be impossible."
        );
    }
};

} // namespace cowel

#endif
