#include <optional>
#include <string_view>

#include "mmml/util/annotated_string.hpp"
#include "mmml/util/annotation_type.hpp"
#include "mmml/util/ansi.hpp"
#include "mmml/util/assert.hpp"
#include "mmml/util/source_position.hpp"
#include "mmml/util/strings.hpp"
#include "mmml/util/to_chars.hpp"

#include "mmml/ast.hpp"
#include "mmml/parse.hpp"
#include "mmml/print.hpp"

namespace mmml {

namespace {

[[nodiscard]]
std::u8string_view highlight_color_of(Annotation_Type type)
{
    using enum Annotation_Type;
    switch (type) {
    case text: return ansi::reset;

    case identifier:
    case variable_name:
    case function_name: return ansi::h_white;

    case annotation_name:
    case type_name: return ansi::h_blue;

    case number: return ansi::h_cyan;

    case string: return ansi::h_green;

    case comment:
    case operation: return ansi::h_black;

    case bracket:
    case punctuation: return ansi::black;

    case keyword:
    case boolean_literal: return ansi::h_magenta;

    case error: return ansi::h_red;

    case diagnostic_text:
    case diagnostic_code_citation:
    case diagnostic_punctuation:
    case diagnostic_operator: return ansi::reset;

    case diagnostic_code_position:
    case diagnostic_internal: return ansi::h_black;

    case diagnostic_error_text:
    case diagnostic_error: return ansi::h_red;

    case diagnostic_warning:
    case diagnostic_line_number: return ansi::h_yellow;

    case diagnostic_note: return ansi::h_white;

    case diagnostic_position_indicator: return ansi::h_green;

    case diagnostic_internal_error_notice: return ansi::h_yellow;

    case diagnostic_operand: return ansi::h_magenta;

    case diagnostic_tag: return ansi::h_blue;

    case diagnostic_attribute: return ansi::h_magenta;

    case diagnostic_escape: return ansi::h_yellow;

    case html_preamble:
    case html_comment:
    case html_tag_bracket:
    case html_attribute_equal: return ansi::h_black;

    case html_tag_identifier: return ansi::h_blue;
    case html_attribute_key: return ansi::h_cyan;
    case html_attribute_value: return ansi::h_green;
    case html_inner_text: return ansi::reset;
    }
    MMML_ASSERT_UNREACHABLE(u8"Unknown code span type.");
}

enum struct Error_Line_Type : Default_Underlying { note, error };

struct Error_Line {
    std::optional<File_Source_Position> pos {};
    std::u8string_view message;
    bool omit_affected_line = false;
};

constexpr std::u8string_view error_prefix = u8"error:";
constexpr std::u8string_view note_prefix = u8"note:";

[[nodiscard]]
std::u8string_view to_prose(IO_Error_Code e)
{
    using enum IO_Error_Code;
    switch (e) {
    case cannot_open: //
        return u8"Failed to open file.";
    case read_error: //
        return u8"I/O error occurred when reading from file.";
    case write_error: //
        return u8"I/O error occurred when writing to file.";
    case corrupted: //
        return u8"Data in the file is corrupted (not properly encoded).";
    }
    MMML_ASSERT_UNREACHABLE(u8"invalid error code");
}

void file_name_to_utf8(std::pmr::u8string& out, std::string_view name)
{
    // Technically, we're making the (not always correct) assumption that file name encoding
    // is UTF-8, which is not guaranteed.
    // However, the process of converting this properly in a portable way would be somewhat
    // complex, and I don't care about correct non-ASCII file names for now.
    out.resize(name.size());
    std::memcpy(out.data(), name.data(), name.size());
}

std::pmr::u8string file_name_to_utf8(std::string_view name, std::pmr::memory_resource* memory)
{
    std::pmr::u8string result { memory };
    file_name_to_utf8(result, name);
    return result;
}

void print_source_position(Annotated_String8& out, const std::optional<File_Source_Position>& pos)
{
    if (!pos) {
        out.append(u8"(internal):", Annotation_Type::diagnostic_code_position);
    }
    else {
        print_file_position(out, pos->file_name, Source_Position { *pos });
    }
}

void print_diagnostic_prefix(
    Annotated_String8& out,
    Error_Line_Type type,
    std::optional<File_Source_Position> pos
)
{
    print_source_position(out, pos);
    out.append(' ');
    switch (type) {
    case Error_Line_Type::error: //
        out.append(error_prefix, Annotation_Type::diagnostic_error);
        break;
    case Error_Line_Type::note: //
        out.append(note_prefix, Annotation_Type::diagnostic_note);
        break;
    }
}

void print_diagnostic_line(
    Annotated_String8& out,
    Error_Line_Type type,
    const Error_Line& line,
    std::u8string_view source
)
{
    print_diagnostic_prefix(out, type, line.pos);
    out.append(u8' ');
    out.append(line.message, Annotation_Type::diagnostic_text);

    out.append(u8'\n');
    if (line.pos && !line.omit_affected_line) {
        print_affected_line(out, source, *line.pos);
    }
}

[[maybe_unused]]
void print_error_line(Annotated_String8& out, const Error_Line& line, std::u8string_view source)
{
    return print_diagnostic_line(out, Error_Line_Type::error, line, source);
}

[[maybe_unused]]
void print_note_line(Annotated_String8& out, const Error_Line& line, std::u8string_view source)
{
    return print_diagnostic_line(out, Error_Line_Type::note, line, source);
}

void do_print_affected_line(
    Annotated_String8& out,
    std::u8string_view source,
    std::size_t begin,
    std::size_t length,
    std::size_t line,
    std::size_t column
)
{
    MMML_ASSERT(length > 0);

    {
        // Sorry, multi-line printing is not supported yet.
        const std::u8string_view snippet = source.substr(begin, length);
        MMML_ASSERT(length <= 1 || snippet.find('\n') == std::string_view::npos);
    }

    const std::u8string_view cited_code = find_line(source, begin);

    const auto line_chars = to_characters<char8_t>(line + 1);
    constexpr std::size_t pad_max = 6;
    const std::size_t pad_length
        = pad_max - std::min(line_chars.length, std::size_t { pad_max - 1 });
    out.append(pad_length, u8' ');
    out.append_integer(line + 1, Annotation_Type::diagnostic_line_number);
    out.append(u8' ');
    out.append(u8'|', Annotation_Type::diagnostic_punctuation);
    out.append(u8' ');
    out.append(cited_code, Annotation_Type::diagnostic_code_citation);
    out.append(u8'\n');

    const std::size_t align_length = std::max(pad_max, line_chars.length + 1);
    out.append(align_length, u8' ');
    out.append(u8' ');
    out.append(u8'|', Annotation_Type::diagnostic_punctuation);
    out.append(u8' ');
    out.append(column, u8' ');
    {
        auto position = out.build(Annotation_Type::diagnostic_position_indicator);
        position.append(u8'^');
        if (length > 1) {
            position.append(length - 1, u8'~');
        }
    }
    out.append(u8'\n');
}

} // namespace

void print_file_position(
    Annotated_String8& out,
    std::string_view file,
    const Source_Position& pos,
    bool suffix_colon
)
{
    std::pmr::u8string file8 = file_name_to_utf8(file, out.get_memory());

    auto builder = out.build(Annotation_Type::diagnostic_code_position);
    builder.append(file8)
        .append(':')
        .append_integer(pos.line + 1)
        .append(':')
        .append_integer(pos.column + 1);
    if (suffix_colon) {
        builder.append(':');
    }
}

void print_affected_line(
    Annotated_String8& out,
    std::u8string_view source,
    const Source_Position& pos
)
{
    do_print_affected_line(out, source, pos.begin, 1, pos.line, pos.column);
}

void print_affected_line(Annotated_String8& out, std::u8string_view source, const Source_Span& pos)
{
    MMML_ASSERT(!pos.empty());
    do_print_affected_line(out, source, pos.begin, pos.length, pos.line, pos.column);
}

std::u8string_view find_line(std::u8string_view source, std::size_t index)
{
    MMML_ASSERT(index <= source.size());

    if (index == source.size() || source[index] == '\n') {
        // Special case for EOF positions, which may be past the end of a line,
        // and even past the end of the whole source, but only by a single character.
        // For such positions, we yield the currently ended line.
        --index;
    }

    std::size_t begin = source.rfind('\n', index);
    begin = begin != std::u8string_view::npos ? begin + 1 : 0;

    std::size_t end = std::min(source.find('\n', index + 1), source.size());

    return source.substr(begin, end - begin);
}

void print_location_of_file(Annotated_String8& out, std::string_view file)
{
    std::pmr::u8string file8 = file_name_to_utf8(file, out.get_memory());
    out.build(Annotation_Type::diagnostic_code_position).append(file8).append(u8':');
}

void print_assertion_error(Annotated_String8& out, const Assertion_Error& error)
{
    out.append(u8"Assertion failed! ", Annotation_Type::diagnostic_error);

    const std::u8string_view message = error.type == Assertion_Error_Type::expression
        ? u8"The following expression evaluated to 'false', but was expected to be 'true':"
        : u8"Code which must be unreachable has been reached.";
    out.append(message, Annotation_Type::diagnostic_text);
    out.append(u8"\n\n");

    Source_Position pos { .line = error.location.line(),
                          .column = error.location.column(),
                          .begin = {} };
    print_file_position(out, error.location.file_name(), pos);
    out.append(u8' ');
    out.append(error.message, Annotation_Type::diagnostic_error_text);
    out.append(u8"\n\n");
    print_internal_error_notice(out);
}

void print_io_error(Annotated_String8& out, std::string_view file, IO_Error_Code error)
{
    print_location_of_file(out, file);
    out.append(u8' ');
    out.append(to_prose(error), Annotation_Type::diagnostic_text);
    out.append(u8'\n');
}

namespace {

void print_cut_off(Annotated_String8& out, std::u8string_view v, std::size_t limit)
{
    std::size_t visual_length = 0;

    for (std::size_t i = 0; i < v.length();) {
        if (visual_length >= limit) {
            out.append(u8"...", Annotation_Type::diagnostic_punctuation);
            break;
        }

        if (v[i] == '\r') {
            out.append(u8"\\r", Annotation_Type::diagnostic_escape);
            visual_length += 2;
            ++i;
        }
        else if (v[i] == '\t') {
            out.append(u8"\\t", Annotation_Type::diagnostic_escape);
            visual_length += 2;
            ++i;
        }
        else if (v[i] == '\n') {
            out.append(u8"\\n", Annotation_Type::diagnostic_escape);
            visual_length += 2;
            ++i;
        }
        else {
            const auto remainder = v.substr(i, limit - visual_length);
            const auto part = remainder.substr(0, remainder.find_first_of(u8"\r\t\n"));
            out.append(part, Annotation_Type::diagnostic_code_citation);
            visual_length += part.size();
            i += part.size();
        }
    }
}

} // namespace

struct [[nodiscard]] AST_Printer : ast::Const_Visitor {
private:
    using char_type = char8_t;
    using string_view_type = std::u8string_view;
    using annotated_string_type = Annotated_String8;

