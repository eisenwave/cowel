#include <string_view>

#include "cowel/policy/paragraph_split.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/fwd.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

void warn_all_arguments_ignored(const ast::Directive& d, Context& context)
{
    if (!d.get_arguments().empty()) {
        context.try_warning(
            diagnostic::ignored_args, d.get_arguments().front().get_source_span(),
            u8"This argument (and all other arguments) are ignored."sv
        );
    }
}

[[nodiscard]]
Processing_Status control_paragraph(
    void (Paragraph_Split_Policy::*action)(),
    Content_Policy& out,
    const ast::Directive& d,
    Context& context
)
{
    warn_all_arguments_ignored(d, context);

    if (!d.get_content().empty()) {
        context.try_warning(
            diagnostic::ignored_content, d.get_source_span(),
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
Paragraph_Enter_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context)
    const
{
    return control_paragraph(&Paragraph_Split_Policy::enter_paragraph, out, d, context);
}

Processing_Status
Paragraph_Leave_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context)
    const
{
    return control_paragraph(&Paragraph_Split_Policy::leave_paragraph, out, d, context);
}

Processing_Status Paragraph_Inherit_Behavior::operator()(
    Content_Policy& out,
    const ast::Directive& d,
    Context& context
) const
{
    return control_paragraph(&Paragraph_Split_Policy::inherit_paragraph, out, d, context);
}

} // namespace cowel
