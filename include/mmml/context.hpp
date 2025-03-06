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
    mutable std::pmr::unsynchronized_pool_resource m_transient_memory { m_memory };
    /// @brief Source code of the document.
    string_view_type m_source;
    /// @brief A list of (non-null) name resolvers.
    /// Whenever directives have to be looked up,
    /// these are processed from
    /// last to first (i.e. from most recently added) to determine which
    /// `Directive_Behavior` should handle a given directive.
    std::pmr::vector<Name_Resolver*> m_name_resolvers { &m_transient_memory };

public:
    Variable_Map m_variables { m_memory };

private:
    Function_Ref<void(Diagnostic&&)> m_emit_diagnostic;
    Diagnostic_Type m_min_diagnostic;

public:
    /// @brief Constructs a new context.
    /// @param path The file path of the current document.
    /// @param source The source code.
    /// @param emit_diagnostic Called when a diagnostic is emitted.
    /// @param min_diagnostic_level The minimum level of diagnostics that are emitted.
    /// @param memory Additional memory.
    explicit Context(
        std::filesystem::path path,
        string_view_type source,
        Function_Ref<void(Diagnostic&&)> emit_diagnostic,
        Diagnostic_Type min_diagnostic_level,
        std::pmr::memory_resource* memory
    )
        : m_document_path { path }
        , m_memory { memory }
        , m_source { source }
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
        return &m_transient_memory;
    }

    [[nodiscard]]
    string_view_type get_source() const
    {
        return m_source;
    }

    /// @brief Returns the inclusive minimum level of diagnostics that are currently emitted.
    /// This may be `none`, in which case no diagnostic are emitted.
    [[nodiscard]]
    Diagnostic_Type get_min_diagnostic_level() const
    {
        return m_min_diagnostic;
    }

    /// @brief Equivalent to `get_min_diagnostic_level() >= type`.
    [[nodiscard]]
    bool emits(Diagnostic_Type type) const
    {
        return type < Diagnostic_Type::all && type >= m_min_diagnostic;
    }

    void emit(Diagnostic&& diagnostic)
    {
        MMML_ASSERT(emits(diagnostic.type));
        m_emit_diagnostic(std::move(diagnostic));
    }

    /// @brief Equivalent to `emit(make_diagnostic(type, location, message))`.
    void emit(Diagnostic_Type type, Source_Span location, string_view_type message)
    {
        MMML_ASSERT(emits(type));
        m_emit_diagnostic(make_diagnostic(type, location, message));
    }

    /// @brief Returns a diagnostic with the given `type` and using `get_persistent_memory()`
    /// as a memory resource for the message.
    /// The message is empty.
    /// @param type The diagnostic type. `emits(type)` shall be `true`.
    /// @param location The location within the file where the diagnostic was generated.
    [[nodiscard]]
    Diagnostic make_diagnostic(Diagnostic_Type type, Source_Span location) const
    {
        MMML_ASSERT(emits(type));
        return { .type = type, .location = location, .message { get_persistent_memory() } };
    }

    /// @brief Like `make_diagnostic(type)`,
    /// but initializes the result's message using the given `message`.
    [[nodiscard]]
    Diagnostic
    make_diagnostic(Diagnostic_Type type, Source_Span location, string_view_type message) const
    {
        MMML_ASSERT(emits(type));
        return { .type = type,
                 .location = location,
                 .message { message, get_persistent_memory() } };
    }

    void add_resolver(Name_Resolver& resolver)
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