    annotated_string_type& out;
    string_view_type source;
    const AST_Formatting_Options options;
    int indent_level = 0;

    struct [[nodiscard]] Scoped_Indent {
        int& level;

        explicit Scoped_Indent(int& level)
            : level { ++level }
        {
        }

        Scoped_Indent(const Scoped_Indent&) = delete;
        Scoped_Indent& operator=(const Scoped_Indent&) = delete;

        ~Scoped_Indent()
        {
            --level;
        }
    };

public:
    AST_Printer(
        annotated_string_type& out,
        string_view_type source,
        const AST_Formatting_Options& options
    )
        : out { out }
        , source { source }
        , options { options }
    {
    }

    void visit(const ast::Text& node) final
    {
        print_indent();
        const string_view_type extracted = node.get_text(source);

        out.append(u8"Text", Annotation_Type::diagnostic_tag);
        out.append(u8'(', Annotation_Type::diagnostic_punctuation);
        print_cut_off(out, extracted, std::size_t(options.max_node_text_length));
        out.append(u8')', Annotation_Type::diagnostic_punctuation);
        out.append(u8'\n');
    }

    void visit(const ast::Escaped& node) final
    {
        print_indent();
        const string_view_type extracted = node.get_text(source);

        out.append(u8"Escaped", Annotation_Type::diagnostic_tag);
        out.append(u8'(', Annotation_Type::diagnostic_punctuation);
        print_cut_off(out, extracted, std::size_t(options.max_node_text_length));
        out.append(u8')', Annotation_Type::diagnostic_punctuation);
        out.append(u8'\n');
    }

