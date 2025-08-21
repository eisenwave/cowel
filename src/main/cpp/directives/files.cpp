#include <string_view>
#include <vector>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/source_position.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"
#include "cowel/output_language.hpp"
#include "cowel/parse.hpp"
#include "cowel/services.hpp"

using namespace std::string_view_literals;

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

Processing_Status
Include_Text_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context)
    const
{
    // TODO: warn about ignored arguments

    std::pmr::vector<char8_t> path_data { context.get_transient_memory() };
    const auto path_status
        = to_plaintext(path_data, call.get_content_span(), call.content_frame, context);
    switch (path_status) {
    case Processing_Status::ok: break;
    case Processing_Status::brk: return Processing_Status::brk;
    case Processing_Status::error:
    case Processing_Status::error_brk:
    case Processing_Status::fatal: return Processing_Status::fatal;
    }

    if (path_data.empty()) {
        context.try_fatal(
            diagnostic::file_path_missing, call.directive.get_source_span(),
            u8"The given path to include text data from cannot empty."sv
        );
        return Processing_Status::fatal;
    }

    const auto path_string = as_u8string_view(path_data);
    const File_Id relative_to = call.directive.get_source_span().file;
    const Result<File_Entry, File_Load_Error> entry
        = context.get_file_loader().load(path_string, relative_to);
    if (!entry) {
        const std::u8string_view message[] {
            u8"Failed to include text from file \"", path_string, u8"\". ",
            file_load_error_explanation(entry.error()),
            u8" Note that files are loaded relative to the directory of the current document."
        };
        context.try_fatal(
            diagnostic::file_io, call.directive.get_source_span(), joined_char_sequence(message)
        );
        return Processing_Status::fatal;
    }
    out.write(entry->source, Output_Language::text);
    return Processing_Status::ok;
}

Processing_Status
Include_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context) const
{
    // TODO: warn about ignored arguments

    std::pmr::vector<char8_t> path_data { context.get_transient_memory() };
    const auto path_status
        = to_plaintext(path_data, call.get_content_span(), call.content_frame, context);
    switch (path_status) {
    case Processing_Status::ok: break;
    case Processing_Status::brk: return Processing_Status::brk;
    case Processing_Status::error:
    case Processing_Status::error_brk:
    case Processing_Status::fatal: return Processing_Status::fatal;
    }

    if (path_data.empty()) {
        context.try_fatal(
            diagnostic::file_path_missing, call.directive.get_source_span(),
            u8"The given path to include text data from cannot empty."sv
        );
        return Processing_Status::fatal;
    }

    const auto path_string = as_u8string_view(path_data);
    const File_Id relative_to = call.directive.get_source_span().file;
    const Result<File_Entry, File_Load_Error> entry
        = context.get_file_loader().load(path_string, relative_to);
    if (!entry) {
        const std::u8string_view message[] {
            u8"Failed to import sub-document from file \"", path_string, u8"\". ",
            file_load_error_explanation(entry.error()),
            u8" Note that files are loaded relative to the directory of the current document."
        };
        context.try_fatal(
            diagnostic::file_io, call.directive.get_source_span(), joined_char_sequence(message)
        );
        return Processing_Status::fatal;
    }

    auto on_error
        = [&](std::u8string_view id, const Source_Span& location, std::u8string_view message) {
              constexpr auto severity = Severity::error;
              if (!context.emits(severity)) {
                  return;
              }
              const File_Source_Span file_location { location, entry->id };
              context.emit(severity, id, file_location, message);
          };

    const ast::Pmr_Vector<ast::Content> imported_content
        = parse_and_build(entry->source, entry->id, context.get_transient_memory(), on_error);

    try_inherit_paragraph(out);
    return consume_all(out, imported_content, call.call_frame, context);
}

} // namespace cowel
