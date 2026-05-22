#ifndef COWEL_CONTEXT_HPP
#define COWEL_CONTEXT_HPP

#include <memory_resource>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence.hpp"
#include "cowel/util/small_vector.hpp"
#include "cowel/util/stringify.hpp"
#include "cowel/util/transparent_comparison.hpp"

#include "cowel/call_stack.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_behavior.hpp"
#include "cowel/document_sections.hpp"
#include "cowel/fwd.hpp"
#include "cowel/services.hpp"

#include "cowel/syntax/ast.hpp"

namespace cowel {

struct Name_Resolver {
    [[nodiscard]]
    virtual Distant<std::u8string_view>
    fuzzy_lookup_name(std::u8string_view name, Context& context) const = 0;

    [[nodiscard]]
    virtual const Directive_Behavior* operator()(std::u8string_view name) const
        = 0;
};

struct Referred {
    std::u8string_view mask_html;
};

struct Macro_Definition final : Block_Directive_Behavior {
private:
    std::pmr::u8string m_name;
    std::pmr::u8string m_decl;
    std::pmr::vector<ast::Markup_Element> m_body;

public:
    [[nodiscard]]
    explicit Macro_Definition(
        std::pmr::vector<ast::Markup_Element>&& body,
        std::pmr::u8string&& name,
        std::pmr::u8string&& decl
    )
        : m_name { std::move(name) }
        , m_decl { std::move(decl) }
        , m_body { std::move(body) }
    {
        set_tooltip_article(
            Tooltip_Article {
                .kind = Tooltip_Kind::macro,
                .subject = m_name,
                .declaration_language = u8"cowel",
                .declaration = m_decl,
            }
        );
    }

    [[nodiscard]]
    Processing_Status splice(Content_Policy& out, const Invocation&, Context&) const final;
};

/// @brief A hover entry: source location and Markdown article text.
struct Hover_Entry {
    File_Source_Span span;
    std::pmr::u8string article;
};

/// @brief Stores contextual information during document processing.
struct Context {
public:
    using char_type = char8_t;
    using string_type = std::pmr::u8string;
    using string_view_type = std::u8string_view;

    using Variable_Map = std::pmr::unordered_map<
        string_type,
        Value,
        Transparent_String_View_Hash8,
        Transparent_String_View_Equals8>;
    using Macro_Map = std::pmr::unordered_map<
        std::pmr::u8string,
        Macro_Definition,
        Transparent_String_View_Hash8,
        Transparent_String_View_Equals8>;
    using Alias_Map = std::pmr::unordered_map<
        std::pmr::u8string,
        const Directive_Behavior*,
        Transparent_String_View_Hash8,
        Transparent_String_View_Equals8>;
    using ID_Map = std::pmr::unordered_map<
        std::pmr::u8string,
        Referred,
        Transparent_String_View_Hash8,
        Transparent_String_View_Equals8>;

private:
    struct Scoped_Diagnostic_Frame {
    private:
        Small_Vector<Frame_Index, 8>* m_frames = nullptr;

    public:
        explicit Scoped_Diagnostic_Frame(Small_Vector<Frame_Index, 8>& frames, Frame_Index frame)
            : m_frames { &frames }
        {
            m_frames->push_back(frame);
        }

        Scoped_Diagnostic_Frame(const Scoped_Diagnostic_Frame&) = delete;
        Scoped_Diagnostic_Frame& operator=(const Scoped_Diagnostic_Frame&) = delete;

        Scoped_Diagnostic_Frame(Scoped_Diagnostic_Frame&& other) noexcept
            : m_frames { std::exchange(other.m_frames, nullptr) }
        {
        }

        Scoped_Diagnostic_Frame& operator=(Scoped_Diagnostic_Frame&&) = delete;

        ~Scoped_Diagnostic_Frame()
        {
            if (m_frames) {
                m_frames->pop_back();
            }
        }
    };

    /// @brief Additional memory used during processing.
    std::pmr::memory_resource* m_memory;
    std::pmr::memory_resource* m_transient_memory;
    /// @brief JSON source code of the syntax highlighting theme.
    string_view_type m_highlight_theme_source;
    /// @brief Map of ids (as in, `id` attributes in HTML elements)
    /// to information about the reference.
    ID_Map m_id_references { m_transient_memory };
    Alias_Map m_aliases { m_transient_memory };
    Macro_Map m_macros { m_transient_memory };
    const Directive_Behavior* m_error_behavior;