    void visit(const ast::Directive& directive) final
    {
        print_indent();

        out.build(Annotation_Type::diagnostic_tag)
            .append(u8'\\')
            .append(directive.get_name(source));

        if (!directive.get_arguments().empty()) {
            out.append(u8'[', Annotation_Type::diagnostic_punctuation);
            out.append(u8'\n');
            {
                Scoped_Indent i = indented();
                visit_arguments(directive);
            }
            print_indent();
            out.append(u8']', Annotation_Type::diagnostic_punctuation);
        }
        else {
            out.append(u8"[]", Annotation_Type::diagnostic_punctuation);
        }

        if (!directive.get_content().empty()) {
            out.append(u8'{', Annotation_Type::diagnostic_punctuation);
            out.append(u8'\n');
            {
                Scoped_Indent i = indented();
                visit_content_sequence(directive.get_content());
            }
            print_indent();
            out.append(u8'}', Annotation_Type::diagnostic_punctuation);
        }
        else {
            out.append(u8"{}", Annotation_Type::diagnostic_punctuation);
        }

        out.append(u8'\n');
    }

    void visit(const ast::Argument& arg) final
    {
        print_indent();

        if (arg.has_name()) {
            out.append(u8"Named_Argument", Annotation_Type::diagnostic_tag);
            out.append(u8'(', Annotation_Type::diagnostic_punctuation);
            out.append(arg.get_name(source), Annotation_Type::diagnostic_attribute);
            out.append(u8')', Annotation_Type::diagnostic_punctuation);
        }
        else {
            out.append(u8"Positional_Argument", Annotation_Type::diagnostic_tag);
        }

        if (!arg.get_content().empty()) {
            out.append(u8'\n');
            Scoped_Indent i = indented();
            visit_content_sequence(arg.get_content());
        }
        else {
            out.append(u8" (empty value)", Annotation_Type::diagnostic_internal);
            out.append(u8'\n');
        }
    }

private:
    Scoped_Indent indented()
    {
        return Scoped_Indent { indent_level };
    }

