#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <memory_resource>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "mmml/util/typo.hpp"

#include "mmml/base_behaviors.hpp"
#include "mmml/builtin_directive_set.hpp"
#include "mmml/directive_behavior.hpp"
#include "mmml/directive_processing.hpp"
#include "mmml/fwd.hpp"

namespace mmml {

struct Builtin_Directive_Set::Impl {
    Special_Block_Behavior abstract //
        { u8"abstract-block" };
    Fixed_Name_Passthrough_Behavior b //
        { u8"b", Directive_Category::formatting, Directive_Display::in_line };
    Bibliography_Add_Behavior bib //
        {};
    Wrap_Behavior block //
        { Directive_Category::formatting, Directive_Display::block, To_HTML_Mode::direct };
    Special_Block_Behavior blockquote //
        { u8"blockquote", false };
    Self_Closing_Behavior br //
        { u8"br", diagnostic::br_content_ignored, Directive_Display::in_line };
    Special_Block_Behavior bug //
        { u8"bug-block" };
    HTML_Entity_Behavior c //
        {};
    Syntax_Highlight_Behavior code //
        { u8"code", Directive_Display::in_line, To_HTML_Mode::direct };
    Syntax_Highlight_Behavior codeblock //
        { u8"code-block", Directive_Display::block, To_HTML_Mode::trimmed };
    Do_Nothing_Behavior comment //
        { Directive_Category::meta, Directive_Display::none };
    Fixed_Name_Passthrough_Behavior dd //
        { u8"dd", Directive_Category::pure_html, Directive_Display::block };
    Special_Block_Behavior decision //
        { u8"decision-block" };
    Def_Behavior def //
        {};
    Fixed_Name_Passthrough_Behavior del //
        { u8"del", Directive_Category::formatting, Directive_Display::in_line };
    Special_Block_Behavior delblock //
        { u8"del-block", false };
    Fixed_Name_Passthrough_Behavior details //
        { u8"details", Directive_Category::pure_html, Directive_Display::block };
    Special_Block_Behavior diff //
        { u8"diff-block", false };
    Fixed_Name_Passthrough_Behavior dl //
        { u8"dl", Directive_Category::pure_html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior dt //
        { u8"dt", Directive_Category::pure_html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior em //
        { u8"em", Directive_Category::formatting, Directive_Display::in_line };
    Error_Behavior error //
        {};
    Special_Block_Behavior example //
        { u8"example-block" };
    Fixed_Name_Passthrough_Behavior gterm //
        { u8"g-term", Directive_Category::formatting, Directive_Display::in_line };
    Heading_Behavior h1 //
        { 1 };
    Heading_Behavior h2 //
        { 2 };
    Heading_Behavior h3 //
        { 3 };
    Heading_Behavior h4 //
        { 4 };
    Heading_Behavior h5 //
        { 5 };
    Heading_Behavior h6 //
        { 6 };
    Here_Behavior here //
        { Directive_Display::in_line };
    Here_Behavior hereblock //
        { Directive_Display::block };
    Self_Closing_Behavior hr //
        { u8"hr", diagnostic::hr_content_ignored, Directive_Display::block };
    HTML_Literal_Behavior html //
        { Directive_Display::in_line };
    HTML_Literal_Behavior htmlblock //
        { Directive_Display::block };
    Directive_Name_Passthrough_Behavior html_tags //
        { Directive_Category::pure_html, Directive_Display::block, html_tag_prefix };
    Fixed_Name_Passthrough_Behavior i //
        { u8"i", Directive_Category::formatting, Directive_Display::in_line };
    Special_Block_Behavior insblock //
        { u8"ins-block", false };
    Special_Block_Behavior important //
        { u8"important-block" };
    In_Tag_Behavior indent //
        { u8"div", u8"indent", Directive_Display::in_line };
    Wrap_Behavior in_line //
        { Directive_Category::formatting, Directive_Display::in_line, To_HTML_Mode::direct };
    Fixed_Name_Passthrough_Behavior ins //
        { u8"ins", Directive_Category::formatting, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior kbd //
        { u8"kbd", Directive_Category::formatting, Directive_Display::in_line };
    Lorem_Ipsum_Behavior lorem_ipsum //
        {};
    Macro_Behavior macro //
        {};
    URL_Behavior mail //
        { u8"mailto:" };
    Make_Section_Behavior make_bibliography //
        { Directive_Display::block, class_name::bibliography, section_name::bibliography };
    Make_Section_Behavior make_contents //
        { Directive_Display::block, class_name::table_of_contents,
          section_name::table_of_contents };
    Fixed_Name_Passthrough_Behavior mark //
        { u8"mark", Directive_Category::formatting, Directive_Display::in_line };
    Math_Behavior math //
        { Directive_Display::in_line };
    Math_Behavior mathblock //
        { Directive_Display::block };
    Special_Block_Behavior note //
        { u8"note-block" };
    List_Behavior ol //
        { u8"ol" };
    Fixed_Name_Passthrough_Behavior p //
        { u8"p", Directive_Category::pure_html, Directive_Display::block };
    Wrap_Behavior paragraphs //
        { Directive_Category::formatting, Directive_Display::block, To_HTML_Mode::paragraphs };
    Fixed_Name_Passthrough_Behavior q //
        { u8"q", Directive_Category::formatting, Directive_Display::in_line };
    Ref_Behavior ref //
        {};
    Fixed_Name_Passthrough_Behavior s //
        { u8"s", Directive_Category::formatting, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior sans //
        { u8"f-sans", Directive_Category::formatting, Directive_Display::in_line };
    HTML_Raw_Text_Behavior script //
        { u8"script" };
    Fixed_Name_Passthrough_Behavior serif //
        { u8"f-serif", Directive_Category::formatting, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior small //
        { u8"small", Directive_Category::formatting, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior strong //
        { u8"strong", Directive_Category::formatting, Directive_Display::in_line };
    HTML_Raw_Text_Behavior style //
        { u8"style" };
    Fixed_Name_Passthrough_Behavior sub //
        { u8"sub", Directive_Category::formatting, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior summary //
        { u8"summary", Directive_Category::pure_html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior sup //
        { u8"sup", Directive_Category::formatting, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior table //
        { u8"table", Directive_Category::pure_html, Directive_Display::block };
    URL_Behavior tel //
        { u8"tel:" };
    Fixed_Name_Passthrough_Behavior tbody //
        { u8"tbody", Directive_Category::pure_html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior td //
        { u8"td", Directive_Category::pure_html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior th //
        { u8"th", Directive_Category::pure_html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior thead //
        { u8"thead", Directive_Category::pure_html, Directive_Display::block };
    There_Behavior there //
        {};
    Special_Block_Behavior tip //
        { u8"tip-block" };
    Special_Block_Behavior todo //
        { u8"todo-block" };
    Fixed_Name_Passthrough_Behavior tr //
        { u8"tr", Directive_Category::pure_html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior tt //
        { u8"tt-", Directive_Category::formatting, Directive_Display::in_line };
    Code_Point_Behavior U //
        {};
    Fixed_Name_Passthrough_Behavior u //
        { u8"u", Directive_Category::formatting, Directive_Display::in_line };
    List_Behavior ul //
        { u8"ul" };
    URL_Behavior url //
        {};
    Get_Variable_Behavior vget //
        {};
    Modify_Variable_Behavior vset //
        { Variable_Operation::set };
    Special_Block_Behavior warning //
        { u8"warning-block" };
    WG21_Block_Behavior wg21_example //
        { u8"Example", u8"end example" };
    In_Tag_Behavior wg21_grammar //
        { u8"dl", u8"grammar", Directive_Display::block };
    WG21_Head_Behavior wg21_head //
        {};
    WG21_Block_Behavior wg21_note //
        { u8"Note", u8"end note" };

    Impl() = default;
};

Builtin_Directive_Set::Builtin_Directive_Set()
    : m_impl(std::make_unique<Builtin_Directive_Set::Impl>())
{
}

Builtin_Directive_Set::~Builtin_Directive_Set() = default;

Directive_Behavior& Builtin_Directive_Set::get_error_behavior() noexcept
{
    return m_impl->error;
}

Directive_Behavior& Builtin_Directive_Set::get_macro_behavior() noexcept
{
    return m_impl->macro;
}

Distant<std::u8string_view> Builtin_Directive_Set::fuzzy_lookup_name(
    std::u8string_view name,
    std::pmr::memory_resource* memory
) const
{
    // clang-format off
    static constexpr std::u8string_view prefixed_names[] {
        u8"-U",
        u8"-abstract",
        u8"-b",
        u8"-bib",
        u8"-block",
        u8"-blockquote",
        u8"-br",
        u8"-bug",
        u8"-c",
        u8"-code",
        u8"-codeblock",
        u8"-comment",
        u8"-dd",
        u8"-decision",
        u8"-def",
        u8"-del",
        u8"-delblock",
        u8"-details",
        u8"-diff",
        u8"-dl",
        u8"-dt",
        u8"-em",
        u8"-error",
        u8"-example",
        u8"-gterm",
        u8"-h1",
        u8"-h2",
        u8"-h3",
        u8"-h4",
        u8"-h5",
        u8"-h6",
        u8"-here",
        u8"-hereblock",
        u8"-hr",
        u8"-html",
        u8"-html-",
        u8"-htmlblock",
        u8"-i",
        u8"-important",
        u8"-indent",
        u8"-inline",
        u8"-ins",
        u8"-insblock",
        u8"-item",
        u8"-k",
        u8"-kbd",
        u8"-lorem-ipsum",
        u8"-mail",
        u8"-make-bib",
        u8"-make-contents",
        u8"-mark",
        u8"-math",
        u8"-mathblock",
        u8"-note",
        u8"-ol",
        u8"-p",
        u8"-paragraphs",
        u8"-q",
        u8"-ref",
        u8"-s",
        u8"-sans",
        u8"-script",
        u8"-serif",
        u8"-small",
        u8"-strong",
        u8"-style",
        u8"-sub",
        u8"-summary",
        u8"-sup",
        u8"-table",
        u8"-tbody",
        u8"-td",
        u8"-tel",
        u8"-th",
        u8"-thead",
        u8"-tip",
        u8"-todo",
        u8"-trow",
        u8"-tt",
        u8"-u",
        u8"-ul",
        u8"-url",
        u8"-vget",
        u8"-vset",
        u8"-warning",
        u8"-wg21-example",
        u8"-wg21-grammar",
        u8"-wg21-head",
        u8"-wg21-note",
    };
    // clang-format on
    static_assert(std::ranges::is_sorted(prefixed_names));
    static_assert(prefixed_names[0][0] == builtin_directive_prefix);

    static constexpr auto all_names = [] {
        std::array<std::u8string_view, std::size(prefixed_names) * 2> result;
        std::ranges::copy(prefixed_names, result.data());
        std::ranges::copy(
            prefixed_names
                | std::views::transform([](std::u8string_view n) { return n.substr(1); }),
            result.data() + std::size(prefixed_names)
        );
        return result;
    }();
    const Distant<std::size_t> result = closest_match(all_names, name, memory);
    if (!result) {
        return {};
    }
    return { .value = all_names[result.value], .distance = result.distance };
}

Directive_Behavior* Builtin_Directive_Set::operator()(std::u8string_view name) const
{
    // Any builtin names should be found with both `\\-directive` and `\\directive`.
    // `\\def` does not permit defining directives with a hyphen prefix,
    // so this lets the user
    if (name.starts_with(builtin_directive_prefix)) {
        return (*this)(name.substr(1));
    }
    if (name.empty()) {
        return nullptr;
    }
    // NOLINTBEGIN(readability-braces-around-statements)
    switch (name[0]) {
    case u8'a':
        if (name == u8"abstract")
            return &m_impl->abstract;
        break;

    case u8'b':
        if (name == u8"b")
            return &m_impl->b;
        if (name == u8"bib")
            return &m_impl->bib;
        if (name == u8"block")
            return &m_impl->block;
        if (name == u8"blockquote")
            return &m_impl->blockquote;
        if (name == u8"br")
            return &m_impl->br;
        if (name == u8"bug")
            return &m_impl->bug;
        break;

    case u8'c':
        if (name == u8"c")
            return &m_impl->c;
        if (name == u8"code")
            return &m_impl->code;
        if (name == u8"codeblock")
            return &m_impl->codeblock;
        if (name == u8"comment")
            return &m_impl->comment;
        break;

    case u8'd':
        if (name == u8"dd")
            return &m_impl->dd;
        if (name == u8"decision")
            return &m_impl->decision;
        if (name == u8"def")
            return &m_impl->def;
        if (name == u8"del")
            return &m_impl->del;
        if (name == u8"delblock")
            return &m_impl->delblock;
        if (name == u8"details")
            return &m_impl->details;
        if (name == u8"diff")
            return &m_impl->diff;
        if (name == u8"dl")
            return &m_impl->dl;
        if (name == u8"dt")
            return &m_impl->dt;
        break;

    case u8'e':
        if (name == u8"em")
            return &m_impl->em;
        if (name == u8"error")
            return &m_impl->error;
        if (name == u8"example")
            return &m_impl->example;
        break;

    case u8'g':
        if (name == u8"gterm")
            return &m_impl->gterm;
        break;

    case u8'h':
        if (name == u8"h1")
            return &m_impl->h1;
        if (name == u8"h2")
            return &m_impl->h2;
        if (name == u8"h3")
            return &m_impl->h3;
        if (name == u8"h4")
            return &m_impl->h4;
        if (name == u8"h5")
            return &m_impl->h5;
        if (name == u8"h6")
            return &m_impl->h6;
        if (name == u8"here")
            return &m_impl->here;
        if (name == u8"hereblock")
            return &m_impl->hereblock;
        if (name == u8"hr")
            return &m_impl->hr;
        if (name == u8"html")
            return &m_impl->html;
        if (name == u8"htmlblock")
            return &m_impl->htmlblock;
        static_assert(html_tag_prefix[0] == 'h');
        if (name.starts_with(html_tag_prefix))
            return &m_impl->html_tags;
        break;

    case u8'i':
        if (name == u8"i")
            return &m_impl->i;
        if (name == u8"important")
            return &m_impl->important;
        if (name == u8"indent")
            return &m_impl->indent;
        if (name == u8"inline")
            return &m_impl->in_line;
        if (name == u8"ins")
            return &m_impl->ins;
        if (name == u8"insblock")
            return &m_impl->insblock;
        break;

    case u8'k':
        if (name == u8"kbd")
            return &m_impl->kbd;
        break;

    case u8'l':
        if (name == u8"lorem-ipsum")
            return &m_impl->lorem_ipsum;
        break;

    case u8'm':
        if (name == u8"mail")
            return &m_impl->mail;
        if (name == u8"make-bib")
            return &m_impl->make_bibliography;
        if (name == u8"make-contents")
            return &m_impl->make_contents;
        if (name == u8"mark")
            return &m_impl->mark;
        if (name == u8"math")
            return &m_impl->math;
        if (name == u8"mathblock")
            return &m_impl->mathblock;
        break;

    case u8'n':
        if (name == u8"note")
            return &m_impl->note;
        break;

    case u8'o':
        if (name == u8"ol")
            return &m_impl->ol;
        break;

    case u8'p':
        if (name == u8"p")
            return &m_impl->p;
        if (name == u8"paragraphs")
            return &m_impl->paragraphs;
        break;

    case u8'q':
        if (name == u8"q")
            return &m_impl->q;
        break;

    case u8'r':
        if (name == u8"ref")
            return &m_impl->ref;
        break;

    case u8's':
        if (name == u8"s")
            return &m_impl->s;
        if (name == u8"sans")
            return &m_impl->sans;
        if (name == u8"script")
            return &m_impl->script;
        if (name == u8"serif")
            return &m_impl->serif;
        if (name == u8"small")
            return &m_impl->small;
        if (name == u8"strong")
            return &m_impl->strong;
        if (name == u8"style")
            return &m_impl->style;
        if (name == u8"sub")
            return &m_impl->sub;
        if (name == u8"summary")
            return &m_impl->summary;
        if (name == u8"sup")
            return &m_impl->sup;
        break;

    case u8't':
        if (name == u8"table")
            return &m_impl->table;
        if (name == u8"tbody")
            return &m_impl->tbody;
        if (name == u8"td")
            return &m_impl->td;
        if (name == u8"tel")
            return &m_impl->tel;
        if (name == u8"th")
            return &m_impl->th;
        if (name == u8"thead")
            return &m_impl->thead;
        if (name == u8"there")
            return &m_impl->there;
        if (name == u8"tip")
            return &m_impl->tip;
        if (name == u8"todo")
            return &m_impl->todo;
        if (name == u8"tr")
            return &m_impl->tr;
        if (name == u8"tt")
            return &m_impl->tt;
        break;

    case u8'U':
        if (name == u8"U")
            return &m_impl->U;
        break;

    case u8'u':
        if (name == u8"u")
            return &m_impl->u;
        if (name == u8"ul")
            return &m_impl->ul;
        if (name == u8"url")
            return &m_impl->url;
        break;

    case u8'v':
        if (name == u8"vget")
            return &m_impl->vget;
        if (name == u8"vset")
            return &m_impl->vset;
        break;

    case u8'w':
        if (name == u8"warning")
            return &m_impl->warning;
        if (name == u8"wg21-example")
            return &m_impl->wg21_example;
        if (name == u8"wg21-grammar")
            return &m_impl->wg21_grammar;
        if (name == u8"wg21-head")
            return &m_impl->wg21_head;
        if (name == u8"wg21-note")
            return &m_impl->wg21_note;
        break;

    default: break;
    }
    // NOLINTEND(readability-braces-around-statements)

    return nullptr;
}

} // namespace mmml
