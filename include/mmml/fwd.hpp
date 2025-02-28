#ifndef MMML_CONFIG_HPP
#define MMML_CONFIG_HPP

#include <cstddef>
#include <cstdint>

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

struct Argument_Matcher;
enum struct Argument_Status : Default_Underlying;
struct Assertion_Error;
enum struct Assertion_Error_Type : Default_Underlying;
struct AST_Formatting_Options;
struct Builtin_Directive_Set;
template <std::size_t>
struct Characters;
enum struct Code_Language : Default_Underlying;
enum struct Code_Span_Type : Default_Underlying;
struct Code_String;
struct Content;
struct Context;
struct Directive_Behavior;
enum struct Directive_Category : Default_Underlying;
enum struct Directive_Display : Default_Underlying;
struct Error_Tag;
template <typename>
struct Function_Ref;
struct HTML_Writer;
enum struct IO_Error_Code : Default_Underlying;
struct Name_Resolver;
struct Parsed_Document;
template <typename, typename>
struct Result;
enum struct Sign_Policy : Default_Underlying;
struct Success_Tag;
struct Transparent_String_View_Equals;
struct Transparent_String_View_Hash;

namespace ast {

struct Argument;
struct Content;
struct Directive;
struct Escaped;
struct Text;

} // namespace ast

} // namespace mmml

#endif
