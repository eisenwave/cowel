#ifndef MMML_BUILTIN_DIRECTIVE_SET
#define MMML_BUILTIN_DIRECTIVE_SET

#include <string_view>

#include "mmml/util/html_writer.hpp"

#include "mmml/base_behaviors.hpp"
#include "mmml/directive_behavior.hpp"

namespace mmml {

inline constexpr char8_t builtin_directive_prefix = u8'-';
inline constexpr std::u8string_view html_tag_prefix = u8"html-";

/// @brief Behavior for `\\error` directives.
/// Does not processing.
/// Generates no plaintext.
/// Generates HTML with the source code of the contents wrapped in an `<error->` custom tag.
struct Error_Behavior : Do_Nothing_Behavior {
    static constexpr std::u8string_view id = u8"error-";

    Error_Behavior()
        : Do_Nothing_Behavior { Directive_Category::pure_html, Directive_Display::in_line }
    {
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const override
    {
        out.open_tag(id);
        out.write_inner_text(d.get_source(context.get_source()));
        out.close_tag(id);
    }
};

struct [[nodiscard]]
HTML_Entity_Behavior final : Directive_Behavior {
    HTML_Entity_Behavior()
        : Directive_Behavior { Directive_Category::pure_plaintext, Directive_Display::in_line }
    {
    }

    void
    generate_plaintext(std::pmr::vector<char8_t>& out, const ast::Directive& d, Context& context)
        const override;

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const final;
};

struct [[nodiscard]]
Code_Point_Behavior final : Directive_Behavior {

    Code_Point_Behavior()
        : Directive_Behavior { Directive_Category::pure_plaintext, Directive_Display::in_line }
    {
    }

    void
    generate_plaintext(std::pmr::vector<char8_t>& out, const ast::Directive& d, Context& context)
        const override;

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const final;
};

struct [[nodiscard]] Syntax_Highlight_Behavior : Parametric_Behavior {
private:
    static constexpr std::u8string_view lang_parameter = u8"lang";
    static constexpr std::u8string_view parameters[] { lang_parameter };

    const std::u8string_view m_tag_name;
    const To_HTML_Mode m_to_html_mode;

public:
    explicit Syntax_Highlight_Behavior(
        std::u8string_view tag_name,
        Directive_Display d,
        To_HTML_Mode mode
    )
        : Parametric_Behavior { Directive_Category::pure_html, d, parameters }
        , m_tag_name { tag_name }
        , m_to_html_mode { mode }
    {
    }

    void
    generate_plaintext(std::pmr::vector<char8_t>&, const ast::Directive&, const Argument_Matcher&, Context&)
        const override;

    void generate_html(
        HTML_Writer& out,
        const ast::Directive& d,
        const Argument_Matcher& args,
        Context& context
    ) const override;
};

struct HTML_Literal_Behavior : Pure_HTML_Behavior {

    HTML_Literal_Behavior()
        : Pure_HTML_Behavior { Directive_Display::block }
    {
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const override;
};

struct Variable_Behavior : Parametric_Behavior {
    static constexpr std::u8string_view var_parameter = u8"var";
    static constexpr std::u8string_view parameters[] { var_parameter };

    Variable_Behavior(Directive_Category c, Directive_Display d)
        : Parametric_Behavior { c, d, parameters }
    {
    }

    void generate_plaintext(
        std::pmr::vector<char8_t>& out,
        const ast::Directive& d,
        const Argument_Matcher& args,
        Context& context
    ) const override;

    void generate_html(
        HTML_Writer& out,
        const ast::Directive& d,
        const Argument_Matcher& args,
        Context& context
    ) const override;

protected:
    virtual void generate_var_plaintext(
        std::pmr::vector<char8_t>& out,
        const ast::Directive& d,
        std::u8string_view var,
        Context& context
    ) const
        = 0;

    virtual void generate_var_html(
        HTML_Writer& out,
        const ast::Directive& d,
        std::u8string_view var,
        Context& context
    ) const
        = 0;
};

struct Get_Variable_Behavior final : Variable_Behavior {

