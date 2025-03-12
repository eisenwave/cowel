#include <memory_resource>

#include "mmml/util/assert.hpp"
#include "mmml/util/html_writer.hpp"

#include "mmml/context.hpp"
#include "mmml/directives.hpp"
#include "mmml/document_generation.hpp"

namespace mmml {

void generate_document(const Generation_Options& options)
{
    MMML_ASSERT(options.memory != nullptr);

    std::pmr::unsynchronized_pool_resource transient_memory { options.memory };

    HTML_Writer writer { options.output };

    Context context { options.path,   options.source,      options.error_behavior, //
                      options.logger, options.highlighter, options.document_finder, //
                      options.memory, &transient_memory };
    context.add_resolver(options.builtin_behavior);

    options.root_behavior.generate_html(writer, options.root_content, context);
}

} // namespace mmml
