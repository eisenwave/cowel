#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cowel/util/html_names.hpp"
#include "cowel/util/typo.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/directive_behavior.hpp"
#include "cowel/directive_display.hpp"
#include "cowel/fwd.hpp"

namespace cowel {
namespace {

constexpr Policy_Behavior cowel_actions //
    { Known_Content_Policy::actions };
constexpr Expression_Behavior cowel_add //
    { Numeric_Expression_Kind::add };
constexpr Alias_Behavior cowel_alias //
    {};
constexpr Char_By_Entity_Behavior cowel_char_by_entity //
    {};
constexpr Char_By_Name_Behavior cowel_char_by_name //
    {};
constexpr Char_By_Num_Behavior cowel_char_by_num //
    {};
constexpr Char_Get_Num_Behavior cowel_char_get_num //
    {};
constexpr Expression_Behavior cowel_div //
    { Numeric_Expression_Kind::div };
constexpr Policy_Behavior cowel_highlight //
    { Known_Content_Policy::highlight };
constexpr Highlight_As_Behavior cowel_highlight_as //
    {};
constexpr Policy_Behavior cowel_highlight_phantom //
    { Known_Content_Policy::phantom };
constexpr HTML_Element_Behavior cowel_html_element //
    { HTML_Element_Self_Closing::normal };
constexpr HTML_Element_Behavior cowel_html_self_closing_element //
    { HTML_Element_Self_Closing::self_closing };
constexpr Include_Behavior cowel_include //
    {};
constexpr Include_Text_Behavior cowel_include_text //
    {};
constexpr Invoke_Behavior cowel_invoke //
    {};
constexpr Macro_Behavior cowel_macro //
    {};
constexpr Expression_Behavior cowel_mul //
    { Numeric_Expression_Kind::mul };
constexpr Expression_Behavior cowel_neg //
    { Numeric_Expression_Kind::neg };
constexpr Policy_Behavior cowel_no_invoke //
    { Known_Content_Policy::no_invoke };
constexpr Policy_Behavior cowel_paragraphs //
    { Known_Content_Policy::paragraphs };
constexpr Paragraph_Enter_Behavior cowel_paragraph_enter //
    {};
constexpr Paragraph_Inherit_Behavior cowel_paragraph_inherit //
    {};
constexpr Paragraph_Leave_Behavior cowel_paragraph_leave //
    {};
constexpr Put_Behavior cowel_put //
    {};
constexpr Policy_Behavior cowel_source_as_text //
    { Known_Content_Policy::source_as_text };
constexpr Expression_Behavior cowel_sub //
    { Numeric_Expression_Kind::sub };
constexpr Policy_Behavior cowel_text_as_html //
    { Known_Content_Policy::text_as_html };
constexpr Policy_Behavior cowel_text_only //
    { Known_Content_Policy::text_only };
constexpr Policy_Behavior cowel_to_html //
    { Known_Content_Policy::to_html };

// Legacy directives
constexpr Fixed_Name_Passthrough_Behavior b //
    { html_tag::b, Policy_Usage::inherit, Directive_Display::in_line };
constexpr Special_Block_Behavior Babstract //
    { HTML_Tag_Name(u8"abstract-block"), Intro_Policy::yes };
constexpr Special_Block_Behavior Bdecision //
    { HTML_Tag_Name(u8"decision-block"), Intro_Policy::yes };
constexpr Special_Block_Behavior Bdel //
    { HTML_Tag_Name(u8"del-block"), Intro_Policy::no };
constexpr Fixed_Name_Passthrough_Behavior Bdetails //
    { HTML_Tag_Name(u8"details"), Policy_Usage::inherit, Directive_Display::block };
constexpr Special_Block_Behavior Bdiff //
    { HTML_Tag_Name(u8"diff-block"), Intro_Policy::no };
constexpr Special_Block_Behavior Bex //
    { HTML_Tag_Name(u8"example-block"), Intro_Policy::yes };
constexpr Bibliography_Add_Behavior bib //
    {};
constexpr Special_Block_Behavior Bimp //
    { HTML_Tag_Name(u8"important-block"), Intro_Policy::yes };
constexpr In_Tag_Behavior Bindent //
    { html_tag::div, u8"indent", Policy_Usage::html, Directive_Display::block };
constexpr Special_Block_Behavior Bins //
    { HTML_Tag_Name(u8"ins-block"), Intro_Policy::no };
constexpr Special_Block_Behavior blockquote //
    { HTML_Tag_Name(u8"blockquote"), Intro_Policy::no };
constexpr Special_Block_Behavior Bnote //
    { HTML_Tag_Name(u8"note-block"), Intro_Policy::yes };
constexpr Special_Block_Behavior Bquote //
    { HTML_Tag_Name(u8"blockquote"), Intro_Policy::no };
constexpr Self_Closing_Behavior br //
    { html_tag::br, Directive_Display::in_line };
constexpr Special_Block_Behavior Btip //
    { HTML_Tag_Name(u8"tip-block"), Intro_Policy::yes };
constexpr Special_Block_Behavior Btodo //
    { HTML_Tag_Name(u8"todo-block"), Intro_Policy::yes };
constexpr Special_Block_Behavior Bug //
    { HTML_Tag_Name(u8"bug-block"), Intro_Policy::yes };
constexpr Special_Block_Behavior Bwarn //
    { HTML_Tag_Name(u8"warning-block"), Intro_Policy::yes };
constexpr Fixed_Name_Passthrough_Behavior caption //
    { HTML_Tag_Name(u8"caption"), Policy_Usage::html, Directive_Display::block };
constexpr Fixed_Name_Passthrough_Behavior cite //
    { HTML_Tag_Name(u8"cite"), Policy_Usage::html, Directive_Display::in_line };
constexpr Code_Behavior code //
    { HTML_Tag_Name(u8"code"), Directive_Display::in_line, Pre_Trimming::no };
constexpr Code_Behavior codeblock //
    { HTML_Tag_Name(u8"code-block"), Directive_Display::block, Pre_Trimming::yes };
constexpr Fixed_Name_Passthrough_Behavior col //
    { HTML_Tag_Name(u8"col"), Policy_Usage::html, Directive_Display::block };
constexpr Fixed_Name_Passthrough_Behavior colgroup //
    { HTML_Tag_Name(u8"colgroup"), Policy_Usage::html, Directive_Display::block };
constexpr Comment_Behavior comment //
    {};
constexpr Fixed_Name_Passthrough_Behavior dd //
    { HTML_Tag_Name(u8"dd"), Policy_Usage::html, Directive_Display::block };
constexpr Fixed_Name_Passthrough_Behavior del //
    { HTML_Tag_Name(u8"del"), Policy_Usage::inherit, Directive_Display::in_line };
constexpr Fixed_Name_Passthrough_Behavior details //
    { HTML_Tag_Name(u8"details"), Policy_Usage::html, Directive_Display::block };
constexpr Fixed_Name_Passthrough_Behavior dfn //
    { HTML_Tag_Name(u8"dfn"), Policy_Usage::inherit, Directive_Display::in_line };
constexpr Fixed_Name_Passthrough_Behavior div //
    { html_tag::div, Policy_Usage::html, Directive_Display::block };
constexpr Fixed_Name_Passthrough_Behavior dl //
    { HTML_Tag_Name(u8"dl"), Policy_Usage::html, Directive_Display::block };
constexpr Fixed_Name_Passthrough_Behavior dt //
    { HTML_Tag_Name(u8"dt"), Policy_Usage::html, Directive_Display::block };
constexpr Fixed_Name_Passthrough_Behavior em //
    { HTML_Tag_Name(u8"em"), Policy_Usage::inherit, Directive_Display::in_line };
constexpr Error_Behavior error //
    {};
constexpr Fixed_Name_Passthrough_Behavior gterm //
    { HTML_Tag_Name(u8"g-term"), Policy_Usage::inherit, Directive_Display::in_line };
constexpr Heading_Behavior h1 //
    { 1 };
constexpr Heading_Behavior h2 //
    { 2 };
constexpr Heading_Behavior h3 //
    { 3 };
constexpr Heading_Behavior h4 //
    { 4 };
constexpr Heading_Behavior h5 //
    { 5 };
constexpr Heading_Behavior h6 //
    { 6 };
constexpr Here_Behavior here //
    { Directive_Display::in_line };
constexpr Here_Behavior hereblock //
    { Directive_Display::block };
constexpr Self_Closing_Behavior hr //
    { HTML_Tag_Name(u8"hr"), Directive_Display::block };
constexpr Fixed_Name_Passthrough_Behavior i //
    { HTML_Tag_Name(u8"i"), Policy_Usage::inherit, Directive_Display::in_line };
constexpr Fixed_Name_Passthrough_Behavior ins //
    { HTML_Tag_Name(u8"ins"), Policy_Usage::inherit, Directive_Display::in_line };
constexpr Fixed_Name_Passthrough_Behavior kbd //
    { HTML_Tag_Name(u8"kbd"), Policy_Usage::inherit, Directive_Display::in_line };
constexpr Fixed_Name_Passthrough_Behavior li //
    { HTML_Tag_Name(u8"li"), Policy_Usage::html, Directive_Display::block };
constexpr Literally_Behavior literally //
    {};
constexpr Lorem_Ipsum_Behavior lorem_ipsum //
    {};
constexpr URL_Behavior mail //
    { u8"mailto:" };
constexpr Make_Section_Behavior make_bib //
    { Directive_Display::block, class_name::bibliography, section_name::bibliography };
constexpr Make_Section_Behavior make_contents //
    { Directive_Display::block, class_name::table_of_contents, section_name::table_of_contents };
constexpr Fixed_Name_Passthrough_Behavior mark //
    { HTML_Tag_Name(u8"mark"), Policy_Usage::inherit, Directive_Display::in_line };
constexpr Math_Behavior math //
    { Directive_Display::in_line };
constexpr Math_Behavior mathblock //
    { Directive_Display::block };
constexpr In_Tag_Behavior nobr //
    { html_tag::span, u8"word", Policy_Usage::inherit, Directive_Display::in_line };
constexpr Fixed_Name_Passthrough_Behavior noscript //
    { HTML_Tag_Name(u8"noscript"), Policy_Usage::html, Directive_Display::block };
constexpr In_Tag_Behavior o //
    { html_tag::span, u8"oblique", Policy_Usage::inherit, Directive_Display::in_line };
constexpr Fixed_Name_Passthrough_Behavior ol //
    { html_tag::ol, Policy_Usage::html, Directive_Display::block };
constexpr Fixed_Name_Passthrough_Behavior p //
    { HTML_Tag_Name(u8"p"), Policy_Usage::html, Directive_Display::block };
constexpr Fixed_Name_Passthrough_Behavior pre //
    { HTML_Tag_Name(u8"pre"), Policy_Usage::html, Directive_Display::block };
constexpr Fixed_Name_Passthrough_Behavior q //
    { HTML_Tag_Name(u8"q"), Policy_Usage::inherit, Directive_Display::in_line };
constexpr Ref_Behavior ref //
    {};
constexpr Fixed_Name_Passthrough_Behavior s //
    { HTML_Tag_Name(u8"s"), Policy_Usage::inherit, Directive_Display::in_line };
constexpr Fixed_Name_Passthrough_Behavior samp //
    { HTML_Tag_Name(u8"samp"), Policy_Usage::inherit, Directive_Display::in_line };
constexpr Fixed_Name_Passthrough_Behavior sans //
    { HTML_Tag_Name(u8"f-sans"), Policy_Usage::inherit, Directive_Display::in_line };
constexpr HTML_Raw_Text_Behavior script //
    { html_tag::script };
constexpr Fixed_Name_Passthrough_Behavior serif //
    { HTML_Tag_Name(u8"f-serif"), Policy_Usage::inherit, Directive_Display::in_line };
constexpr Fixed_Name_Passthrough_Behavior small //
    { HTML_Tag_Name(u8"small"), Policy_Usage::inherit, Directive_Display::in_line };
constexpr Fixed_Name_Passthrough_Behavior span //
    { html_tag::span, Policy_Usage::inherit, Directive_Display::in_line };
constexpr Fixed_Name_Passthrough_Behavior strong //
    { HTML_Tag_Name(u8"strong"), Policy_Usage::inherit, Directive_Display::in_line };
constexpr HTML_Raw_Text_Behavior style //
    { html_tag::style };
constexpr Fixed_Name_Passthrough_Behavior sub //
    { HTML_Tag_Name(u8"sub"), Policy_Usage::inherit, Directive_Display::in_line };
constexpr Fixed_Name_Passthrough_Behavior summary //
    { HTML_Tag_Name(u8"summary"), Policy_Usage::html, Directive_Display::block };
constexpr Fixed_Name_Passthrough_Behavior sup //
    { HTML_Tag_Name(u8"sup"), Policy_Usage::inherit, Directive_Display::in_line };
constexpr Fixed_Name_Passthrough_Behavior table //
    { HTML_Tag_Name(u8"table"), Policy_Usage::html, Directive_Display::block };
constexpr URL_Behavior tel //
    { u8"tel:" };
constexpr Plaintext_Wrapper_Behavior text //
    { Directive_Display::in_line };
constexpr Fixed_Name_Passthrough_Behavior tbody //
    { HTML_Tag_Name(u8"tbody"), Policy_Usage::html, Directive_Display::block };
constexpr Fixed_Name_Passthrough_Behavior td //
    { HTML_Tag_Name(u8"td"), Policy_Usage::html, Directive_Display::block };
constexpr Fixed_Name_Passthrough_Behavior tfoot //
    { HTML_Tag_Name(u8"tfoot"), Policy_Usage::html, Directive_Display::block };
constexpr Fixed_Name_Passthrough_Behavior th //
    { HTML_Tag_Name(u8"th"), Policy_Usage::html, Directive_Display::block };
constexpr Fixed_Name_Passthrough_Behavior thead //
    { HTML_Tag_Name(u8"thead"), Policy_Usage::html, Directive_Display::block };
constexpr There_Behavior there //
    {};
constexpr Fixed_Name_Passthrough_Behavior tr //
    { HTML_Tag_Name(u8"tr"), Policy_Usage::html, Directive_Display::block };
constexpr Trim_Behavior trim //
    { Directive_Display::in_line };
constexpr Fixed_Name_Passthrough_Behavior tt //
    { HTML_Tag_Name(u8"tt-"), Policy_Usage::inherit, Directive_Display::in_line };
constexpr Fixed_Name_Passthrough_Behavior u //
    { HTML_Tag_Name(u8"u"), Policy_Usage::inherit, Directive_Display::in_line };
constexpr Fixed_Name_Passthrough_Behavior ul //
    { html_tag::ul, Policy_Usage::html, Directive_Display::block };
constexpr Unprocessed_Behavior unprocessed //
    {};
constexpr URL_Behavior url //
    {};
constexpr Fixed_Name_Passthrough_Behavior var //
    { HTML_Tag_Name(u8"var"), Policy_Usage::inherit, Directive_Display::in_line };
constexpr Get_Variable_Behavior Vget //
    {};
constexpr Modify_Variable_Behavior Vset //
    { Variable_Operation::set };
constexpr Self_Closing_Behavior wbr //
    { HTML_Tag_Name(u8"wbr"), Directive_Display::in_line };
constexpr In_Tag_Behavior wg21_grammar //
    { HTML_Tag_Name(u8"dl"), u8"grammar", Policy_Usage::html, Directive_Display::block };
constexpr WG21_Head_Behavior wg21_head //
    {};

// clang-format off
#define COWEL_DEPRECATED_ALIAS(name, use_instead)                                                  \
    constexpr Deprecated_Behavior name { use_instead, u8## #use_instead }
// clang-format on

// Usage:
// COWEL_DEPRECATED_ALIAS(abstract, Babstract);

struct Name_And_Behavior {
    std::u8string_view name;
    const Directive_Behavior* behavior;
};

#define COWEL_NAME_AND_BEHAVIOR_ENTRY(...) { u8## #__VA_ARGS__##sv, &__VA_ARGS__ }

constexpr Name_And_Behavior behaviors_by_name[] {
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Babstract),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bdecision),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bdel),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bdetails),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bdiff),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bex),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bimp),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bindent),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bins),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bnote),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bquote),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Btip),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Btodo),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bug),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bwarn),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Vget),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Vset),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(b),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(bib),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(blockquote),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(br),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(caption),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cite),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(code),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(codeblock),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(col),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(colgroup),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(comment),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_actions),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_add),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_alias),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_char_by_entity),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_char_by_name),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_char_by_num),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_char_get_num),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_div),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_highlight),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_highlight_as),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_highlight_phantom),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_html_element),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_html_self_closing_element),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_include),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_include_text),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_invoke),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_macro),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_mul),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_neg),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_no_invoke),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_paragraph_enter),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_paragraph_inherit),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_paragraph_leave),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_paragraphs),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_put),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_source_as_text),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_sub),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_text_as_html),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_text_only),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_to_html),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(dd),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(del),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(details),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(dfn),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(div),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(dl),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(dt),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(em),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(error),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(gterm),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(h1),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(h2),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(h3),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(h4),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(h5),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(h6),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(here),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(hereblock),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(hr),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(i),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(ins),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(kbd),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(li),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(literally),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(lorem_ipsum),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(mail),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(make_bib),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(make_contents),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(mark),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(math),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(mathblock),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(nobr),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(noscript),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(o),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(ol),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(p),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(pre),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(q),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(ref),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(s),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(samp),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(sans),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(script),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(serif),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(small),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(span),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(strong),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(style),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(sub),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(summary),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(sup),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(table),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(tbody),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(td),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(tel),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(text),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(tfoot),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(th),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(thead),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(there),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(tr),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(trim),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(tt),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(u),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(ul),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(unprocessed),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(url),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(var),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(wbr),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(wg21_grammar),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(wg21_head),
};

