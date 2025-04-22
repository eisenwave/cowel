#ifndef MMML_CONFIG_HPP
#define MMML_CONFIG_HPP

#include <cstddef>

#ifndef NDEBUG // debug builds
#define MMML_DEBUG 1
#define MMML_IF_DEBUG(...) __VA_ARGS__
#define MMML_IF_NOT_DEBUG(...)
#else // release builds
#define MMML_IF_DEBUG(...)
#define MMML_IF_NOT_DEBUG(...) __VA_ARGS__
#endif

#ifdef __EMSCRIPTEN__
#define MMML_EMSCRIPTEN 1
#define MMML_IF_EMSCRIPTEN(...) __VA_ARGS__
#else
#define MMML_IF_EMSCRIPTEN(...)
#endif

#ifdef __clang__
#define MMML_CLANG 1
#endif

#define MMML_UNREACHABLE() __builtin_unreachable()

namespace mmml {

/// @brief The default underlying type for scoped enumerations.
using Default_Underlying = unsigned char;

#define MMML_ENUM_STRING_CASE(...)                                                                 \
    case __VA_ARGS__: return #__VA_ARGS__

#define MMML_ENUM_STRING_CASE8(...)                                                                \
    case __VA_ARGS__: return u8## #__VA_ARGS__

struct Annotated_String_Length;
template <typename>
struct Annotation_Span;
enum struct Diagnostic_Highlight : Default_Underlying;
struct Argument_Matcher;
enum struct Argument_Status : Default_Underlying;
struct AST_Formatting_Options;
struct AST_Instruction;
enum struct AST_Instruction_Type : Default_Underlying;
enum struct Attribute_Style : Default_Underlying;
struct Attribute_Writer;
struct Author_Info;
struct Builtin_Directive_Set;
template <typename, typename>
struct Basic_Annotated_String;
template <typename Char, std::size_t>
struct Basic_Characters;
template <typename>
struct Basic_Transparent_String_View_Equals;
template <typename>
struct Basic_Transparent_String_View_Greater;
template <typename>
struct Basic_Transparent_String_View_Hash;
template <typename>
struct Basic_Transparent_String_View_Less;
enum struct Diagnostic_Highlight : Default_Underlying;
struct Content_Behavior;
struct Context;
struct Diagnostic;
struct Document_Finder;
struct Document_Info;
struct Document_Sections;
struct Directive_Behavior;
struct Directive_Content_Behavior;
enum struct Directive_Category : Default_Underlying;
enum struct Directive_Display : Default_Underlying;
struct Error_Tag;
struct File_Source_Position;
struct File_Source_Span;
struct Generation_Options;
enum struct HLJS_Scope : Default_Underlying;
struct HTML_Writer;
struct Ignorant_Logger;
enum struct IO_Error_Code : Default_Underlying;
struct Logger;
struct Name_Resolver;
struct No_Support_Document_Finder;
struct No_Support_Syntax_Highlighter;
template <typename, typename>
struct Result;
enum struct Severity : Default_Underlying;
enum struct Sign_Policy : Default_Underlying;
struct Source_Position;
struct Source_Span;
struct Success_Tag;
struct Syntax_Highlighter;
enum struct Syntax_Highlight_Error : Default_Underlying;
enum struct To_HTML_Mode : Default_Underlying;

namespace ast {

struct Argument;
struct Content;
struct Directive;
struct Escaped;
struct Generated;
enum struct Generated_Type : bool;
struct Text;

} // namespace ast

template <typename T>
using Annotated_String = Basic_Annotated_String<char, T>;
template <typename T>
using Annotated_String8 = Basic_Annotated_String<char8_t, T>;

template <std::size_t capacity>
using Characters = Basic_Characters<char, capacity>;
template <std::size_t capacity>
using Characters8 = Basic_Characters<char8_t, capacity>;

using Diagnostic_String = Annotated_String8<Diagnostic_Highlight>;

using HLJS_Annotation_Span = Annotation_Span<HLJS_Scope>;

using Transparent_String_View_Equals = Basic_Transparent_String_View_Equals<char>;
using Transparent_String_View_Equals8 = Basic_Transparent_String_View_Equals<char8_t>;
using Transparent_String_View_Greater = Basic_Transparent_String_View_Greater<char>;
using Transparent_String_View_Greater8 = Basic_Transparent_String_View_Greater<char8_t>;
using Transparent_String_View_Hash = Basic_Transparent_String_View_Hash<char>;
using Transparent_String_View_Hash8 = Basic_Transparent_String_View_Hash<char8_t>;
using Transparent_String_View_Less = Basic_Transparent_String_View_Less<char>;
using Transparent_String_View_Less8 = Basic_Transparent_String_View_Less<char8_t>;

} // namespace mmml

#endif
