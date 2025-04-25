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
    Do_Nothing_Behavior comment //
        { Directive_Category::meta, Directive_Display::none };
    Fixed_Name_Passthrough_Behavior b //
        { u8"b", Directive_Category::formatting, Directive_Display::in_line };
    HTML_Entity_Behavior c //
        {};
    Syntax_Highlight_Behavior code //
        { u8"code", Directive_Display::in_line, To_HTML_Mode::direct };
    Syntax_Highlight_Behavior codeblock //
        { u8"code-block", Directive_Display::block, To_HTML_Mode::trimmed };
    Fixed_Name_Passthrough_Behavior dd //
        { u8"dd", Directive_Category::pure_html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior del //
        { u8"del", Directive_Category::formatting, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior dl //
        { u8"dl", Directive_Category::pure_html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior dt //
        { u8"dt", Directive_Category::pure_html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior em //
        { u8"em", Directive_Category::formatting, Directive_Display::in_line };
    Error_Behavior error //
        {};
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
    HTML_Literal_Behavior html //
        {};
    Directive_Name_Passthrough_Behavior html_tags //
        { Directive_Category::pure_html, Directive_Display::block, html_tag_prefix };
    Fixed_Name_Passthrough_Behavior i //
        { u8"i", Directive_Category::formatting, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior ins //
        { u8"ins", Directive_Category::formatting, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior kbd //
        { u8"kbd", Directive_Category::formatting, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior mark //
        { u8"mark", Directive_Category::formatting, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior ol //
        { u8"ol", Directive_Category::pure_html, Directive_Display::block };
    Fixed_Name_Passthrough_Behavior s //
        { u8"s", Directive_Category::formatting, Directive_Display::in_line };
    HTML_Raw_Text_Behavior script //
        { u8"script" };
    Fixed_Name_Passthrough_Behavior small //
        { u8"small", Directive_Category::formatting, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior strong //
        { u8"strong", Directive_Category::formatting, Directive_Display::in_line };
    HTML_Raw_Text_Behavior style //
        { u8"style" };
    Fixed_Name_Passthrough_Behavior sub //
        { u8"sub", Directive_Category::formatting, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior sup //
        { u8"sup", Directive_Category::formatting, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior tt //
        { u8"tt-", Directive_Category::formatting, Directive_Display::in_line };
    Code_Point_Behavior U //
        {};
    Fixed_Name_Passthrough_Behavior u //
        { u8"u", Directive_Category::formatting, Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior ul //
        { u8"ul", Directive_Category::pure_html, Directive_Display::block };
    Get_Variable_Behavior vget //
        {};
    Modify_Variable_Behavior vset //
        { Variable_Operation::set };

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

Distant<std::u8string_view> Builtin_Directive_Set::fuzzy_lookup_name(
    std::u8string_view name,
    std::pmr::memory_resource* memory
) const
{
    // clang-format off
    static constexpr std::u8string_view prefixed_names[] {
        u8"-U",
        u8"-b",
        u8"-c",
        u8"-code",
        u8"-codeblock",
        u8"-comment",
        u8"-dd",
        u8"-del",
        u8"-dl",
        u8"-dt",
        u8"-em",
        u8"-error",
        u8"-h1",
        u8"-h2",
        u8"-h3",
        u8"-h4",
        u8"-h5",
        u8"-h6",
        u8"-html",
        u8"-html-",
        u8"-i",
        u8"-ins",
        u8"-k",
        u8"-kbd",
        u8"-mark",
        u8"-ol",
        u8"-s",
        u8"-script",
        u8"-small",
        u8"-strong",
        u8"-style",
        u8"-sub",
        u8"-sup",
        u8"-tt",
        u8"-u",
        u8"-ul",
        u8"-vget",
        u8"-vset",
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
    case u8'b':
        if (name == u8"b")
            return &m_impl->b;
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
        if (name == u8"del")
            return &m_impl->del;
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
        if (name == u8"html")
            return &m_impl->html;
        static_assert(html_tag_prefix[0] == 'h');
        if (name.starts_with(html_tag_prefix))
            return &m_impl->html_tags;
        break;

    case u8'i':
        if (name == u8"i")
            return &m_impl->i;
        if (name == u8"ins")
            return &m_impl->ins;
        break;

    case u8'k':
        if (name == u8"kbd")
            return &m_impl->kbd;
        break;

    case u8'm':
        if (name == u8"mark")
            return &m_impl->mark;
        break;

    case u8'o':
        if (name == u8"ol")
            return &m_impl->ol;
        break;

    case u8's':
        if (name == u8"s")
            return &m_impl->s;
        if (name == u8"script")
            return &m_impl->script;
        if (name == u8"small")
            return &m_impl->small;
        if (name == u8"strong")
            return &m_impl->strong;
        if (name == u8"style")
            return &m_impl->style;
        if (name == u8"sub")
            return &m_impl->sub;
        if (name == u8"sup")
            return &m_impl->sup;
        break;

    case u8't':
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
        break;

    case u8'v':
        if (name == u8"vget")
            return &m_impl->vget;
        if (name == u8"vset")
            return &m_impl->vset;
        break;

    default: break;
    }
    // NOLINTEND(readability-braces-around-statements)

    return nullptr;
}

} // namespace mmml
