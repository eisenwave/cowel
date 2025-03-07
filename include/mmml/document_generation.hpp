#ifndef MMML_DOCUMENT_GENERATION_HPP
#define MMML_DOCUMENT_GENERATION_HPP

#include <filesystem>
#include <span>
#include <vector>

#include "mmml/util/function_ref.hpp"

#include "mmml/ast.hpp"
#include "mmml/fwd.hpp"

namespace mmml {

struct Generation_Options {
    std::pmr::vector<char8_t>& output;

    Content_Behavior& root_behavior;
    std::pmr::vector<ast::Content>& root_content;

    /// @brief Name resolver for builtin behavior (without macro definitions, etc.).
    const Name_Resolver& builtin_behavior;
    /// @brief To be used for generating error content within the document
    /// when directive processing runs into an error.
    Directive_Behavior* error_behavior = nullptr;

    /// @brief The path of the source file.
    std::filesystem::path path;
    /// @brief The document source code.
    std::u8string_view source;
    /// @brief Invoked when diagnostics are emitted.
    Function_Ref<void(Diagnostic&&)> emit_diagnostic = {};
    /// @brief The minimum level of diagnostics that are emitted.
    /// This option is ignored if `emit_diagnostic` is empty;
    /// in that case, the level is implicitly `none`.
    Severity min_diagnostic_level = Severity::none;
    /// @brief A source of memory to be used throughout generation,
    /// emitting diagnostics, etc.
    std::pmr::memory_resource* memory;
};

void generate_document(const Generation_Options& options);

} // namespace mmml

#endif