    Get_Variable_Behavior()
        : Variable_Behavior { Directive_Category::pure_plaintext, Directive_Display::in_line }
    {
    }

    void generate_var_plaintext(
        std::pmr::vector<char8_t>& out,
        const ast::Directive&,
        std::u8string_view var,
        Context& context
    ) const final;

    void generate_var_html(
        HTML_Writer& out,
        const ast::Directive&,
        std::u8string_view var,
        Context& context
    ) const final;
};

enum struct Variable_Operation : Default_Underlying {
    // TODO: add more operations
    set
};

void process(
    Variable_Operation op,
    const ast::Directive& d,
    std::u8string_view var,
    Context& context
);

struct Modify_Variable_Behavior final : Variable_Behavior {
private:
    const Variable_Operation m_op;

public:
    Modify_Variable_Behavior(Variable_Operation op)
        : Variable_Behavior { Directive_Category::meta, Directive_Display::none }
        , m_op { op }
    {
    }

    void generate_var_plaintext(
        std::pmr::vector<char8_t>&,
        const ast::Directive& d,
        std::u8string_view var,
        Context& context
    ) const final
    {
        process(m_op, d, var, context);
    }

    void generate_var_html(
        HTML_Writer&,
        const ast::Directive& d,
        std::u8string_view var,
        Context& context
    ) const final
    {
        process(m_op, d, var, context);
    }
};

struct Passthrough_Behavior : Directive_Behavior {

    Passthrough_Behavior(Directive_Category category, Directive_Display display)
        : Directive_Behavior { category, display }
    {
    }

    void
    generate_plaintext(std::pmr::vector<char8_t>& out, const ast::Directive& d, Context& context)
        const override;

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const override;

    [[nodiscard]]
    virtual std::u8string_view get_name(const ast::Directive& d, Context& context) const
        = 0;
};

/// @brief Behavior for any formatting tags that are mapped onto HTML with the same name.
/// This includes `\\i{...}`, `\\strong`, and many more.
///
/// Preprocesses and processes all arguments.
/// Generates the contents inside in plaintext.
///
/// Generates HTML where arguments are converted to HTML attributes,
/// in a tag that has the same name as the directive.
/// For example, `\\i[id = 123]{...}` generates `<i id=123>...</i>`.
struct Directive_Name_Passthrough_Behavior : Passthrough_Behavior {
private:
    const std::u8string_view m_name_prefix;

public:
    Directive_Name_Passthrough_Behavior(
        Directive_Category category,
        Directive_Display display,
        const std::u8string_view name_prefix = u8""
    )
        : Passthrough_Behavior { category, display }
        , m_name_prefix { name_prefix }
    {
    }

    [[nodiscard]]
    std::u8string_view get_name(const ast::Directive& d, Context& context) const override;
};

struct Fixed_Name_Passthrough_Behavior : Passthrough_Behavior {
private:
    const std::u8string_view m_name;

public:
    explicit Fixed_Name_Passthrough_Behavior(
        std::u8string_view name,
        Directive_Category category,
        Directive_Display display
    )
        : Passthrough_Behavior { category, display }
        , m_name { name }
    {
    }

    [[nodiscard]]
    std::u8string_view get_name(const ast::Directive&, Context&) const override
    {
        return m_name;
    }
};

struct [[nodiscard]]
Builtin_Directive_Set final : Name_Resolver {
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

public:
    Builtin_Directive_Set();

    Builtin_Directive_Set(const Builtin_Directive_Set&) = delete;
    Builtin_Directive_Set& operator=(const Builtin_Directive_Set&) = delete;

    ~Builtin_Directive_Set();

    [[nodiscard]]
    Directive_Behavior& get_error_behavior() noexcept;

    [[nodiscard]]
    Distant<std::u8string_view>
    fuzzy_lookup_name(std::u8string_view name, std::pmr::memory_resource* memory) const final;

    [[nodiscard]]
    Directive_Behavior* operator()(std::u8string_view name) const final;
};

} // namespace mmml

#endif
