#ifndef MMML_PROCESSING_HPP
#define MMML_PROCESSING_HPP

#include <filesystem>
#include <memory_resource>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "mmml/fwd.hpp"

namespace mmml {

struct Name_Resolver {
    virtual Directive_Behavior* operator()(std::u8string_view name) const = 0;
};

struct Transparent_String_View_Hash {
    using is_transparent = void;

    std::size_t operator()(std::u8string_view v) const
    {
        return std::hash<std::u8string_view> {}(v);
    }
};

struct Transparent_String_View_Equals {
    using is_transparent = void;

    bool operator()(std::u8string_view x, std::u8string_view y) const
    {
        return x == y;
    }
};

/// @brief Stores contextual information during document processing.
/// Such an object is created once per processing phase.
struct Context {
public:
    using Variable_Map = std::pmr::unordered_map<
        std::pmr::u8string,
        std::pmr::u8string,
        Transparent_String_View_Hash,
        Transparent_String_View_Equals>;

private:
    /// @brief The path at which the document is located.
    std::filesystem::path m_document_path;
    /// @brief Additional memory used during processing.
    std::pmr::memory_resource* m_memory;
    mutable std::pmr::unsynchronized_pool_resource m_transient_memory { m_memory };
    /// @brief Source code of the document.
    std::u8string_view m_source;
    /// @brief A list of (non-null) name resolvers.
    /// Whenever directives have to be looked up,
    /// these are processed from
    /// last to first (i.e. from most recently added) to determine which
    /// `Directive_Behavior` should handle a given directive.
    std::pmr::vector<Name_Resolver*> m_name_resolvers { &m_transient_memory };

public:
    Variable_Map m_variables;

    explicit Context(
        std::filesystem::path path,
        std::u8string_view source,
        std::pmr::memory_resource* memory
    )
        : m_document_path { path }
        , m_memory { memory }
        , m_source { source }
    {
    }

    [[nodiscard]]
    const std::filesystem::path& get_document_path() const
    {
        return m_document_path;
    }

    /// @brief Returns a memory resource that the `Context` has been constructed with.
    /// This may possibly persist beyond the destruction of the context.
    ///
    /// Note that the context is destroyed after the preprocessing pass,
    /// so this may be useful to retain some information between passes (e.g. caches).
    [[nodiscard]]
    std::pmr::memory_resource* get_persistent_memory() const
    {
        return m_memory;
    }

    /// @brief Returns a memory resource that is destroyed with this object.
    /// Note that contexts are destroyed after each pass, so this should only be used for
    /// temporary memory.
    [[nodiscard]]
    std::pmr::memory_resource* get_transient_memory() const
    {
        return &m_transient_memory;
    }

    [[nodiscard]]
    std::u8string_view get_source() const
    {
        return m_source;
    }

    void add_resolver(Name_Resolver& resolver)
    {
        m_name_resolvers.push_back(&resolver);
    }

    /// @brief Finds a directive behavior using `name_resolvers` in reverse order.
    /// @param name the name of the directive
    /// @return the behavior for the given name, or `nullptr` if none could be found
    [[nodiscard]]
    Directive_Behavior* find_directive(std::u8string_view name) const;

    /// @brief Equivalent to `find_directive(directive.get_name(source))`.
    [[nodiscard]]
    Directive_Behavior* find_directive(const ast::Directive& directive) const;
};

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
    /// @brief Mixed plaintext and HTML content.
    /// This is a fallback category for when none of the other options apply.
    /// Using it as an HTML attribute is not erroneous, but may lead to unexpected results.
    /// For syntax highlighting, this is treated same as `pure_html`.
    mixed,
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
};

/// @brief Implements behavior that one or multiple directives should have.
struct Directive_Behavior {
    const Directive_Category category;
    const Directive_Display display;

    Directive_Behavior(Directive_Category c, Directive_Display d)
        : category { c }
        , display { d }
    {
    }

    /// @brief Performs any preprocessing (first pass) on the document.
    /// This typically includes registering IDs that can be referenced by other directives,
    /// registering meta-information etc.
    ///
    /// This function shall only be invoked during preprocessing.
    ///
    /// Note that preprocessing can rewrite the AST.
    /// A classic example of that is `\\code{\\b{void}}`,
    /// which may apply syntax highlighting so to produce
    /// `\\code{\\b{\\hl-keyword{void}}}`.
    virtual void preprocess(ast::Directive& d, Context&) = 0;

    virtual void
    generate_plaintext(std::pmr::vector<char8_t>& out, const ast::Directive& d, Context&) const
        = 0;

    virtual void generate_html(HTML_Writer& out, const ast::Directive& d, Context&) const = 0;
};

struct [[nodiscard]] Builtin_Directive_Set : Name_Resolver {
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

public:
    Builtin_Directive_Set();

    Builtin_Directive_Set(const Builtin_Directive_Set&) = delete;
    Builtin_Directive_Set& operator=(const Builtin_Directive_Set&) = delete;

    ~Builtin_Directive_Set();

    [[nodiscard]]
    Directive_Behavior* operator()(std::u8string_view name) const override;
};

} // namespace mmml

#endif
