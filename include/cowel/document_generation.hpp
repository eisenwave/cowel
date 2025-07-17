#ifndef COWEL_DOCUMENT_GENERATION_HPP
#define COWEL_DOCUMENT_GENERATION_HPP

#include <memory_resource>
#include <span>
#include <string_view>

#include "cowel/util/function_ref.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/content_status.hpp"
#include "cowel/fwd.hpp"
#include "cowel/services.hpp"
#include "cowel/simple_bibliography.hpp"

namespace cowel {

struct Generation_Options {
    /// @brief Name resolver for builtin behavior (without macro definitions, etc.).
    const Name_Resolver& builtin_behavior;
    /// @brief To be used for generating error content within the document
    /// when directive processing runs into an error.
    Directive_Behavior* error_behavior = nullptr;

    /// @brief The highlight theme source.
    std::u8string_view highlight_theme_source;

    File_Loader& file_loader = always_failing_file_loader;
    Logger& logger = ignorant_logger;
    Syntax_Highlighter& highlighter = no_support_syntax_highlighter;
    Bibliography& bibliography = simple_bibliography;

    /// @brief A source of memory to be used throughout generation,
    /// emitting diagnostics, etc.
    std::pmr::memory_resource* memory;
};

/// @brief Constructs a `Context` and invokes `generate` with that context.
/// @returns The result returned by `generate`.
Content_Status
run_generation(Function_Ref<Content_Status(Context&)> generate, const Generation_Options& options);

[[nodiscard]]
Content_Status
write_wg21_document(Text_Sink& out, std::span<const ast::Content> content, Context& context);

[[nodiscard]]
Content_Status
write_wg21_head_contents(Text_Sink& out, std::span<const ast::Content>, Context& context);

[[nodiscard]]
Content_Status write_wg21_body_contents(
    Content_Policy& out,
    std::span<const ast::Content> content,
    Context& context
);

} // namespace cowel

#endif
