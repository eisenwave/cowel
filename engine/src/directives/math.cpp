#include <algorithm>
#include <bitset>
#include <cstddef>
#include <span>
#include <string_view>

#include "cowel/parameters.hpp"
#include "cowel/util/chars.hpp"
#include "cowel/util/html_writer.hpp"

#include "cowel/policy/content_policy.hpp"
#include "cowel/policy/factory.hpp"
#include "cowel/policy/html.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_display.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/invocation.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

// clang-format off
#define COWEL_MATHML_ELEMENT_DATA(F)                                                               \
    F(annotation, 1)                                                                               \
    F(annotation-xml, 0)                                                                         \
    F(maction, 0)                                                                                  \
    F(menclose, 0)                                                                                 \
    F(merror, 0)                                                                                   \
    F(mfenced, 0)                                                                                  \
    F(mfrac, 0)                                                                                    \
    F(mi, 1)                                                                                       \
    F(mmultiscripts, 0)                                                                            \
    F(mn, 1)                                                                                       \
    F(mo, 1)                                                                                       \
    F(mover, 0)                                                                                    \
    F(mpadded, 0)                                                                                  \
    F(mphantom, 0)                                                                                 \
    F(mprescripts, 0)                                                                              \
    F(mroot, 0)                                                                                    \
    F(mrow, 0)                                                                                     \
    F(ms, 1)                                                                                       \
    F(mspace, 0)                                                                                   \
    F(msqrt, 0)                                                                                    \
    F(mstyle, 0)                                                                                   \
    F(msub, 0)                                                                                     \
    F(msubsup, 0)                                                                                  \
    F(msup, 0)                                                                                     \
    F(mtable, 0)                                                                                   \
    F(mtd, 0)                                                                                      \
    F(mtext, 1)                                                                                    \
    F(mtr, 0)                                                                                      \
    F(munder, 0)                                                                                   \
    F(munderover, 0)                                                                               \
    F(semantics, 0)
// clang-format on

#define COWEL_MATHML_ELEMENT_NAME(name, permits_text) u8## #name,
#define COWEL_MATHML_ELEMENT_PERMITS_TEXT(name, permits_text) #permits_text[0],

constexpr std::u8string_view mathml_names[] = { //
    COWEL_MATHML_ELEMENT_DATA(COWEL_MATHML_ELEMENT_NAME)
};

static_assert(std::ranges::is_sorted(mathml_names));

constexpr auto mathml_permits_text_bits = [] { //
    char init[] { 0, COWEL_MATHML_ELEMENT_DATA(COWEL_MATHML_ELEMENT_PERMITS_TEXT) };
    std::ranges::reverse(init);
    // Use of std::string is workaround for libc++ bug:
    // https://github.com/llvm/llvm-project/issues/143684
    return std::bitset<std::size(init)>(init);
}();

constexpr std::ptrdiff_t mathml_element_index(std::u8string_view name)
{
    const auto* const pos = std::ranges::lower_bound(mathml_names, name);
    if (pos == std::end(mathml_names) || *pos != name) {
        return -1;
    }
    return pos - mathml_names;
}

static_assert(mathml_permits_text_bits[mathml_element_index(u8"mi")]);
static_assert(!mathml_permits_text_bits[mathml_element_index(u8"munderover")]);

constexpr Type bool_or_str_types[] { Type::boolean, Type::str };
constexpr auto bool_or_str = Type::union_of(bool_or_str_types);
constexpr auto named_bool_or_str = Type::named(&bool_or_str);
constexpr auto pack_named_bool_or_str = Type::pack_of(&named_bool_or_str);
static_assert(pack_named_bool_or_str.is_canonical());

struct Math_Content_Policy final : HTML_Content_Policy {
private:
    bool m_permit_text;

public:
    [[nodiscard]]
    Math_Content_Policy(Text_Sink& parent, const bool permit_text)
        : Text_Sink { Output_Language::html }
        , Content_Policy { Output_Language::html }
        , HTML_Content_Policy { parent }
        , m_permit_text { permit_text }
    {
    }