    const Name_Resolver& m_builtin_name_resolver;
    File_Loader& m_file_loader;
    Logger* m_logger;
    Syntax_Highlighter& m_syntax_highlighter;

    Document_Sections m_sections { m_memory };
    Variable_Map m_variables { m_memory };

    Call_Stack m_call_stack { m_memory };
    Small_Vector<Frame_Index, 8> m_active_diagnostic_frames;
    Small_Vector<File_Source_Span, 8> m_diagnostic_stack;
    std::pmr::vector<Hover_Entry>* m_hover_sink = nullptr;

public:
    /// @brief Constructs a new context.
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
        string_view_type highlight_theme_source,
        const Directive_Behavior* error_behavior,
        const Name_Resolver& builtin_name_resolver,
        File_Loader& file_loader,
        Logger& logger,
        Syntax_Highlighter& highlighter,
        std::pmr::memory_resource* persistent_memory,
        std::pmr::memory_resource* transient_memory
    )
        : m_memory { persistent_memory }
        , m_transient_memory { transient_memory }
        , m_highlight_theme_source { highlight_theme_source }
        , m_error_behavior { error_behavior }
        , m_builtin_name_resolver { builtin_name_resolver }
        , m_file_loader { file_loader }
        , m_logger { &logger }
        , m_syntax_highlighter { highlighter }
    {
    }

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    ~Context() = default;

    [[nodiscard]]
    File_Loader& get_file_loader()
    {
        return m_file_loader;
    }
    [[nodiscard]]
    const File_Loader& get_file_loader() const
    {
        return m_file_loader;
    }

