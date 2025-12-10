#ifndef COWEL_CONFIG_HPP
#define COWEL_CONFIG_HPP

#include "cowel/settings.hpp"

COWEL_IF_DEBUG() // silence unused warning for settings.hpp

namespace cowel {

/// @brief The default underlying type for scoped enumerations.
using Default_Underlying = unsigned char;

#define COWEL_ENUM_STRING_CASE(...)                                                                \
    case __VA_ARGS__: return #__VA_ARGS__

#define COWEL_ENUM_STRING_CASE8(...)                                                               \
    case __VA_ARGS__: return u8## #__VA_ARGS__

struct Annotated_String_Length;
template <typename>
struct Annotation_Span;
enum struct Diagnostic_Highlight : Default_Underlying;
enum struct Argument_Status : Default_Underlying;
struct AST_Formatting_Options;
struct AST_Instruction;
enum struct AST_Instruction_Type : Default_Underlying;
enum struct Attribute_Style : Default_Underlying;
struct Author_Info;
struct Builtin_Directive_Set;
template <typename, typename>
struct Basic_Annotated_String;
template <typename File>
struct Basic_File_Source_Position;
template <typename File>
struct Basic_File_Source_Span;
template <typename>
struct Basic_Transparent_String_View_Equals;
template <typename>
struct Basic_Transparent_String_View_Greater;
template <typename>
struct Basic_Transparent_String_View_Hash;
template <typename>
struct Basic_Transparent_String_View_Less;
enum struct Blank_Line_Initial_State : bool;
enum struct Diagnostic_Highlight : Default_Underlying;
struct Content_Policy;
struct Context;
struct Diagnostic;
struct Bibliography;
struct Document_Info;
struct Document_Sections;
struct Directive_Behavior;
struct Error_Tag;
using Float = double;
struct Generation_Options;
enum struct HLJS_Scope : Default_Underlying;
struct Ignorant_Logger;
enum struct IO_Error_Code : Default_Underlying;
using Integer = long long;
struct Invocation;
struct Logger;
struct Name_Resolver;
struct Null;
struct Simple_Bibliography;
struct No_Support_Syntax_Highlighter;
template <typename, typename>
struct Result;
struct Scoped_Frame;
enum struct Severity : Default_Underlying;
enum struct Sign_Policy : Default_Underlying;
struct Source_Position;
struct Source_Span;
struct Stack_Frame;
struct Success_Tag;
struct Syntax_Highlighter;
enum struct Syntax_Highlight_Error : Default_Underlying;
enum struct To_HTML_Mode : Default_Underlying;
struct Type;
enum struct Type_Kind : Default_Underlying;
struct Unit;
struct Value;

namespace ast {

struct Markup_Element;
struct Member_Value;
struct Directive;
struct Group_Member;
struct Primary;

} // namespace ast

template <typename T>
using Annotated_String = Basic_Annotated_String<char, T>;
template <typename T>
using Annotated_String8 = Basic_Annotated_String<char8_t, T>;

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

/// @brief A numeric file identifier.
enum struct File_Id : int { main = -1 }; // NOLINT(performance-enum-size)

/// @brief A stack frame index.
/// The special value `root = -1` expresses top-level content,
/// i.e. content which is not expanded from any macro.
enum struct Frame_Index : int { root = -1 }; // NOLINT(performance-enum-size)

using File_Source_Position = Basic_File_Source_Position<File_Id>;
using File_Source_Span = Basic_File_Source_Span<File_Id>;

} // namespace cowel

#endif
