#ifndef COWEL_TOOLTIP_HPP
#define COWEL_TOOLTIP_HPP

#include <string_view>

#include "cowel/fwd.hpp"

namespace cowel {

enum struct Tooltip_Kind : Default_Underlying {
    custom,
    numeric_escape,
    named_escape,
    escape,
    builtin_directive,
    macro,
};

struct Tooltip_Article {
    /// The kind of article.
    Tooltip_Kind kind = Tooltip_Kind::custom;
    /// The article subject,
    /// which describes what the article applies to.
    /// For example, if the article is for the directive `s`,
    /// `subject` is `"s"`.
    std::u8string_view subject;

    /// The language ID of the `declaration` code block.
    /// May be empty, in which case the declaration code block receives no highlighting.
    std::u8string_view declaration_language = {};
    /// The contents of the code block in which the declaration is shown.
    /// For example, for macros,
    /// this is the `cowel_macro` invocation that declared the macro.
    /// May be empty, in which case there is no declaration code block.
    std::u8string_view declaration = {};

    /// A detailed (possibly multi-paragraph) description.
    /// May be empty, in which case there is no description.
    std::u8string_view description = {};

    /// An example of how to use the subject.
    /// This is typically only shown for builtin directives.
    /// May be empty, in which case there is no example.
    std::u8string_view example = {};
};

[[nodiscard]]
constexpr std::u8string_view tooltip_kind_title(const Tooltip_Kind kind)
{
    using enum Tooltip_Kind;
    switch (kind) {
    case numeric_escape: return u8"Numeric escape";
    case named_escape: return u8"Named escape";
    case escape: return u8"Escape";
    case macro: return u8"Macro";
    default: return u8"Directive";
    }
}

} // namespace cowel

#endif
