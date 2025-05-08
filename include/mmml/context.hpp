#ifndef MMML_CONTEXT_HPP
#define MMML_CONTEXT_HPP

#include <filesystem>
#include <memory_resource>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "mmml/util/assert.hpp"
#include "mmml/util/transparent_comparison.hpp"
#include "mmml/util/typo.hpp"

#include "mmml/diagnostic.hpp"
#include "mmml/document_sections.hpp"
#include "mmml/fwd.hpp"
#include "mmml/services.hpp"

namespace mmml {

namespace detail {

// We only use the aliases from fwd.hpp, but we need the definition of
// `Basic_Transparent_String_View_Equals` to actually make this usable.
using Suppress_Unused_Include_Transparent_Equals = Basic_Transparent_String_View_Equals<void>;

} // namespace detail

struct Name_Resolver {
    [[nodiscard]]
    virtual Distant<std::u8string_view>
    fuzzy_lookup_name(std::u8string_view name, std::pmr::memory_resource* memory) const = 0;

    [[nodiscard]]
    virtual Directive_Behavior* operator()(std::u8string_view name) const
        = 0;
};

struct Referred {
    std::u8string_view mask_html;
};

/// @brief Stores contextual information during document processing.
struct Context {
public:
    using char_type = char8_t;
    using string_type = std::pmr::u8string;
    using string_view_type = std::u8string_view;

    using Variable_Map = std::pmr::unordered_map<
        string_type,
        string_type,
        Transparent_String_View_Hash8,
        Transparent_String_View_Equals8>;

private:
    /// @brief The path at which the document is located.
    const std::filesystem::path& m_document_path;
    /// @brief Additional memory used during processing.
    std::pmr::memory_resource* m_memory;
    std::pmr::memory_resource* m_transient_memory;
    /// @brief Source code of the document.
    string_view_type m_source;
    /// @brief JSON source code of the syntax highlighting theme.
    string_view_type m_highlight_theme_source;
    /// @brief A list of (non-null) name resolvers.
    /// Whenever directives have to be looked up,
    /// these are processed from
    /// last to first (i.e. from most recently added) to determine which
    /// `Directive_Behavior` should handle a given directive.
    std::pmr::vector<const Name_Resolver*> m_name_resolvers { m_transient_memory };
    /// @brief Map of ids (as in, `id` attributes in HTML elements)
    /// to information about the reference.
    std::pmr::unordered_map<
        std::pmr::u8string,
        Referred,
        Transparent_String_View_Hash8,
        Transparent_String_View_Equals8>
        m_id_references { m_transient_memory };
    Directive_Behavior* m_error_behavior;

    Logger& m_logger;
    Syntax_Highlighter& m_syntax_highlighter;
    Bibliography& m_bibliography;

    Document_Sections m_sections { m_memory };
    Variable_Map m_variables { m_memory };

public:
    /// @brief Constructs a new context.
    /// @param path The file path of the current document.
    /// @param source The source code.
    /// @param emit_diagnostic Called when a diagnostic is emitted.
    /// @param min_diagnostic_level The minimum level of diagnostics that are emitted.
    /// @param error_behavior The behavior to be used when directive processing encounters an error.
    /// May be null.
    /// @param persistent_memory Additional memory which persists beyond the destruction
    /// of the context.
    /// This is used e.g. for the creation of diagnostic messages,
    /// for directive behavior that requires storing information between passes, etc.
    /// @param transient_memory Additional memory which does not persist beyond the
    /// destruction of the context.
    explicit Context(
        const std::filesystem::path& path,
        string_view_type source,
        string_view_type highlight_theme_source,
        Directive_Behavior* error_behavior,
        Logger& logger,
        Syntax_Highlighter& highlighter,
        Bibliography& bibliography,
        std::pmr::memory_resource* persistent_memory,
        std::pmr::memory_resource* transient_memory
    )
        : m_document_path { path }
        , m_memory { persistent_memory }
        , m_transient_memory { transient_memory }
        , m_source { source }
        , m_highlight_theme_source { highlight_theme_source }
        , m_error_behavior { error_behavior }
        , m_logger { logger }
        , m_syntax_highlighter { highlighter }
        , m_bibliography { bibliography }
    {
    }

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    ~Context() = default;

    [[nodiscard]]
    const std::filesystem::path& get_document_path() const
    {
        return m_document_path;
    }

    [[nodiscard]]
    Logger& get_logger()
    {
        return m_logger;
    }
    [[nodiscard]]
    const Logger& get_logger() const
    {
        return m_logger;
    }

    [[nodiscard]]
    Syntax_Highlighter& get_highlighter()
    {
        return m_syntax_highlighter;
    }
    [[nodiscard]]
    const Syntax_Highlighter& get_highlighter() const
    {
        return m_syntax_highlighter;
    }

    [[nodiscard]]
    Bibliography& get_documents()
    {
        return m_bibliography;
    }
    [[nodiscard]]
    const Bibliography& get_documents() const
    {
        return m_bibliography;
    }

    [[nodiscard]]
    Variable_Map& get_variables()
    {
        return m_variables;
    }

    [[nodiscard]]
    const Variable_Map& get_variables() const
    {
        return m_variables;
    }

