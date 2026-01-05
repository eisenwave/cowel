#include <string_view>
#include <vector>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/source_position.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"
#include "cowel/parameters.hpp"
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
Include_Text_Behavior::do_evaluate(String_Sink& out, const Invocation& call, Context& context) const
{
    String_Matcher path_matcher { context.get_transient_memory() };
    Group_Member_Matcher path_member { u8"path", Optionality::mandatory, path_matcher };
    Group_Member_Matcher* matchers[] { &path_member };
    Pack_Usual_Matcher args_matcher { matchers };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(
        call, context, make_fail_callback<Severity::fatal>(), Processing_Status::fatal
    );
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    const auto path_string = path_matcher.get();
    if (path_string.empty()) {
        context.try_fatal(
            diagnostic::file_path_missing, call.directive.get_source_span(),
            u8"The given path to include text data from cannot empty."sv
        );
        return Processing_Status::fatal;
    }

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
    out.consume(entry->source);
    return Processing_Status::ok;
}

Processing_Status
Include_Behavior::splice(Content_Policy& out, const Invocation& call, Context& context) const
{
    String_Matcher path_matcher { context.get_transient_memory() };
    Group_Member_Matcher path_member { u8"path", Optionality::mandatory, path_matcher };
    Group_Member_Matcher* matchers[] { &path_member };
    Pack_Usual_Matcher args_matcher { matchers };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(
        call, context, make_fail_callback<Severity::fatal>(), Processing_Status::fatal
    );
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    const auto path_string = path_matcher.get();
    if (path_string.empty()) {
        context.try_fatal(
            diagnostic::file_path_missing, call.directive.get_source_span(),
            u8"The given path to include text data from cannot empty."sv
        );
        return Processing_Status::fatal;
    }

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
    COWEL_ASSERT(context.get_file_loader().is_valid(entry->id));

    const std::convertible_to<Parse_Error_Consumer> auto on_parse_error
        = [&](std::u8string_view id, const Source_Span& location, const Char_Sequence8& message) {
              constexpr auto severity = Severity::error;
              if (!context.emits(severity)) {
                  return;
              }
              const File_Source_Span file_location { location, entry->id };
              context.emit(severity, id, file_location, message);
          };

    ast::Pmr_Vector<ast::Markup_Element> imported_content { context.get_transient_memory() };
    const bool parse_success = lex_and_parse_and_build(
        imported_content, entry->source, entry->id, context.get_transient_memory(), on_parse_error
    );
    if (!parse_success) {
        context.try_fatal(
            diagnostic::parse, call.directive.get_source_span(),
            joined_char_sequence(
                {
                    u8"Abandoning processing because the included file \"",
                    path_string,
                    u8"\" could not be parsed, i.e. raised syntax errors.",
                }
            )
        );
        return Processing_Status::fatal;
    }

    try_inherit_paragraph(out);
    return splice_all(out, imported_content, call.call_frame, context);
}

} // namespace cowel
