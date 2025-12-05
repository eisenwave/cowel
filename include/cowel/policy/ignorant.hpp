#ifndef COWEL_POLICY_IGNORANT_HPP
#define COWEL_POLICY_IGNORANT_HPP

#include "cowel/util/char_sequence.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/content_status.hpp"
#include "cowel/fwd.hpp"
#include "cowel/output_language.hpp"

namespace cowel {

struct Ignorant_Content_Policy : virtual Content_Policy {

    [[nodiscard]]
    explicit Ignorant_Content_Policy()
        : Text_Sink { Output_Language::none }
        , Content_Policy { Output_Language::none }

    {
    }

    bool write(Char_Sequence8, Output_Language) override
    {
        return true;
    }

    [[nodiscard]]
    Processing_Status consume(const ast::Primary&, Frame_Index, Context&) override
    {
        return Processing_Status::ok;
    }
    [[nodiscard]]
    Processing_Status consume(const ast::Directive&, Frame_Index, Context&) override
    {
        return Processing_Status::ok;
    }
    [[nodiscard]]
    Processing_Status consume(const ast::Generated&, Frame_Index, Context&) override
    {
        return Processing_Status::ok;
    }
};

} // namespace cowel

#endif
