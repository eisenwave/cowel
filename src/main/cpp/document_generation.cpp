#include "mmml/util/html_writer.hpp"

#include "mmml/context.hpp"
#include "mmml/diagnostic.hpp"
#include "mmml/directives.hpp"
#include "mmml/document_generation.hpp"

namespace mmml {

namespace {

enum struct Pass : Default_Underlying { preprocess, generate };

} // namespace

void generate_document(const Generation_Options& options)
{
    MMML_ASSERT(options.memory != nullptr);
    MMML_ASSERT(options.min_diagnostic_level <= Severity::none);

    const auto diagnostic_level
        = options.emit_diagnostic ? options.min_diagnostic_level : Severity::none;

    std::pmr::unsynchronized_pool_resource transient_memory { options.memory };

    HTML_Writer writer { options.output };

    const auto make_context = [&]() {
        Context result { options.path,     options.source,         options.emit_diagnostic,
                         diagnostic_level, options.error_behavior, options.memory,
                         &transient_memory };
        result.add_resolver(options.builtin_behavior);
        return result;
    };

    for (const auto pass : { Pass::preprocess, Pass::generate }) {
        auto context = make_context();

        if (pass == Pass::preprocess) {
            options.root_behavior.preprocess(options.root_content, context);
        }
        else {
            options.root_behavior.generate_html(writer, options.root_content, context);
        }

        transient_memory.release();
    }
}

} // namespace mmml
