#ifndef COWEL_DOCUMENT_GENERATION_HPP
#define COWEL_DOCUMENT_GENERATION_HPP

#include <memory_resource>
#include <span>
#include <string_view>

#include "cowel/util/function_ref.hpp"
#include "cowel/util/meta.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/content_status.hpp"
#include "cowel/fwd.hpp"
#include "cowel/services.hpp"

namespace cowel {

struct Generation_Options {
    /// @brief To be used for generating error content within the document
    /// when directive processing runs into an error.
    const Directive_Behavior* error_behavior = nullptr;

    /// @brief The highlight theme source.
    std::u8string_view highlight_theme_source;

    const Name_Resolver& builtin_name_resolver;
    File_Loader& file_loader = always_failing_file_loader;
    Logger& logger = ignorant_logger;
    Syntax_Highlighter& highlighter = no_support_syntax_highlighter;

    /// @brief A source of memory to be used throughout generation,
    /// emitting diagnostics, etc.
    std::pmr::memory_resource* memory;
};

/// @brief Constructs a `Context` and invokes `generate` with that context.
/// @returns The result returned by `generate`.
[[nodiscard]]
Processing_Status run_generation(
    Function_Ref<Processing_Status(Context&)> generate,
    const Generation_Options& options
);

/// @brief Resolves references previously written via `reference_section`, recursively.
bool resolve_references(Text_Sink& out, std::u8string_view text, Context& context, File_Id file);

[[nodiscard]]
Processing_Status write_head_body_document(
    Text_Sink& out,
    std::span<const ast::Markup_Element> content,
    Context& context,
    Function_Ref<Processing_Status(Content_Policy&, std::span<const ast::Markup_Element>, Context&)>
        head,
    Function_Ref<Processing_Status(Content_Policy&, std::span<const ast::Markup_Element>, Context&)>
        body
);

[[nodiscard]]
Processing_Status write_wg21_head_contents(
    Content_Policy& out,
    std::span<const ast::Markup_Element>,
    Context& context
);

[[nodiscard]]
Processing_Status write_wg21_body_contents(
    Content_Policy& out,
    std::span<const ast::Markup_Element> content,
    Context& context
);

[[nodiscard]]
inline Processing_Status
write_wg21_document(Text_Sink& out, std::span<const ast::Markup_Element> content, Context& context)
{
    return write_head_body_document(out, content, context, //
        const_v<&write_wg21_head_contents>, //
        const_v<&write_wg21_body_contents>);
}

} // namespace cowel

#endif