static_assert(std::ranges::is_sorted(behaviors_by_name, {}, &Name_And_Behavior::name));

} // namespace

struct Builtin_Directive_Set::Impl {
    constexpr Impl() = default;
    constexpr ~Impl() = default;

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
};

Builtin_Directive_Set::Builtin_Directive_Set()
    : m_impl(std::make_unique<Builtin_Directive_Set::Impl>())
{
}

Builtin_Directive_Set::~Builtin_Directive_Set() = default;

const Directive_Behavior& Builtin_Directive_Set::get_error_behavior() const noexcept
{
    return error;
}

Distant<std::u8string_view>
Builtin_Directive_Set::fuzzy_lookup_name(std::u8string_view name, Context& context) const
{
    static constexpr auto all_names = [] {
        std::array<std::u8string_view, std::size(behaviors_by_name)> result;
        std::ranges::transform(behaviors_by_name, result.data(), &Name_And_Behavior::name);
        return result;
    }();
    const Distant<std::size_t> result
        = closest_match(all_names, name, context.get_transient_memory());
    if (!result) {
        return {};
    }
    return { .value = all_names[result.value], .distance = result.distance };
}

const Directive_Behavior* Builtin_Directive_Set::operator()(std::u8string_view name) const
{
    const auto* const it
        = std::ranges::lower_bound(behaviors_by_name, name, {}, &Name_And_Behavior::name);
    if (it == std::end(behaviors_by_name) || it->name != name) {
        return nullptr;
    }
    return it->behavior;
}

} // namespace cowel
