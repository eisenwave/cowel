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
namespace {

[[nodiscard]]
std::u8string_view file_load_error_explanation(File_Load_Error error)
{
    switch (error) {
    case File_Load_Error::error: return u8"A generic I/O error occurred.";
    case File_Load_Error::not_found: return u8"The file could not be opened using the given path.";
    case File_Load_Error::read_error:
        return u8"A disk error occurred when reading the file contents.";
    case File_Load_Error::corrupted: return u8"The file contains corrupted UTF-8 data.";
    case File_Load_Error::permissions: return u8"You do not have permissions to open this file.";
    }
    return u8"An unidentified I/O error occurred.";
}

} // namespace

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
    const Result<File_Entry, File_Load_Error> entry = context.get_file_loader().load(path_string);
    if (!entry) {
        const std::u8string_view message[] {
            u8"Failed to include text from file \"", path_string, u8"\". ",
            file_load_error_explanation(entry.error()),
            u8" Note that files are loaded relative to the directory of the current document."
        };
        context.try_error(diagnostic::include::io, d.get_source_span(), message);
    }
    append(out, entry->source);
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

    const Result<File_Entry, File_Load_Error> entry
        = context.get_file_loader().load(as_u8string_view(path_data));
    if (!entry) {
        const std::u8string_view message[] {
            u8"Failed to import sub-document from file \"", as_u8string_view(path_data), u8"\". ",
            file_load_error_explanation(entry.error()),
            u8" Note that files are loaded relative to the directory of the current document."
        };
        context.try_error(diagnostic::import::io, d.get_source_span(), message);
        return;
    }

    auto on_error
        = [&](std::u8string_view id, const Source_Span& location, std::u8string_view message) {
              constexpr auto severity = Severity::error;
              if (!context.emits(severity)) {
                  return;
              }
              context.emit(Diagnostic { severity, id, { location, entry->name }, { &message, 1 } });
          };
    parse_and_build(out, entry->source, entry->name, context.get_transient_memory(), on_error);
}

} // namespace cowel
