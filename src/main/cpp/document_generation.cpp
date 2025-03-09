#include "mmml/util/html_writer.hpp"

#include "mmml/context.hpp"
#include "mmml/diagnostic.hpp"
#include "mmml/directives.hpp"
#include "mmml/document_generation.hpp"

namespace mmml {

void generate_document(const Generation_Options& options)
{
    MMML_ASSERT(options.memory != nullptr);
    MMML_ASSERT(options.min_diagnostic_level <= Severity::none);

    const auto diagnostic_level
        = options.emit_diagnostic ? options.min_diagnostic_level : Severity::none;

    std::pmr::unsynchronized_pool_resource transient_memory { options.memory };

    HTML_Writer writer { options.output };

    Context context { options.path,     options.source,         options.emit_diagnostic,
                      diagnostic_level, options.error_behavior, options.memory,
                      &transient_memory };
    context.add_resolver(options.builtin_behavior);

    options.root_behavior.generate_html(writer, options.root_content, context);
}

} // namespace mmml