    void print_indent()
    {
        MMML_ASSERT(indent_level >= 0);
        MMML_ASSERT(options.indent_width >= 0);

        const auto indent = std::size_t(options.indent_width * indent_level);
        out.append(indent, u8' ');
    }
};

void print_ast(
    Annotated_String8& out,
    std::u8string_view source,
    std::span<const ast::Content> root_content,
    AST_Formatting_Options options
)
{
    AST_Printer { out, source, options }.visit_content_sequence(root_content);
}

void print_internal_error_notice(Annotated_String8& out)
{
    constexpr std::u8string_view notice
        = u8"This is an internal error. Please report this bug at:\n"
          u8"https://github.com/Eisenwave/bit-manipulation/issues\n";
    out.append(notice, Annotation_Type::diagnostic_internal_error_notice);
}

#ifndef MMML_EMSCRIPTEN

std::ostream& operator<<(std::ostream& out, std::u8string_view str)
{
    return out << as_string_view(str);
}

std::ostream& print_code_string(std::ostream& out, const Annotated_String8& string, bool colors)
{
    const std::u8string_view text = string.get_text();
    if (!colors) {
        return out << text;
    }

    Annotation_Span previous {};
    for (Annotation_Span span : string) {
        const std::size_t previous_end = previous.begin + previous.length;
        MMML_ASSERT(span.begin >= previous_end);
        if (previous_end != span.begin) {
            out << text.substr(previous_end, span.begin - previous_end);
        }
        out << highlight_color_of(span.type) << text.substr(span.begin, span.length) << ansi::reset;
        previous = span;
    }
    const std::size_t last_span_end = previous.begin + previous.length;
    if (last_span_end != text.size()) {
        out << text.substr(last_span_end);
    }

    return out;
}
#endif

} // namespace mmml
