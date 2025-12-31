#ifndef COWEL_HTML_NAMES_HPP
#define COWEL_HTML_NAMES_HPP

#include <optional>
#include <string_view>
#include <type_traits>

#include "ulight/impl/lang/html.hpp"

#include "cowel/util/assert.hpp"

namespace cowel {

/// @brief Returns `true` if `str` is a valid HTML tag identifier.
/// This includes both builtin tag names (which are purely alphabetic)
/// and custom tag names.
[[nodiscard]]
constexpr bool is_html_tag_name(std::u8string_view str)
{
    return ulight::html::is_tag_name(str);
}

/// @brief Returns `true` if `str` is a valid HTML attribute name.
[[nodiscard]]
constexpr bool is_html_attribute_name(std::u8string_view str)
{
    return ulight::html::is_attribute_name(str);
}

/// @brief Returns `true` if the given string requires no wrapping in quotes when it
/// appears as the value in an attribute.
/// For example, `id=123` is a valid HTML attribute with a value and requires
/// no wrapping, but `id="<x>"` requires `<x>` to be surrounded by quotes.
[[nodiscard]]
constexpr bool is_html_unquoted_attribute_value(std::u8string_view str)
{
    return ulight::html::is_unquoted_attribute_value(str);
}

struct Unchecked { };

template <auto predicate>
    requires std::is_invocable_r_v<bool, decltype(predicate), std::u8string_view>
struct Predicated_String_View8 {

    static constexpr bool nothrow
        = noexcept(std::is_nothrow_invocable_r_v<bool, decltype(predicate), std::u8string_view>);

    [[nodiscard]]
    static std::optional<Predicated_String_View8> make(std::u8string_view str) noexcept(nothrow)
    {
        if (predicate(str)) {
            return Predicated_String_View8 { Unchecked {}, str };
        }
        return {};
    }

private:
    std::u8string_view m_string;

public:
    [[nodiscard]]
    // NOLINTNEXTLINE(bugprone-exception-escape)
    constexpr explicit Predicated_String_View8(std::u8string_view str) noexcept(nothrow)
        : m_string { str }
    {
        COWEL_ASSERT(predicate(str));
    }

    [[nodiscard]]
    constexpr Predicated_String_View8(Unchecked, std::u8string_view str) noexcept
        : m_string { str }
    {
        COWEL_DEBUG_ASSERT(predicate(str));
    }

    [[nodiscard]]
    constexpr operator std::u8string_view() const noexcept
    {
        return m_string;
    }

    [[nodiscard]]
    constexpr std::u8string_view str() const noexcept
    {
        return m_string;
    }
};

using HTML_Tag_Name = Predicated_String_View8<is_html_tag_name>;
using HTML_Attribute_Name = Predicated_String_View8<is_html_attribute_name>;

namespace html_tag {

inline constexpr HTML_Tag_Name a { u8"a" };
inline constexpr HTML_Tag_Name b { u8"b" };
inline constexpr HTML_Tag_Name body { u8"body" };
inline constexpr HTML_Tag_Name br { u8"br" };
inline constexpr HTML_Tag_Name div { u8"div" };
inline constexpr HTML_Tag_Name h1 { u8"h1" };
inline constexpr HTML_Tag_Name h2 { u8"h2" };
inline constexpr HTML_Tag_Name h3 { u8"h3" };
inline constexpr HTML_Tag_Name h4 { u8"h4" };
inline constexpr HTML_Tag_Name h5 { u8"h5" };
inline constexpr HTML_Tag_Name h6 { u8"h6" };
inline constexpr HTML_Tag_Name head { u8"head" };
inline constexpr HTML_Tag_Name html { u8"html" };
inline constexpr HTML_Tag_Name link { u8"link" };
inline constexpr HTML_Tag_Name main { u8"main" };
inline constexpr HTML_Tag_Name math { u8"math" };
inline constexpr HTML_Tag_Name meta { u8"meta" };
inline constexpr HTML_Tag_Name ol { u8"ol" };
inline constexpr HTML_Tag_Name p { u8"p" };
inline constexpr HTML_Tag_Name script { u8"script" };
inline constexpr HTML_Tag_Name span { u8"span" };
inline constexpr HTML_Tag_Name style { u8"style" };
inline constexpr HTML_Tag_Name title { u8"title" };
inline constexpr HTML_Tag_Name ul { u8"ul" };

inline constexpr HTML_Tag_Name error_ { u8"error-" };
inline constexpr HTML_Tag_Name g_term { u8"g-term" };
inline constexpr HTML_Tag_Name h_ { u8"h-" };
inline constexpr HTML_Tag_Name intro_ { u8"intro-" };
inline constexpr HTML_Tag_Name tt_ { u8"tt-" };
inline constexpr HTML_Tag_Name wg21_block { u8"wg21-block" };

} // namespace html_tag

namespace html_attr {

inline constexpr HTML_Attribute_Name charset { u8"charset" };
inline constexpr HTML_Attribute_Name class_ { u8"class" };
inline constexpr HTML_Attribute_Name content { u8"content" };
inline constexpr HTML_Attribute_Name crossorigin { u8"crossorigin" };
inline constexpr HTML_Attribute_Name display { u8"display" };
inline constexpr HTML_Attribute_Name hidden { u8"hidden" };
inline constexpr HTML_Attribute_Name href { u8"href" };
inline constexpr HTML_Attribute_Name id { u8"id" };
inline constexpr HTML_Attribute_Name name { u8"name" };
inline constexpr HTML_Attribute_Name rel { u8"rel" };
inline constexpr HTML_Attribute_Name src { u8"src" };
inline constexpr HTML_Attribute_Name tabindex { u8"tabindex" };

inline constexpr HTML_Attribute_Name data_h { u8"data-h" };
inline constexpr HTML_Attribute_Name data_level { u8"data-level" };

} // namespace html_attr

} // namespace cowel

#endif
