#include <string_view>
#include <vector>

#include "cowel/parse.hpp"
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

void Import_Behavior::instantiate(
    std::pmr::vector<ast::Content>& out,
    const ast::Directive& d,
    Context& context
) const
{
    std::pmr::vector<char8_t> path_data { context.get_transient_memory() };
    to_plaintext(path_data, d.get_content(), context);

    if (path_data.empty()) {
        context.try_error(
            diagnostic::import::path_missing, d.get_source_span(),
            u8"The given path to include text data from cannot empty."
        );
        return;
    }

    // This is a bit of a dirty hack.
    // We need to somehow keep the source code of the loaded file alive
    // because the AST is filled with nodes that have string_views into it.
    //
    // The easiest approach is to just write to a section or variable or something.
    // Anything in "std." is reserved for COWEL anyway, so we may as well do it like this.
    std::u8string_view persistent_path;
    const auto* const source_text = [&] -> std::pmr::vector<char8_t>* {
        std::pmr::u8string section_name { u8"std.import.", context.get_transient_memory() };
        const std::size_t base_length = section_name.length();
        section_name += as_u8string_view(path_data);
        const auto scope = context.get_sections().go_to_scoped(std::move(section_name));
        persistent_path = context.get_sections().current_name().substr(base_length);

        std::pmr::vector<char8_t>& out = context.get_sections().current_text();
        const bool success = context.get_file_loader()(out, persistent_path);
        if (!success) {
            const std::u8string_view message[] {
                u8"Failed to import sub-document from file \"", persistent_path,
                u8"\" because the file could not be opened or because of an I/O error. ",
                u8"Note that files are loaded relative to the directory of the current document."
            };
            context.try_error(diagnostic::import::io, d.get_source_span(), message);
            return nullptr;
        }
        return &out;
    }();
    if (!source_text) {
        return;
    }

    const auto source_string = as_u8string_view(*source_text);

    auto on_error = [&](std::u8string_view id, const Source_Span& location,
                        std::u8string_view message) {
        constexpr auto severity = Severity::error;
        if (!context.emits(severity)) {
            return;
        }
        context.emit(Diagnostic { severity, id, { location, persistent_path }, { &message, 1 } });
    };
    parse_and_build(out, source_string, persistent_path, context.get_transient_memory(), on_error);
}

} // namespace cowel
