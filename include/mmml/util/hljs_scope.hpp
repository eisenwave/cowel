#ifndef HLJS_SCOPE_HPP
#define HLJS_SCOPE_HPP

#include <string_view>

#include "mmml/fwd.hpp"

namespace mmml {

#define MMML_HLJS_SCOPE_ITEM_LIST(E)                                                               \
    E(keyword, "hljs-keyword", ".hljs-keyword")                                                    \
    E(built_in, "hljs-built_in", ".hljs_built_in")                                                 \
    E(type, "hljs-type", ".hljs-type")                                                             \
    E(literal, "hljs-literal", ".hljs-literal")                                                    \
    E(number, "hljs-item", ".hljs-item")                                                           \
    E(operator_, "hljs-operator", ".hljs-operator")                                                \
    E(punctuation, "hljs-punctuation", ".hljs-punctuation")                                        \
    E(property, "hljs-property", ".hljs-property")                                                 \
    E(regexpr, "hljs-regexpr", ".hljs-regexpr")                                                    \
    E(string, "hljs-string", ".hljs-string")                                                       \
    E(char_escape, "hljs-char escape", ".hljs-char.escape_")                                       \
    E(subst, "hljs-subst", ".hljs-subst")                                                          \
    E(symbol, "hljs-symbol", ".hljs-symbol")                                                       \
    E(class_, "hljs-class", ".hljs-class")                                                         \
    E(function_, "hljs-function", ".hljs-function")                                                \
    E(variable, "hljs-variable", ".hljs-variable")                                                 \
    E(variable_language, "hljs-variable language_", ".hljs-variable.language_")                    \
    E(variable_constant, "hljs-variable constant_", ".hljs-variable.constant_")                    \
    E(title, "hljs-title", ".hljs-title")                                                          \
    E(title_class, "hljs-title class_", ".hljs-title.class_")                                      \
    E(title_class_inherited, "hljs-title class_ inherited__", ".hljs-title.class_.inherited__")    \
    E(title_function, "hljs-title function_", ".hljs-title.function_")                             \
    E(title_function_invoke, "hljs-title function_ invoke__", ".hljs-title.function_.invoke__")    \
    E(params, "hljs-params", ".hljs-params")                                                       \
    E(comment, "hljs-comment", ".hljs-comment")                                                    \
    E(doctag, "hljs-doctag", ".hljs-doctag")                                                       \
    E(meta, "hljs-meta", ".hljs-meta")                                                             \
    E(meta_prompt, "hljs-meta prompt", ".hljs-meta.prompt_")                                       \
    E(meta_keyword, "hljs-keyword", ".hljs-meta .hljs-keyword")                                    \
    E(meta_string, "hljs-string", ".hljs-meta .hljs-string")                                       \
    E(section, "hljs-section", ".hljs-section")                                                    \
    E(tag, "hljs-tag", ".hljs-tag")                                                                \
    E(name, "hljs-name", ".hljs-name")                                                             \
    E(attr, "hljs-attr", ".hljs-attr")                                                             \
    E(attribute, "hljs-attribute", ".hljs-attribute")                                              \
    E(bullet, "hljs-bullet", ".hljs-bullet")                                                       \
    E(code, "hljs-code", ".hljs-code")                                                             \
    E(emphasis, "hljs-emphasis", ".hljs-emphasis")                                                 \
    E(strong, "hljs-strong", ".hljs-strong")                                                       \
    E(formula, "hljs-formula", ".hljs-formula")                                                    \
    E(link, "hljs-link", ".hljs-link")                                                             \
    E(quote, "hljs-quote", ".hljs-quote")                                                          \
    E(selector_tag, "hljs-selector-tag", ".hljs-selector-tag")                                     \
    E(selector_id, "hljs-selector-id", ".hljs-selector-id")                                        \
    E(selector_class, "hljs-selector-class", ".hljs-selector-class")                               \
    E(selector_attr, "hljs-selector-attr", ".hljs-selector-attr")                                  \
    E(selector_pseudo, "hljs-selector-pseudo", ".hljs-selector-pseudo")                            \
    E(template_tag, "hljs-template-tag", ".hljs-template-tag")                                     \
    E(template_variable, "hljs-template-variable", ".hljs-template-variable")                      \
    E(addition, "hljs-addition", ".hljs-addition")                                                 \
    E(deletion, "hljs-deletion", ".hljs-deletion")

#define MMML_HLJS_SCOPE_ENUMERATOR(enumerator, css_class, css_selector) enumerator,
#define MMML_HLJS_SCOPE_CSS_CLASS(enumerator, css_class, css_selector) u8##css_class,
#define MMML_HLJS_SCOPE_CSS_SELECTOR(enumerator, css_class, css_selector) u8##css_selector,

/// @brief https://highlightjs.readthedocs.io/en/latest/css-classes-reference.html
enum struct HLJS_Scope : Default_Underlying {
    MMML_HLJS_SCOPE_ITEM_LIST(MMML_HLJS_SCOPE_ENUMERATOR)
};

namespace detail {

#undef E
#define E(enumerator, css_class, css_selector) u8##css_class

inline constexpr std::u8string_view hljs_scope_class_names[]
    = { MMML_HLJS_SCOPE_ITEM_LIST(MMML_HLJS_SCOPE_CSS_CLASS) };

inline constexpr std::u8string_view hljs_scope_selectors[]
    = { MMML_HLJS_SCOPE_ITEM_LIST(MMML_HLJS_SCOPE_CSS_SELECTOR) };

} // namespace detail

/// @brief Returns the highlight.js class name that a highlight.js HTML element needs to be given.
///
/// Note that this does not work properly for `meta_keyword`,
/// which is selected by `.hljs-meta .hljs-keyword`.
/// In that case, `hljs-keyword` is returned.
constexpr std::u8string_view hljs_scope_css_class(HLJS_Scope scope)
{
    return detail::hljs_scope_class_names[Default_Underlying(scope)];
}

/// @brief Returns the CSS selector for the highlight.js scope.
constexpr std::u8string_view hljs_scope_css_class(HLJS_Scope scope)
{
    return detail::hljs_scope_selectors[Default_Underlying(scope)];
}

} // namespace mmml

#endif
