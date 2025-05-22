#ifndef COWEL_DIRECTIVE_BEHAVIOR_HPP
#define COWEL_DIRECTIVE_BEHAVIOR_HPP

#include <vector>

#include "cowel/ast.hpp"
#include "cowel/context.hpp"
#include "cowel/fwd.hpp"

namespace cowel {

/// @brief A category which applies to a directive behavior generally,
/// regardless of the specific directive processed at the time.
///
/// These categories are important to guide how directives that are effectively
/// put into HTML attributes (e.g. `\\html-div[id=\\something]`) should be treated,
/// as well as how syntax highlighting interacts with a directive.
enum struct Directive_Category : Default_Underlying {
    /// @brief The directive generates no plaintext or HTML.
    /// For example, `\\comment`.
    meta,
    /// @brief The directive (regardless of input content or arguments)
    /// produces purely plaintext.
    ///
    /// During syntax highlighting, such directives are eliminated entirely,
    /// and integrated into the syntax-highlighted content.
    pure_plaintext,
    /// @brief Purely HTML content, such as `\\html{...}`.
    /// Such content produces no plaintext, and using it as an HTML attribute is erroneous.
    pure_html,
    /// @brief HTML formatting wrapper for content within.
    /// Using formatting inside of HTML attributes is erroneous.
    ///
    /// During syntax highlighting, the contents of formatting directives are
    /// replaced with highlighted contents.
    /// For example, `\\code{\\b{void}}` may be turned into `\\code{\\b{\\hl-keyword{void}}}`.
    formatting,
    /// @brief Directive which is replaced by other content,
    /// and doesn't have fixed behavior or display style.
    macro,
};

/// @brief Specifies how a directive should be displayed.
enum struct Directive_Display : Default_Underlying {
    /// @brief Nothing is displayed.
    none,
    /// @brief The directive is a block, such as `\\h1` or `\\codeblock`.
    /// Such directives are not integrated into other paragraphs or surround text.
    block,
    /// @brief The directive is inline, such as `\\b` or `\\code`.
    /// This means that it will be displayed within paragraphs and as part of other text.
    in_line,
    /// @brief The directive expands to other content;
    /// it has not display style on its own.
    macro,
};

/// @brief Implements behavior that one or multiple directives should have.
struct Directive_Behavior {
    const Directive_Category category;
    const Directive_Display display;

    constexpr Directive_Behavior(Directive_Category c, Directive_Display d)
        : category { c }
        , display { d }
    {
    }

    virtual void
    generate_plaintext(std::pmr::vector<char8_t>& out, const ast::Directive&, Context&) const
        = 0;

    virtual void generate_html(HTML_Writer& out, const ast::Directive&, Context&) const = 0;

    virtual void instantiate(std::pmr::vector<ast::Content>&, const ast::Directive&, Context&) const
    {
        COWEL_ASSERT_UNREACHABLE(u8"Instantiation unimplemented.");
    }

    [[nodiscard]]
    std::pmr::vector<char8_t> generate_plaintext(const ast::Directive& d, Context& context) const
    {
        std::pmr::vector<char8_t> result { context.get_transient_memory() };
        generate_plaintext(result, d, context);
        return result;
    }

    [[nodiscard]]
    std::pmr::vector<char8_t> generate_html(const ast::Directive& d, Context& context) const
    {
        std::pmr::vector<char8_t> result { context.get_transient_memory() };
        HTML_Writer writer { result };
        generate_html(writer, d, context);
        return result;
    }

    [[nodiscard]]
    std::pmr::vector<ast::Content> instantiate(const ast::Directive& d, Context& context) const
    {
        std::pmr::vector<ast::Content> result { context.get_transient_memory() };
        instantiate(result, d, context);
        return result;
    }
};

} // namespace cowel

#endif