    [[nodiscard]]
    Logger& get_logger()
    {
        return *m_logger;
    }
    [[nodiscard]]
    const Logger& get_logger() const
    {
        return *m_logger;
    }
    void set_logger(Logger& logger)
    {
        m_logger = &logger;
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
    Call_Stack& get_call_stack()
    {
        return m_call_stack;
    }
    [[nodiscard]]
    const Call_Stack& get_call_stack() const
    {
        return m_call_stack;
    }

    [[nodiscard]]
    Scoped_Diagnostic_Frame push_diagnostic_frame(Frame_Index frame)
    {
        return Scoped_Diagnostic_Frame { m_active_diagnostic_frames, frame };
    }

    [[nodiscard]]
    Value* get_variable(string_view_type key)
    {
        const auto it = m_variables.find(key);
        return it == m_variables.end() ? nullptr : &it->second;
    }
    [[nodiscard]]
    const Value* get_variable(string_view_type key) const
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

    /// @brief Sets the sink into which hover entries are pushed during processing.
    /// Only used when `collect_hovers` is true in the generation options.
    void set_hover_sink(std::pmr::vector<Hover_Entry>& sink) noexcept
    {
        m_hover_sink = &sink;
    }

    /// @brief Returns true if hover entries are being collected.
    [[nodiscard]]
    bool collects_hovers() const noexcept
    {
        return m_hover_sink != nullptr;
    }

    /// @brief Records a hover entry at @p span with @p article.
    /// Does nothing if `collects_hovers()` is false.
    void push_hover(const File_Source_Span& span, const std::u8string_view article)
    {
        if (m_hover_sink != nullptr) {
            m_hover_sink->push_back({ span, std::pmr::u8string { article, m_memory } });
        }
    }

    [[nodiscard]]
    const Directive_Behavior* get_error_behavior() const
    {
        return m_error_behavior;
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
        return m_logger->get_min_severity();
    }

    /// @brief Equivalent to `get_min_diagnostic_level() >= severity`.
    [[nodiscard]]
    bool emits(Severity severity) const
    {
        return m_logger->can_log(severity);
    }

    void emit(Diagnostic diagnostic)
    {
        COWEL_ASSERT(emits(diagnostic.severity));
        diagnostic.stack = collect_diagnostic_stack();
        (*m_logger)(diagnostic);
    }

    void emit(
        Severity severity,
        string_view_type id,
        const File_Source_Span& location,
        Char_Sequence8 message
    )
    {
        COWEL_ASSERT(emits(severity));
        emit({ severity, id, location, message, {} });
    }

    template <formattable... Args>
    void emit_f(
        const Severity severity,
        const string_view_type id,
        const File_Source_Span& location,
        const string_view_type format,
        const Args&... args
    )
    {
        COWEL_ASSERT(emits(severity));
        emit_f_impl(
            severity, id, location, format, //
            // This looks really scary,
            // and it all needs to be in one expression to avoid lifetime issues
            // as well as object slicing.
            std::initializer_list<const Format_To*> {
                &static_cast<const Format_To&>(Format_Stringify_To<Args> { args })...,
            }
        );
    }

    void emit_trace(string_view_type id, const File_Source_Span& location, Char_Sequence8 message)
    {
        emit(Severity::trace, id, location, message);
    }

    void emit_debug(string_view_type id, const File_Source_Span& location, Char_Sequence8 message)
    {
        emit(Severity::debug, id, location, message);
    }

    void emit_info(string_view_type id, const File_Source_Span& location, Char_Sequence8 message)
    {
        emit(Severity::info, id, location, message);
    }

    void
    emit_soft_warning(string_view_type id, const File_Source_Span& location, Char_Sequence8 message)
    {
        emit(Severity::soft_warning, id, location, message);
    }

    void emit_warning(string_view_type id, const File_Source_Span& location, Char_Sequence8 message)
    {
        emit(Severity::warning, id, location, message);
    }

    void emit_error(string_view_type id, const File_Source_Span& location, Char_Sequence8 message)
    {
        emit(Severity::error, id, location, message);
    }

    void emit_fatal(string_view_type id, const File_Source_Span& location, Char_Sequence8 message)
    {
        emit(Severity::fatal, id, location, message);
    }

    void try_emit(Diagnostic diagnostic)
    {
        if (emits(diagnostic.severity)) {
            emit(diagnostic);
        }
    }

    void try_emit(
        const Severity severity,
        const string_view_type id,
        const File_Source_Span& location,
        Char_Sequence8 message
    )
    {
        if (emits(severity)) {
            emit({ severity, id, location, message, {} });
        }
    }

    template <formattable... Args>
    void try_emit_f(
        const Severity severity,
        const string_view_type id,
        const File_Source_Span& location,
        const string_view_type format,
        const Args&... args
    )
    {
        if (emits(severity)) {
            emit_f(severity, id, location, format, args...);
        }
    }

    void try_trace(string_view_type id, const File_Source_Span& location, Char_Sequence8 message)
    {
        try_emit(Severity::trace, id, location, message);
    }

    template <formattable... Args>
    void try_trace_f(
        const string_view_type id,
        const File_Source_Span& location,
        const string_view_type format,
        const Args&... args
    )
    {
        try_emit_f(Severity::trace, id, location, format, args...);
    }

    void try_debug(string_view_type id, const File_Source_Span& location, Char_Sequence8 message)
    {
        try_emit(Severity::debug, id, location, message);
    }

    template <formattable... Args>
    void try_debug_f(
        const string_view_type id,
        const File_Source_Span& location,
        const string_view_type format,
        const Args&... args
    )
    {
        try_debug_f(Severity::debug, id, location, format, args...);
    }

    void try_info(string_view_type id, const File_Source_Span& location, Char_Sequence8 message)
    {
        try_emit(Severity::info, id, location, message);
    }

    template <formattable... Args>
    void try_info_f(
        const string_view_type id,
        const File_Source_Span& location,
        const string_view_type format,
        const Args&... args
    )
    {
        try_emit_f(Severity::info, id, location, format, args...);
    }

    void
    try_soft_warning(string_view_type id, const File_Source_Span& location, Char_Sequence8 message)
    {
        try_emit(Severity::soft_warning, id, location, message);
    }

    template <formattable... Args>
    void try_soft_warning_f(
        const string_view_type id,
        const File_Source_Span& location,
        const string_view_type format,
        const Args&... args
    )
    {
        try_emit_f(Severity::soft_warning, id, location, format, args...);
    }

    void try_warning(string_view_type id, const File_Source_Span& location, Char_Sequence8 message)
    {
        try_emit(Severity::warning, id, location, message);
    }

    template <formattable... Args>
    void try_warning_f(
        const string_view_type id,
        const File_Source_Span& location,
        const string_view_type format,
        const Args&... args
    )
    {
        try_emit_f(Severity::warning, id, location, format, args...);
    }

    void try_error(string_view_type id, const File_Source_Span& location, Char_Sequence8 message)
    {
        try_emit(Severity::error, id, location, message);
    }

    template <formattable... Args>
    void try_error_f(
        const string_view_type id,
        const File_Source_Span& location,
        const string_view_type format,
        const Args&... args
    )
    {
        try_emit_f(Severity::error, id, location, format, args...);
    }

    void try_fatal(string_view_type id, const File_Source_Span& location, Char_Sequence8 message)
    {
        try_emit(Severity::fatal, id, location, message);
    }

    template <formattable... Args>
    void try_fatal_f(
        const string_view_type id,
        const File_Source_Span& location,
        const string_view_type format,
        const Args&... args
    )
    {
        try_emit_f(Severity::fatal, id, location, format, args...);
    }

    [[nodiscard]]
    const Directive_Behavior* find_directive(string_view_type name);

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

    [[nodiscard]]
    const Directive_Behavior* find_alias(string_view_type name) const
    {
        const auto it = m_aliases.find(name);
        return it == m_aliases.end() ? nullptr : it->second;
    }

    [[nodiscard]]
    bool emplace_alias(std::pmr::u8string&& name, const Directive_Behavior* behavior)
    {
        COWEL_ASSERT(behavior);
        const auto [_, success] = m_aliases.emplace(std::move(name), behavior);
        return success;
    }

    [[nodiscard]]
    const Macro_Definition* find_macro(std::u8string_view id) const
    {
        const auto it = m_macros.find(id);
        return it == m_macros.end() ? nullptr : &it->second;
    }

    [[nodiscard]]
    bool emplace_macro(
        std::pmr::u8string&& name,
        std::span<const ast::Markup_Element> definition,
        std::u8string_view macro_source
    );

private:
    [[nodiscard]]
    std::span<const File_Source_Span> collect_diagnostic_stack()
    {
        // TODO: This technically gets us what we want,
        //       but is a bit backwards.
        //       We push diagnostic frames to `m_active_diagnostic_frames`
        //       whenever some invocation happens,
        //       and filter ones we don't want in here.
        //       This seems preventable if we only push diagnostic frames
        //       in this places we would keep them.
        //       For example, we want to keep macro expansions in the diagnostic stack,
        //       but not invocations of builtin directives.
        //
        //       However, in the future the number of builtin directives in active use goes down,
        //       so maybe this doesn't matter and we should just expand the whole call stack
        //       unfiltered.
        m_diagnostic_stack.clear();
        const Frame_Index diagnostic_frame = m_active_diagnostic_frames.empty()
            ? Frame_Index::root
            : m_active_diagnostic_frames.back();

        bool has_previous_content_frame = false;
        Frame_Index previous_content_frame {};

        for (std::size_t i = m_call_stack.size(); i > 0; --i) {
            const Stack_Frame& frame = m_call_stack[Frame_Index(static_cast<int>(i - 1))];
            const Frame_Index content_frame = frame.invocation.content_frame;
            if (content_frame == diagnostic_frame) {
                continue;
            }
            if (has_previous_content_frame && content_frame == previous_content_frame) {
                continue;
            }

            m_diagnostic_stack.push_back(frame.invocation.directive.get_name_span());
            previous_content_frame = content_frame;
            has_previous_content_frame = true;
        }

        return m_diagnostic_stack;
    }

    struct Format_To {
        virtual void operator()(std::pmr::u8string& out) const = 0;
    };

    /// @brief Adaptor which uses `Stringify` for the given type `T` to implement `Format_To`.
    template <class T>
    struct Format_Stringify_To final : Format_To {
        const T& value;
        [[nodiscard]]
        Format_Stringify_To(const T& value) noexcept
            : value { value }
        {
        }
        void operator()(std::pmr::u8string& out) const override
        {
            Stringify<T> {}.append(out, value);
        }
    };

    void emit_f_impl(
        const Severity severity,
        const string_view_type id,
        const File_Source_Span& location,
        const string_view_type format,
        const std::span<const Format_To* const> args
    )
    {
        std::pmr::u8string result { m_transient_memory };
        result.reserve(format.size() * 2);

        std::size_t arg_index = 0;
        for (std::size_t i = 0; i < format.size();) {
            if (format[i] == u8'{') {
                COWEL_ASSERT(i + 1 < format.length());
                if (format[i + 1] == u8'{') {
                    result += u8'{';
                    i += 2;
                }
                else if (format[i + 1] == u8'}') {
                    (*args[arg_index++])(result);
                    i += 2;
                }
                else {
                    COWEL_ASSERT_UNREACHABLE(u8"Only {{ and {} are supported for now.");
                }
            }
            else if (format[i] == u8'}') {
                COWEL_ASSERT(i + 1 < format.length() && format[i + 1] == u8'}');
                result += u8'}';
                i += 2;
            }
            else {
                result += format[i];
                ++i;
            }
        }
        emit(severity, id, location, std::u8string_view { result });
    }
};

} // namespace cowel

#endif
