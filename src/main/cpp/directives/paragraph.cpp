#include <string_view>

#include "cowel/policy/paragraph_split.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

[[nodiscard]]
Processing_Status control_paragraph(
    void (Paragraph_Split_Policy::*action)(),
    Content_Policy& out,
    const Invocation& call,
    Context& context
)
{
    const auto match_status = match_empty_arguments(call, context);
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    if (!call.get_content_span().empty()) {
        context.try_warning(
            diagnostic::ignored_content, call.content->get_source_span(),
            u8"Content in a paragraph control directive is ignored."sv
        );
    }
    if (auto* const derived = dynamic_cast<Paragraph_Split_Policy*>(&out)) {
        (derived->*action)();
    }
    return Processing_Status::ok;
}

} // namespace

Processing_Status
Paragraph_Enter_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context)
    const
{
    return control_paragraph(&Paragraph_Split_Policy::enter_paragraph, out, call, context);
}

Processing_Status
Paragraph_Leave_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context)
    const
{
    return control_paragraph(&Paragraph_Split_Policy::leave_paragraph, out, call, context);
}

Processing_Status
Paragraph_Inherit_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context)
    const
{
    return control_paragraph(&Paragraph_Split_Policy::inherit_paragraph, out, call, context);
}

} // namespace cowel
