#ifndef COWEL_DOCUMENT_GENERATION_HPP
#define COWEL_DOCUMENT_GENERATION_HPP

#include <filesystem>
#include <memory_resource>
#include <span>
#include <string_view>
#include <vector>

#include "cowel/ast.hpp"
#include "cowel/fwd.hpp"
#include "cowel/services.hpp"
#include "cowel/simple_bibliography.hpp"

namespace cowel {

struct Generation_Options {
    std::pmr::vector<char8_t>& output;

    Content_Behavior& root_behavior;
    std::span<const ast::Content> root_content;

    /// @brief Name resolver for builtin behavior (without macro definitions, etc.).
    const Name_Resolver& builtin_behavior;
    /// @brief To be used for generating error content within the document
    /// when directive processing runs into an error.
    Directive_Behavior* error_behavior = nullptr;

    /// @brief The path of the source file.
    const std::filesystem::path& path;
    /// @brief The document source code.
    std::u8string_view source;
    /// @brief The highlight theme source.
    std::u8string_view highlight_theme_source;

    Logger& logger = ignorant_logger;
    Syntax_Highlighter& highlighter = no_support_syntax_highlighter;
    Bibliography& bibliography = simple_bibliography;

    /// @brief A source of memory to be used throughout generation,
    /// emitting diagnostics, etc.
    std::pmr::memory_resource* memory;
};

void generate_document(const Generation_Options& options);

} // namespace cowel

#endif
