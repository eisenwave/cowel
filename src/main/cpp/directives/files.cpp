#include <string_view>
#include <vector>

#include "cowel/util/strings.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/services.hpp"

namespace cowel {

void Include_Behavior::generate_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    Context& context
) const
{
    std::pmr::vector<char8_t> path_data { context.get_transient_memory() };
    to_plaintext(path_data, d.get_content(), context);

    if (path_data.empty()) {
        context.try_error(
            diagnostic::include::path_missing, d.get_source_span(),
            u8"The given path to include text data from cannot empty."
        );
        return;
    }

    const auto path_string = as_u8string_view(path_data);
    const bool success = context.get_file_loader()(out, path_string);
    if (!success) {
        const std::u8string_view message[] {
            u8"Failed to include text from file \"", path_string,
            u8"\" because the file could not be opened or because of an I/O error. ",
            u8"Note that files are loaded relative to the directory of the current document."
        };
        context.try_error(diagnostic::include::io, d.get_source_span(), message);
    }
}

} // namespace cowel
