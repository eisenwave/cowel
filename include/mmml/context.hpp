#ifndef MMML_CONTEXT_HPP
#define MMML_CONTEXT_HPP

#include <filesystem>
#include <memory_resource>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "mmml/util/transparent_comparison.hpp"

#include "mmml/diagnostic.hpp"
#include "mmml/fwd.hpp"
#include "mmml/util/function_ref.hpp"

namespace mmml {

struct Name_Resolver {
    virtual Directive_Behavior* operator()(std::u8string_view name) const = 0;
};

/// @brief Stores contextual information during document processing.
/// Such an object is created once per processing phase.
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
    std::filesystem::path m_document_path;
    /// @brief Additional memory used during processing.
    std::pmr::memory_resource* m_memory;
    std::pmr::memory_resource* m_transient_memory;
    /// @brief Source code of the document.
    string_view_type m_source;
    /// @brief A list of (non-null) name resolvers.
    /// Whenever directives have to be looked up,
    /// these are processed from
    /// last to first (i.e. from most recently added) to determine which
    /// `Directive_Behavior` should handle a given directive.
    std::pmr::vector<const Name_Resolver*> m_name_resolvers { m_transient_memory };
    Directive_Behavior* m_error_behavior;

public:
    Variable_Map m_variables { m_memory };

private:
    Function_Ref<void(Diagnostic&&)> m_emit_diagnostic;
    Severity m_min_diagnostic;

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
        std::filesystem::path path,
        string_view_type source,
        Function_Ref<void(Diagnostic&&)> emit_diagnostic,
        Severity min_diagnostic_level,
        Directive_Behavior* error_behavior,
        std::pmr::memory_resource* persistent_memory,
        std::pmr::memory_resource* transient_memory
    )
        : m_document_path { path }
        , m_memory { persistent_memory }
        , m_transient_memory { transient_memory }
        , m_source { source }
        , m_error_behavior { error_behavior }
        , m_emit_diagnostic { emit_diagnostic }
        , m_min_diagnostic { min_diagnostic_level }
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

    /// @brief Returns the inclusive minimum level of diagnostics that are currently emitted.
    /// This may be `none`, in which case no diagnostic are emitted.
    [[nodiscard]]
    Severity get_min_diagnostic_level() const
    {
        return m_min_diagnostic;
    }

    /// @brief Equivalent to `get_min_diagnostic_level() >= severity`.
    [[nodiscard]]
    bool emits(Severity severity) const
    {
        return severity < Severity::min && severity >= m_min_diagnostic;
    }

    void emit(Diagnostic&& diagnostic) const
    {
        MMML_ASSERT(emits(diagnostic.severity));
        m_emit_diagnostic(std::move(diagnostic));
    }

    /// @brief Equivalent to `emit(make_diagnostic(severity, location, message))`.
    void emit(Severity severity, Source_Span location, string_view_type message) const
    {
        MMML_ASSERT(emits(severity));
        emit(make_diagnostic(severity, location, message));
    }

    /// @brief Equivalent to `emit(Severity::debug, location, message)`.
    void emit_debug(Source_Span location, string_view_type message) const
    {
        emit(Severity::debug, location, message);
    }

    /// @brief Equivalent to `emit(Severity::soft_warning, location, message)`.
    void emit_soft_warning(Source_Span location, string_view_type message) const
    {
        emit(Severity::soft_warning, location, message);
    }

    /// @brief Equivalent to `emit(Severity::warning, location, message)`.
    void emit_warning(Source_Span location, string_view_type message) const
    {
        emit(Severity::warning, location, message);
    }

    /// @brief Equivalent to `emit(Severity::error, location, message)`.
    void emit_error(Source_Span location, string_view_type message) const
    {
        emit(Severity::error, location, message);
    }

    /// @brief Returns a diagnostic with the given `severity` and using `get_persistent_memory()`
    /// as a memory resource for the message.
    /// The message is empty.
    /// @param severity The diagnostic severity. `emits(severity)` shall be `true`.
    /// @param location The location within the file where the diagnostic was generated.
    [[nodiscard]]
    Diagnostic make_diagnostic(Severity severity, Source_Span location) const
    {
        MMML_ASSERT(emits(severity));
        return { .severity = severity, .location = location, .message { get_persistent_memory() } };
    }

    /// @brief Like `make_diagnostic(severity)`,
    /// but initializes the result's message using the given `message`.
    [[nodiscard]]
    Diagnostic make_diagnostic(Severity severity, Source_Span location, string_view_type message) const
    {
        MMML_ASSERT(emits(severity));
        return { .severity = severity,
                 .location = location,
                 .message { message, get_persistent_memory() } };
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
};

} // namespace mmml

#endif