    [[nodiscard]]
    Processing_Status
    consume(const ast::Primary& node, const Frame_Index frame, Context& context) override
    {
        const auto kind = node.get_kind();
        const bool attempt_diagnose_text = !m_permit_text
            && (kind == ast::Primary_Kind::text || kind == ast::Primary_Kind::escape);
        if (attempt_diagnose_text) {
            const bool is_non_blank_escape
                = kind == ast::Primary_Kind::escape && !expand_escape(node).empty();
            const bool is_non_blank_text = kind == ast::Primary_Kind::text
                && !std::ranges::all_of(node.get_source(),
                                        [](const char8_t c) { return is_ascii_blank(c); });
            if (is_non_blank_escape || is_non_blank_text) {
                context.try_warning(
                    diagnostic::math::text, node.get_source_span(),
                    u8"Text cannot appear in this context. "
                    u8"MathML requires text to be enclosed in <mi>, <mn>, etc., "
                    u8"which correspond to \\mi, \\mn, and other pseudo-directives."sv
                );
            }
        }
        return HTML_Content_Policy::consume(node, frame, context);
    }

    [[nodiscard]]
    Processing_Status
    consume(const ast::Directive& d, const Frame_Index content_frame, Context& context) override
    {
        const std::u8string_view name_string = d.get_name();
        const std::ptrdiff_t index = mathml_element_index(name_string);
        if (index < 0) {
            return HTML_Content_Policy::consume(d, content_frame, context);
        }

        Pack_Named_Of_Type_Matcher attr_matcher { pack_named_bool_or_str };
        Parameter args_param { u8"attr"sv, Optionality::optional, attr_matcher };
        Block_Matcher content_matcher;
        Parameter content_param { u8"content"sv, Optionality::mandatory, content_matcher };
        Parameter* const parameters[] { &args_param, &content_param };

        Invocation call {
            .name = name_string,
            .directive = d,
            .arguments = d.get_arguments(),
            .content = d.get_content(),
            .content_frame = content_frame,
            .call_frame = content_frame,
        };
        const auto match_status = match_call(parameters, call, context);
        if (match_status != Processing_Status::ok) {
            return match_status;
        }

        // directive names are HTML tag names
        const HTML_Tag_Name name { Unchecked {}, name_string };
        HTML_Writer_Buffer buffer { *this, Output_Language::html };
        Text_Buffer_HTML_Writer writer { buffer };
        auto attributes = writer.open_tag_with_attributes(name);
        const auto attributes_status
            = named_arguments_to_attributes_or_error(attributes, attr_matcher, context);
        attributes.end();

        buffer.flush();

        const bool child_permits_text = mathml_permits_text_bits[std::size_t(index)];
        const bool old_permit_text = std::exchange(m_permit_text, child_permits_text);
        const auto nested_status = content_matcher.get().splice_block(*this, context);
        m_permit_text = old_permit_text;

        writer.close_tag(name);
        buffer.flush();
        return status_concat(attributes_status, nested_status);
    }
};

} // namespace

Processing_Status
Math_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    static constexpr auto tag_name = html_tag::math;
    const auto display_string
        = m_display == Directive_Display::in_line ? u8"inline"sv : u8"block"sv;

    Pack_Named_Of_Type_Matcher attr_matcher { pack_named_bool_or_str };
    Parameter attr_param { u8"attr"sv, Optionality::optional, attr_matcher };
    Block_Matcher content_matcher;
    Parameter content_param { u8"content"sv, Optionality::mandatory, content_matcher };
    Parameter* const parameters[] { &attr_param, &content_param };

    const auto match_status = match_call(parameters, call, context);
    if (match_status != Processing_Status::ok) {
        return status_is_error(match_status) ? try_generate_error(out, call, context, match_status)
                                             : match_status;
    }

    ensure_paragraph_matches_display(out, m_display);

    HTML_Content_Policy policy = ensure_html_policy(out);
    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    auto attributes = writer.open_tag_with_attributes(tag_name);
    attributes.write_display(display_string);
    const Processing_Status attributes_status
        = named_arguments_to_attributes_or_error(attributes, attr_matcher, context);
    attributes.end();
    buffer.flush();

    Math_Content_Policy math_policy { policy, /*permit_text=*/false };
    const auto nested_status = content_matcher.get().splice_block(math_policy, context);
    writer.close_tag(tag_name);
    buffer.flush();
    return status_concat(attributes_status, nested_status);
}

} // namespace cowel