    [[nodiscard]]
    string_type* get_variable(string_view_type key)
    {
        const auto it = m_variables.find(key);
        return it == m_variables.end() ? nullptr : &it->second;
    }
    [[nodiscard]]
    const string_type* get_variable(string_view_type key) const
    {
        const auto it = m_variables.find(key);
        return it == m_variables.end() ? nullptr : &it->second;
    }

    [[nodiscard]]
    Document_Sections& get_sections()
    {
        return m_sections;
    }

    [[nodiscard]]
    const Document_Sections& get_sections() const
    {
        return m_sections;
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
        return m_transient_memory;
    }

    [[nodiscard]]
    Directive_Behavior* get_error_behavior() const
    {
        return m_error_behavior;
    }

    [[nodiscard]]
    string_view_type get_source() const
    {
        return m_source;
    }

    [[nodiscard]]
    string_view_type get_highlight_theme_source() const
    {
        return m_highlight_theme_source;
    }

    /// @brief Returns the inclusive minimum level of diagnostics that are currently emitted.
    /// This may be `none`, in which case no diagnostic are emitted.
    [[nodiscard]]
    Severity get_min_diagnostic_level() const
    {
        return m_logger.get_min_severity();
    }

    /// @brief Equivalent to `get_min_diagnostic_level() >= severity`.
    [[nodiscard]]
    bool emits(Severity severity) const
    {
        return m_logger.can_log(severity);
    }

    void emit(Diagnostic&& diagnostic)
    {
        MMML_ASSERT(emits(diagnostic.severity));
        m_logger(std::move(diagnostic));
    }

    /// @brief Equivalent to `emit(make_diagnostic(severity, id, location, message))`.
    void
    try_emit(Severity severity, string_view_type id, Source_Span location, string_view_type message)
    {
        if (emits(severity)) {
            emit(make_diagnostic(severity, id, location, message));
        }
    }

    void try_debug(string_view_type id, Source_Span location, string_view_type message)
    {
        try_emit(Severity::debug, id, location, message);
    }

    void try_soft_warning(string_view_type id, Source_Span location, string_view_type message)
    {
        try_emit(Severity::soft_warning, id, location, message);
    }

    void try_warning(string_view_type id, Source_Span location, string_view_type message)
    {
        try_emit(Severity::warning, id, location, message);
    }

    void try_error(string_view_type id, Source_Span location, string_view_type message)
    {
        try_emit(Severity::error, id, location, message);
    }

    /// @brief Returns a diagnostic with the given `severity` and using `get_persistent_memory()`
    /// as a memory resource for the message.
    /// The message is empty.
    /// @param severity The diagnostic severity. `emits(severity)` shall be `true`.
    /// @param location The location within the file where the diagnostic was generated.
    [[nodiscard]]
    Diagnostic make_diagnostic(Severity severity, std::u8string_view id, Source_Span location) const
    {
        MMML_ASSERT(emits(severity));
        return { .severity = severity,
                 .id = id,
                 .location = location,
                 .message = std::pmr::u8string { get_persistent_memory() } };
    }

    [[nodiscard]]
    Diagnostic make_debug(std::u8string_view id, Source_Span location) const
    {
        return make_diagnostic(Severity::debug, id, location);
    }

    [[nodiscard]]
    Diagnostic make_soft_warning(std::u8string_view id, Source_Span location) const
    {
        return make_diagnostic(Severity::soft_warning, id, location);
    }

    [[nodiscard]]
    Diagnostic make_warning(std::u8string_view id, Source_Span location) const
    {
        return make_diagnostic(Severity::warning, id, location);
    }

    [[nodiscard]]
    Diagnostic make_error(std::u8string_view id, Source_Span location) const
    {
        return make_diagnostic(Severity::error, id, location);
    }

    /// @brief Like `make_diagnostic(severity)`,
    /// but initializes the result's message using the given `message`.
    [[nodiscard]]
    Diagnostic make_diagnostic(
        Severity severity,
        std::u8string_view id,
        Source_Span location,
        string_view_type message
    ) const
    {
        MMML_ASSERT(emits(severity));
        return { .severity = severity,
                 .id = id,
                 .location = location,
                 .message = std::pmr::u8string { message, get_persistent_memory() } };
    }

    void add_resolver(const Name_Resolver& resolver)
    {
        m_name_resolvers.push_back(&resolver);
    }

    /// @brief Finds a directive behavior using `name_resolvers` in reverse order.
    /// @param name the name of the directive
    /// @return the behavior for the given name, or `nullptr` if none could be found
    [[nodiscard]]
    Directive_Behavior* find_directive(string_view_type name) const;

    /// @brief Equivalent to `find_directive(directive.get_name(source))`.
    [[nodiscard]]
    Directive_Behavior* find_directive(const ast::Directive& directive) const;

    [[nodiscard]]
    const Referred* find_id(std::u8string_view id) const
    {
        const auto it = m_id_references.find(id);
        return it == m_id_references.end() ? nullptr : &it->second;
    }

    [[nodiscard]]
    bool emplace_id(std::pmr::u8string&& id, const Referred& referred)
    {
        const auto [it, success] = m_id_references.try_emplace(std::move(id), referred);
        return success;
    }
};

} // namespace mmml

#endif
