#ifndef COWEL_POLICY_FACTORY_HPP
#define COWEL_POLICY_FACTORY_HPP

#include "cowel/policy/content_policy.hpp"
#include "cowel/policy/html.hpp"

namespace cowel {

[[nodiscard]]
inline HTML_Content_Policy ensure_html_policy(Content_Policy& out)
{
    if (auto* const policy = dynamic_cast<HTML_Content_Policy*>(&out)) {
        return HTML_Content_Policy { policy->parent() };
    }
    return HTML_Content_Policy { out };
}

} // namespace cowel

#endif
