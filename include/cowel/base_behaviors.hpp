#ifndef COWEL_BASE_BEHAVIORS_HPP
#define COWEL_BASE_BEHAVIORS_HPP

#include <span>
#include <vector>

#include "cowel/ast.hpp"
#include "cowel/directive_arguments.hpp"
#include "cowel/directive_behavior.hpp"

namespace cowel {

struct Pure_HTML_Behavior : Directive_Behavior {

    constexpr Pure_HTML_Behavior(Directive_Display display)
        : Directive_Behavior { Directive_Category::pure_html, display }
    {
    }

    void generate_plaintext(std::pmr::vector<char8_t>&, const ast::Directive&, Context&) const final
    {
    }
};

struct Pure_Plaintext_Behavior : Directive_Behavior {

    constexpr Pure_Plaintext_Behavior(Directive_Display display)
        : Directive_Behavior { Directive_Category::pure_plaintext, display }
    {
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const final
    {
        std::pmr::vector<char8_t> text { context.get_transient_memory() };
        generate_plaintext(text, d, context);
        out.write_inner_text({ text.data(), text.size() });
    }
};

struct Meta_Behavior : Directive_Behavior {

    constexpr explicit Meta_Behavior()
        : Directive_Behavior { Directive_Category::meta, Directive_Display::none }
    {
    }

    void generate_plaintext(std::pmr::vector<char8_t>&, const ast::Directive& d, Context& context)
        const final
    {
        evaluate(d, context);
    }

    void generate_html(HTML_Writer&, const ast::Directive& d, Context& context) const final
    {
        evaluate(d, context);
    }

    virtual void evaluate(const ast::Directive& d, Context& context) const = 0;
};

/// @brief A base behavior for macro directives.
/// The generation of plaintext and HTML is implemented in terms of `instantiate()`,
/// i.e. we simply instantiate the macro and generate output from the instantiated contents.
struct Instantiated_Behavior : Directive_Behavior {

    constexpr Instantiated_Behavior()
        : Directive_Behavior { Directive_Category::macro, Directive_Display::macro }
    {
    }

    void generate_plaintext(std::pmr::vector<char8_t>& out, const ast::Directive&, Context&)
        const override;

    void generate_html(HTML_Writer& out, const ast::Directive&, Context&) const override;
};

struct Do_Nothing_Behavior : Directive_Behavior {
    // TODO: diagnose ignored arguments

    constexpr Do_Nothing_Behavior(Directive_Category category, Directive_Display display)
        : Directive_Behavior { category, display }
    {
    }

    void
    generate_plaintext(std::pmr::vector<char8_t>&, const ast::Directive&, Context&) const override
    {
    }

    void generate_html(HTML_Writer&, const ast::Directive&, Context&) const override { }
};

struct Parametric_Behavior : Directive_Behavior {
protected:
    const std::span<const std::u8string_view> m_parameters;

public:
    constexpr Parametric_Behavior(
        Directive_Category c,
        Directive_Display d,
        std::span<const std::u8string_view> parameters
    )
        : Directive_Behavior { c, d }
        , m_parameters { parameters }
    {
    }

    void
    generate_plaintext(std::pmr::vector<char8_t>& out, const ast::Directive& d, Context& context)
        const override
    {
        Argument_Matcher args { m_parameters, context.get_transient_memory() };
        args.match(d.get_arguments());
        generate_plaintext(out, d, args, context);
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const override
    {
        Argument_Matcher args { m_parameters, context.get_transient_memory() };
        args.match(d.get_arguments());
        generate_html(out, d, args, context);
    }

protected:
    virtual void generate_plaintext(
        std::pmr::vector<char8_t>& out,
        const ast::Directive& d,
        const Argument_Matcher& args,
        Context& context
    ) const
        = 0;
    virtual void generate_html(
        HTML_Writer& out,
        const ast::Directive& d,
        const Argument_Matcher& args,
        Context& context
    ) const
        = 0;
};

} // namespace cowel

#endif
