#ifndef COWEL_LSP_HPP
#define COWEL_LSP_HPP

#include <any>
#include <cmath>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <variant>

#include "cowel/util/chars.hpp"

#include "cowel/fwd.hpp"
#include "cowel/json.hpp"

/// This header defines reusable JSON-RPC/LSP data-model abstractions.
/// Keep declarations protocol-generic and free of server-specific defaults.
/// Concrete server behavior and concrete protocol values belong in `lsp.cpp`.

namespace cowel::lsp {

using namespace std::literals;

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.18/specification/#integer
using Integer = Int32;
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.18/specification/#uinteger
using UInteger = Uint32;
using json::Null;
using json::null;

/// @brief A JSON-RPC error code.
/// @see https://www.jsonrpc.org/specification §5.1
/// @see
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#errorCodes
enum struct Error_Code { // NOLINT(performance-enum-size)
    /// Invalid JSON was received by the server.
    /// An error occurred on the server while parsing the JSON text.
    parse_error = -32'700,
    /// The JSON sent is not a valid Request object.
    invalid_request = -32'600,
    /// The method does not exist / is not available.
    method_not_found = -32601,
    /// Invalid method parameter(s).
    invalid_params = -32602,
    /// Internal JSON-RPC error.
    internal_error = -32603,
    /// Server received a notification or  request
    /// before the server received the `initialize` request.
    server_not_initialized = -32'002,
    unknown_error_code = -32'001,
    /// A request failed but it was syntactically correct,
    /// e.g the method name was known and the parameters were valid.
    /// The error message should contain human readable information
    /// about why the request failed.
    /// @since 3.17.0
    request_failed = -32803,
    /// The server cancelled the request.
    /// This error code should only be used for requests that explicitly support being
    /// server cancellable.
    /// @since 3.17.0
    server_cancelled = -32802,
    /// The server detected that the content of a document got
    /// modified outside normal conditions.
    /// A server should NOT send this error code if it detects a content change
    /// in its unprocessed messages.
    /// The result even computed on an older state might still be useful for the client.
    ///
    /// If a client decides that a result is not of any use anymore,
    /// the client should cancel the request.
    content_modified = -32801,
    /// The client has canceled a request
    /// and a server has detected the cancel.
    request_cancelled = -32800,
};

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#headerPart
namespace header {

inline constexpr auto content_length = u8"Content-Length: "sv;
inline constexpr auto content_type = u8"Content-Type: "sv;

} // namespace header

struct Deserialize_Storage {
    std::vector<std::any> storage;
    template <typename T>
    std::remove_const_t<T>& emplace(T&& val)
    {
        std::any& result = storage.emplace_back(std::forward<T>(val));
        return std::any_cast<std::remove_cvref_t<T>&>(result);
    }
};

namespace detail {

/// A JSON serializer (and deserializer),
/// where each specialization is expected to provide two static member functions:
/// - `std::optional<T> from_json(const json_type_t<T>&)`
/// - `json_type_t<T> to_json(const T& val, std::pmr::memory_resource*)`
template <typename T>
struct Serializer;

/// The corresponding JSON type of `T` when serialized.
template <typename T>
using json_type_t = decltype(Serializer<T>::to_json(
    std::declval<const T&>(),
    static_cast<std::pmr::memory_resource*>(nullptr)
));

} // namespace detail

template <typename T>
concept json_serializable = requires { detail::Serializer<T> {}; };

/// Deserializes from one of `json::Value`'s alternatives to `T`
/// (specifically from the `json_type_t<T>`).
template <json_serializable T>
    requires(!std::is_same_v<detail::json_type_t<T>, json::Value>)
std::optional<T> from_json(const detail::json_type_t<T>& val, Deserialize_Storage& storage);
/// Deserializes from `json::Value`,
/// which first requires checking whether the active alternative is `json_type_t<T>`,
/// and if so, deserializing that alternative.
template <json_serializable T>
std::optional<T> from_json(const json::Value& val, Deserialize_Storage& storage);

template <json_serializable T>
detail::json_type_t<T> to_json(const T& val, std::pmr::memory_resource* memory);

namespace detail {

template <typename T>
struct Identity_Serializer {
    [[nodiscard]]
    static std::optional<T> from_json(const T& val, Deserialize_Storage&)
    {
        return val;
    }
    [[nodiscard]]
    static T to_json(const T& val, std::pmr::memory_resource* const)
    {
        return val;
    }
};

template <>
struct Serializer<json::Null> : Identity_Serializer<json::Null> { };
template <>
struct Serializer<bool> : Identity_Serializer<bool> { };
template <>
struct Serializer<json::Number> : Identity_Serializer<json::Number> { };
template <>
struct Serializer<json::Array> : Identity_Serializer<json::Array> { };
template <>
struct Serializer<json::Object> : Identity_Serializer<json::Object> { };
template <>
struct Serializer<json::Value> : Identity_Serializer<json::Value> { };

template <typename T>
    requires std::is_integral_v<T>
struct Serializer<T> {
    [[nodiscard]]
    static std::optional<T> from_json(const json::Number val, Deserialize_Storage&)
    {
        json::Number integral;
        if (val >= json::Number(std::numeric_limits<T>::min()) //
            && val <= json::Number(std::numeric_limits<T>::max()) //
            && std::modf(val, &integral) == 0.f) {
            return static_cast<T>(integral);
        }
        return {};
    }
    [[nodiscard]]
    static json::Number to_json(const T val, std::pmr::memory_resource* const)
    {
        return json::Number(val);
    }
};

template <>
struct Serializer<std::u8string_view> {
    [[nodiscard]]
    static std::optional<std::u8string_view>
    from_json(const json::String& val, Deserialize_Storage&)
    {
        return val;
    }
    [[nodiscard]]
    static json::String
    to_json(const std::u8string_view val, std::pmr::memory_resource* const memory)
    {
        return json::String { val, memory };
    }
};

template <typename E>
    requires std::is_enum_v<E>
struct Enum_Serializer {
    [[nodiscard]]
    static std::optional<E> from_json(const json::Number val, Deserialize_Storage& storage)
    {
        if (const std::optional<Integer> integer = Serializer<Integer>::from_json(val, storage)) {
            return E(*integer);
        }
        return {};
    }
    [[nodiscard]]
    static json::Number to_json(const E val, std::pmr::memory_resource* const)
    {
        return json::Number(std::underlying_type_t<E>(val));
    }
};

template <>
struct Serializer<Error_Code> : Enum_Serializer<Error_Code> { };

template <typename... Ts>
std::optional<std::variant<Ts...>>
variant_from_json(const json::Value& val, Deserialize_Storage& storage)
{
    std::optional<std::variant<Ts...>> result;
    ([&]() -> bool {
        auto deserialized = from_json<Ts>(val, storage);
        if (!deserialized) {
            return false;
        }
        result.emplace(std::in_place_type<Ts>, std::move(*deserialized));
        return true;
    }() || ...);
    return result;
}

template <typename... Ts>
struct Serializer<std::variant<Ts...>> {
public:
    [[nodiscard]]
    static std::optional<std::variant<Ts...>>
    from_json(const json::Value& val, Deserialize_Storage& storage)
    {
        return variant_from_json<Ts...>(val, storage);
    }
    [[nodiscard]]
    static json::Value
    to_json(const std::variant<Ts...>& val, std::pmr::memory_resource* const memory)
    {
        return std::visit(
            [memory](const auto& alt) -> json::Value {
                return json::Value {
                    Serializer<std::decay_t<decltype(alt)>>::to_json(alt, memory)
                };
            },
            val
        );
    }
};

template <typename T>
struct Serializer<std::span<const T>> {
    [[nodiscard]]
    static std::optional<std::span<const T>>
    from_json(const json::Array& val, Deserialize_Storage& storage)
    {
        std::vector<T> vector;
        vector.reserve(val.size());
        for (const json::Value& element : val) {
            std::optional<T> element_value = lsp::from_json<T>(element, storage);
            if (!element_value) {
                return {};
            }
            vector.push_back(std::move(*element_value));
        }
        return storage.emplace(std::move(vector));
    }
    [[nodiscard]]
    static json::Array
    to_json(const std::span<const T> span, std::pmr::memory_resource* const memory)
    {
        json::Array result { memory };
        result.reserve(span.size());
        for (const T& item : span) {
            result.push_back(json::Value { Serializer<T>::to_json(item, memory) });
        }
        return result;
    }
};

using Member_Name = Fixed_String8<32>;

template <typename T>
struct Member_Description;

template <typename T, typename C>
struct Member_Description<T C::*> {
    using type = T;
    using class_type = C;

    /// The name of the data member in the serialized JSON object.
    Member_Name name;
    /// The pointer to the non-static data member.
    T C::* member_ptr;
};

[[nodiscard]]
consteval Member_Name to_camel_case(const Member_Name snake_case)
{
    Member_Name result;
    bool capitalize_next = false;
    for (const char8_t c : snake_case) {
        if (c == u8'_') {
            capitalize_next = true;
        }
        else if (capitalize_next) {
            result.push_back(to_ascii_upper(c));
            capitalize_next = false;
        }
        else {
            result.push_back(c);
        }
    }
    return result;
}

static_assert(to_camel_case(u8"snake"sv) == u8"snake"sv);
static_assert(to_camel_case(u8"snake_case"sv) == u8"snakeCase"sv);

// clang-format off
#define COWEL_MEMBER_DESCRIPTION(C, name)                                                          \
    Member_Description<decltype(&C::name)> { to_camel_case(u8## #name##sv), &C::name }
// clang-format on

template <typename T>
inline constexpr bool is_optional_v = false;
template <typename T>
inline constexpr bool is_optional_v<std::optional<T>> = true;

template <Member_Description Member>
bool extract_member(
    const json::Object& obj,
    typename decltype(Member)::class_type& result,
    Deserialize_Storage& storage
)
{
    using value_type = typename decltype(Member)::type;
    const json::Value* const field = obj.find_value(std::u8string_view { Member.name });
    if constexpr (is_optional_v<value_type>) {
        if (field == nullptr) {
            result.*Member.member_ptr = std::nullopt;
            return true;
        }
        using inner = typename value_type::value_type;
        auto deserialized = from_json<inner>(*field, storage);
        if (!deserialized) {
            return false;
        }
        result.*Member.member_ptr = std::move(*deserialized);
        return true;
    }
    else if constexpr (std::is_pointer_v<value_type>) {
        // Nullable pointer (e.g. const json::Value*): absent means nullptr.
        result.*Member.member_ptr = field;
        return true;
    }
    else {
        if (field == nullptr) {
            return false;
        }
        auto deserialized = from_json<value_type>(*field, storage);
        if (!deserialized) {
            return false;
        }
        result.*Member.member_ptr = std::move(*deserialized);
        return true;
    }
}

template <Member_Description Member>
void append_member(json::Object& out, const typename decltype(Member)::class_type& val)
{
    using value_type = typename decltype(Member)::type;
    auto* const memory = out.get_allocator().resource();
    if constexpr (is_optional_v<value_type>) {
        const auto& opt = val.*Member.member_ptr;
        if (!opt) {
            return;
        }
        using inner = typename value_type::value_type;
        out.push_back(
            {
                json::String { std::u8string_view { Member.name }, memory },
                json::Value { to_json<inner>(*opt, memory) },
            }
        );
    }
    else if constexpr (std::is_pointer_v<value_type>) {
        // Nullable pointer (e.g. const json::Value*): omit if null.
        const auto* const ptr = val.*Member.member_ptr;
        if (ptr == nullptr) {
            return;
        }
        out.push_back({ json::String { std::u8string_view { Member.name }, memory }, *ptr });
    }
    else {
        out.push_back(
            {
                json::String { std::u8string_view { Member.name }, memory },
                json::Value { to_json<value_type>(val.*Member.member_ptr, memory) },
            }
        );
    }
}

template <typename T, Member_Description... Members>
struct Object_Serializer {
    static_assert((std::is_same_v<T, typename decltype(Members)::class_type> && ...));
    [[nodiscard]]
    static std::optional<T> from_json(const json::Object& val, Deserialize_Storage& storage)
    {
        T result {};
        if (!((extract_member<Members>(val, result, storage)) && ...)) {
            return {};
        }
        return result;
    }
    [[nodiscard]]
    static json::Object to_json(const T& val, std::pmr::memory_resource* const memory)
    {
        json::Object result { memory };
        (append_member<Members>(result, val), ...);
        return result;
    }
};

} // namespace detail

namespace method {

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#initialize
inline constexpr auto initialize = u8"initialize"sv;
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#initialized
inline constexpr auto initialized = u8"initialized"sv;
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#shutdown
inline constexpr auto shutdown = u8"shutdown"sv;
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#exit
inline constexpr auto exit = u8"exit"sv;

namespace text_document {

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_publishDiagnostics
inline constexpr auto publish_diagnostics = u8"textDocument/publishDiagnostics"sv;
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_didOpen
inline constexpr auto did_open = u8"textDocument/didOpen"sv;
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_didChange
inline constexpr auto did_change = u8"textDocument/didChange"sv;
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_didClose
inline constexpr auto did_close = u8"textDocument/didClose"sv;
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_completion
inline constexpr auto completion = u8"textDocument/completion"sv;
/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_hover
inline constexpr auto hover = u8"textDocument/hover"sv;

} // namespace text_document
} // namespace method

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#position
namespace position_encoding_kind {

/// Character offsets count UTF-8 code units (e.g bytes).
inline constexpr auto utf8 = u8"utf-8"sv;
/// Character offsets count UTF-16 code units.
inline constexpr auto utf16 = u8"utf-16"sv;
/// Character offsets count UTF-32 code units.
inline constexpr auto utf32 = u8"utf-32"sv;

} // namespace position_encoding_kind

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#position
struct Position {
    /// Zero-based line index.
    std::size_t line;
    /// Zero-based character offset within the line.
    std::size_t character;

    friend bool operator==(const Position&, const Position&) = default;
};

namespace detail {

template <>
struct Serializer<Position>
    : Object_Serializer<
          Position, //
          COWEL_MEMBER_DESCRIPTION(Position, line),
          COWEL_MEMBER_DESCRIPTION(Position, character)> { };

} // namespace detail

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#range
struct Range {
    /// Inclusive start position.
    Position start;
    /// Exclusive end position.
    Position end;

    friend bool operator==(const Range&, const Range&) = default;
};

namespace detail {

template <>
struct Serializer<Range>
    : Object_Serializer<
          Range, //
          COWEL_MEMBER_DESCRIPTION(Range, start),
          COWEL_MEMBER_DESCRIPTION(Range, end)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnosticSeverity
enum class Diagnostic_Severity : Default_Underlying {
    error = 1,
    warning = 2,
    information = 3,
    hint = 4,
};

namespace detail {

template <>
struct Serializer<Diagnostic_Severity> {
    [[nodiscard]]
    static std::optional<Diagnostic_Severity>
    from_json(const json::Number& val, Deserialize_Storage& storage)
    {
        if (const auto integer = Serializer<Default_Underlying>::from_json(val, storage)) {
            return Diagnostic_Severity(*integer);
        }
        return {};
    }
    [[nodiscard]]
    static json::Number to_json(const Diagnostic_Severity val, std::pmr::memory_resource* const)
    {
        return json::Number(Default_Underlying(val));
    }
};

} // namespace detail

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnostic
struct Diagnostic {
    /// The range at which the message applies.
    Range range;
    /// The diagnostic's severity.
    std::optional<Diagnostic_Severity> severity = {};
    /// The diagnostic's code, which might appear in the user interface.
    std::u8string_view code;
    /// A human-readable string describing the source of this diagnostic.
    std::u8string_view source;
    /// The diagnostic's message.
    std::u8string_view message;

    friend bool operator==(const Diagnostic&, const Diagnostic&) = default;
};

namespace detail {

template <>
struct Serializer<Diagnostic>
    : Object_Serializer<
          Diagnostic, //
          COWEL_MEMBER_DESCRIPTION(Diagnostic, range),
          COWEL_MEMBER_DESCRIPTION(Diagnostic, severity),
          COWEL_MEMBER_DESCRIPTION(Diagnostic, code),
          COWEL_MEMBER_DESCRIPTION(Diagnostic, source),
          COWEL_MEMBER_DESCRIPTION(Diagnostic, message)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#publishDiagnosticsParams
struct Publish_Diagnostics_Params {
    std::u8string_view uri;
    std::span<const Diagnostic> diagnostics;
};

namespace detail {

template <>
struct Serializer<Publish_Diagnostics_Params>
    : Object_Serializer<
          Publish_Diagnostics_Params,
          COWEL_MEMBER_DESCRIPTION(Publish_Diagnostics_Params, uri),
          COWEL_MEMBER_DESCRIPTION(Publish_Diagnostics_Params, diagnostics)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocumentSyncOptions
struct Text_Document_Sync_Options {
    bool open_close;
    int change;

    friend bool operator==(const Text_Document_Sync_Options&, const Text_Document_Sync_Options&)
        = default;
};

namespace detail {

template <>
struct Serializer<Text_Document_Sync_Options>
    : Object_Serializer<
          Text_Document_Sync_Options,
          COWEL_MEMBER_DESCRIPTION(Text_Document_Sync_Options, open_close),
          COWEL_MEMBER_DESCRIPTION(Text_Document_Sync_Options, change)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.18/specification/#serverCompletionItemOptions
struct Server_Completion_Item_Options {
    /// The server has support for completion item label  details
    /// when receiving a completion item in a resolve call.
    /// @see Completion_Item_Label_Details
    std::optional<bool> label_details_support = {};

    friend bool
    operator==(const Server_Completion_Item_Options&, const Server_Completion_Item_Options&)
        = default;
};

namespace detail {

template <>
struct Serializer<Server_Completion_Item_Options>
    : Object_Serializer<
          Server_Completion_Item_Options,
          COWEL_MEMBER_DESCRIPTION(Server_Completion_Item_Options, label_details_support)> { };

template <typename T>
constexpr bool deep_equal(const std::optional<std::span<T>> x, const std::optional<std::span<T>> y)
{
    if (!x.has_value() && !y.has_value()) {
        return true;
    }
    if (x.has_value() != y.has_value() || x->size() != y->size()) {
        return false;
    }
    for (std::size_t i = 0; i < x->size(); ++i) {
        if ((*x)[i] != (*y)[i]) {
            return false;
        }
    }
    return true;
}

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.18/specification/#completionOptions
struct Completion_Options {
    std::optional<bool> work_done_progres = {};
    /// Most tools trigger completion request automatically without explicitly
    /// requesting it using a keyboard shortcut (e.g., Ctrl+Space).
    /// Typically they do so when the user starts to type an identifier.
    /// For example, if the user types `c` in a JavaScript file,
    /// code complete will automatically pop up and present `console` as a completion item.
    /// Characters that make up identifiers don't need to be listed here.
    ///
    /// If code complete should automatically be triggered on characters not being
    /// valid inside an identifier (for example, `.` in JavaScript),
    /// list them in `triggerCharacters`.
    std::optional<std::span<const std::u8string_view>> trigger_characters = {};
    /// The list of all possible characters that commit a completion.
    /// This field can be used if clients don't support individual commit characters per
    /// completion item.
    /// See client capability `completion.completion_item.commit_characters_support`.
    ///
    /// If a server provides both `all_commit_characters` and commit characters on
    /// an individual completion item, the ones on the completion item win.
    std::optional<std::span<const std::u8string_view>> all_commit_characters = {};
    /// The server provides support to resolve additional information for a completion item.
    std::optional<bool> resolve_provider = {};
    /// The server supports the following `Completion_Item` specific capabilities.
    std::optional<Server_Completion_Item_Options> completion_item = {};

    [[nodiscard]]
    friend bool operator==(const Completion_Options& a, const Completion_Options& b) noexcept
    {
        return a.work_done_progres == b.work_done_progres //
            && detail::deep_equal(a.trigger_characters, b.trigger_characters) //
            && detail::deep_equal(a.all_commit_characters, b.all_commit_characters) //
            && a.resolve_provider == b.resolve_provider //
            && a.completion_item == b.completion_item;
    }
};

namespace detail {

template <>
struct Serializer<Completion_Options>
    : Object_Serializer<
          Completion_Options,
          COWEL_MEMBER_DESCRIPTION(Completion_Options, trigger_characters),
          COWEL_MEMBER_DESCRIPTION(Completion_Options, all_commit_characters),
          COWEL_MEMBER_DESCRIPTION(Completion_Options, resolve_provider),
          COWEL_MEMBER_DESCRIPTION(Completion_Options, completion_item)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#serverCapabilities
struct Server_Capabilities {
    /// The position encoding chosen by the server.
    /// If omitted it defaults to "utf-16".
    std::optional<std::u8string_view> position_encoding = {};
    /// Defines how text documents are synced.
    Text_Document_Sync_Options text_document_sync;
    /// Whether hover is supported.
    bool hover_provider = false;
    /// The server provides completion support.
    std::optional<Completion_Options> completion_provider = {};

    friend bool operator==(const Server_Capabilities&, const Server_Capabilities&) = default;
};

namespace detail {

template <>
struct Serializer<Server_Capabilities>
    : Object_Serializer<
          Server_Capabilities,
          COWEL_MEMBER_DESCRIPTION(Server_Capabilities, position_encoding),
          COWEL_MEMBER_DESCRIPTION(Server_Capabilities, text_document_sync),
          COWEL_MEMBER_DESCRIPTION(Server_Capabilities, hover_provider),
          COWEL_MEMBER_DESCRIPTION(Server_Capabilities, completion_provider)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#initializeResult
struct Server_Info {
    std::u8string_view name;

    friend bool operator==(const Server_Info&, const Server_Info&) = default;
};

namespace detail {

template <>
struct Serializer<Server_Info>
    : Object_Serializer<
          Server_Info, //
          COWEL_MEMBER_DESCRIPTION(Server_Info, name)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#generalClientCapabilities
struct General_Client_Capabilities {
    /// The position encodings supported by the client.
    /// Defaults to ["utf-16"] when absent.
    std::optional<std::span<const std::u8string_view>> position_encodings = {};
};

namespace detail {

template <>
struct Serializer<General_Client_Capabilities>
    : Object_Serializer<
          General_Client_Capabilities,
          COWEL_MEMBER_DESCRIPTION(General_Client_Capabilities, position_encodings)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#clientCapabilities
struct Client_Capabilities {
    std::optional<General_Client_Capabilities> general = {};
};

namespace detail {

template <>
struct Serializer<Client_Capabilities>
    : Object_Serializer<
          Client_Capabilities,
          COWEL_MEMBER_DESCRIPTION(Client_Capabilities, general)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#initializeParams
struct Initialize_Params {
    std::optional<Client_Capabilities> capabilities = {};
};

namespace detail {

template <>
struct Serializer<Initialize_Params>
    : Object_Serializer<
          Initialize_Params,
          COWEL_MEMBER_DESCRIPTION(Initialize_Params, capabilities)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#initializeResult
struct Initialize_Result {
    Server_Capabilities capabilities;
    Server_Info server_info;
};

namespace detail {

template <>
struct Serializer<Initialize_Result>
    : Object_Serializer<
          Initialize_Result,
          COWEL_MEMBER_DESCRIPTION(Initialize_Result, capabilities),
          COWEL_MEMBER_DESCRIPTION(Initialize_Result, server_info)> { };

} // namespace detail

// ── Text document types ───────────────────────────────────────────────────────

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocumentItem
struct Text_Document_Item {
    std::u8string_view uri;
    std::u8string_view language_id;
    Integer version;
    std::u8string_view text;

    friend bool operator==(const Text_Document_Item&, const Text_Document_Item&) = default;
};

namespace detail {

template <>
struct Serializer<Text_Document_Item>
    : Object_Serializer<
          Text_Document_Item,
          COWEL_MEMBER_DESCRIPTION(Text_Document_Item, uri),
          COWEL_MEMBER_DESCRIPTION(Text_Document_Item, language_id),
          COWEL_MEMBER_DESCRIPTION(Text_Document_Item, version),
          COWEL_MEMBER_DESCRIPTION(Text_Document_Item, text)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocumentIdentifier
struct Text_Document_Identifier {
    std::u8string_view uri;

    friend bool operator==(const Text_Document_Identifier&, const Text_Document_Identifier&)
        = default;
};

namespace detail {

template <>
struct Serializer<Text_Document_Identifier>
    : Object_Serializer<
          Text_Document_Identifier,
          COWEL_MEMBER_DESCRIPTION(Text_Document_Identifier, uri)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#versionedTextDocumentIdentifier
struct Versioned_Text_Document_Identifier {
    std::u8string_view uri;
    Integer version;

    friend bool
    operator==(const Versioned_Text_Document_Identifier&, const Versioned_Text_Document_Identifier&)
        = default;
};

namespace detail {

template <>
struct Serializer<Versioned_Text_Document_Identifier>
    : Object_Serializer<
          Versioned_Text_Document_Identifier,
          COWEL_MEMBER_DESCRIPTION(Versioned_Text_Document_Identifier, uri),
          COWEL_MEMBER_DESCRIPTION(Versioned_Text_Document_Identifier, version)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocumentContentChangeEvent
struct Text_Document_Content_Change_Event {
    std::optional<Range> range = {};
    std::u8string_view text;

    friend bool
    operator==(const Text_Document_Content_Change_Event&, const Text_Document_Content_Change_Event&)
        = default;
};

namespace detail {

template <>
struct Serializer<Text_Document_Content_Change_Event>
    : Object_Serializer<
          Text_Document_Content_Change_Event,
          COWEL_MEMBER_DESCRIPTION(Text_Document_Content_Change_Event, range),
          COWEL_MEMBER_DESCRIPTION(Text_Document_Content_Change_Event, text)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#didOpenTextDocumentParams
struct Did_Open_Text_Document_Params {
    Text_Document_Item text_document;

    friend bool
    operator==(const Did_Open_Text_Document_Params&, const Did_Open_Text_Document_Params&)
        = default;
};

namespace detail {

template <>
struct Serializer<Did_Open_Text_Document_Params>
    : Object_Serializer<
          Did_Open_Text_Document_Params,
          COWEL_MEMBER_DESCRIPTION(Did_Open_Text_Document_Params, text_document)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#didChangeTextDocumentParams
struct Did_Change_Text_Document_Params {
    Versioned_Text_Document_Identifier text_document;
    std::span<const Text_Document_Content_Change_Event> content_changes;
};

namespace detail {

template <>
struct Serializer<Did_Change_Text_Document_Params>
    : Object_Serializer<
          Did_Change_Text_Document_Params,
          COWEL_MEMBER_DESCRIPTION(Did_Change_Text_Document_Params, text_document),
          COWEL_MEMBER_DESCRIPTION(Did_Change_Text_Document_Params, content_changes)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#didCloseTextDocumentParams
struct Did_Close_Text_Document_Params {
    Text_Document_Identifier text_document;

    friend bool
    operator==(const Did_Close_Text_Document_Params&, const Did_Close_Text_Document_Params&)
        = default;
};

namespace detail {

template <>
struct Serializer<Did_Close_Text_Document_Params>
    : Object_Serializer<
          Did_Close_Text_Document_Params,
          COWEL_MEMBER_DESCRIPTION(Did_Close_Text_Document_Params, text_document)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#requestMessage
struct Request_Message {
    using Id = std::variant<Integer, std::u8string_view>;

    std::u8string_view jsonrpc = u8"2.0"sv;
    /// The request id.
    Id id;
    /// The method to be invoked.
    std::u8string_view method;
    /// The method's params.
    const json::Value* params = nullptr;

    friend bool operator==(const Request_Message&, const Request_Message&) = default;
};

namespace detail {

template <>
struct Serializer<Request_Message>
    : Object_Serializer<
          Request_Message,
          COWEL_MEMBER_DESCRIPTION(Request_Message, jsonrpc),
          COWEL_MEMBER_DESCRIPTION(Request_Message, id),
          COWEL_MEMBER_DESCRIPTION(Request_Message, method),
          COWEL_MEMBER_DESCRIPTION(Request_Message, params)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#responseError
struct Response_Error {
    /// A number indicating the error type that occurred.
    Error_Code code;
    /// A string providing a short description of the error.
    std::u8string_view message;
    /// A primitive or structured value that contains additional information about the error.
    std::optional<json::Value> data = {};

    friend bool operator==(const Response_Error&, const Response_Error&) = default;
};

namespace detail {

template <>
struct Serializer<Response_Error>
    : Object_Serializer<
          Response_Error,
          COWEL_MEMBER_DESCRIPTION(Response_Error, code),
          COWEL_MEMBER_DESCRIPTION(Response_Error, message),
          COWEL_MEMBER_DESCRIPTION(Response_Error, data)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#responseMessage
struct Response_Message {
    using Id = std::variant<Integer, std::u8string_view, Null>;

    std::u8string_view jsonrpc = u8"2.0"sv;
    Id id;
    std::optional<json::Value> result = {};
    std::optional<Response_Error> error = {};

    friend bool operator==(const Response_Message&, const Response_Message&) = default;
};

/// @brief Widens a request id to a response id.
[[nodiscard]]
inline auto to_response_id(const Request_Message::Id& request_id) -> lsp::Response_Message::Id
{
    if (const auto* const n = std::get_if<Integer>(&request_id)) {
        return *n;
    }
    return std::get<std::u8string_view>(request_id);
}

namespace detail {

template <>
struct Serializer<Response_Message>
    : Object_Serializer<
          Response_Message,
          COWEL_MEMBER_DESCRIPTION(Response_Message, jsonrpc),
          COWEL_MEMBER_DESCRIPTION(Response_Message, id),
          COWEL_MEMBER_DESCRIPTION(Response_Message, result),
          COWEL_MEMBER_DESCRIPTION(Response_Message, error)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#notificationMessage
struct Notification_Message {
    std::u8string_view jsonrpc = u8"2.0"sv;
    /// The method to be invoked.
    std::u8string_view method;
    /// The notification's params.
    std::optional<std::variant<std::span<const json::Array>, json::Object>> params;

    friend bool operator==(const Notification_Message&, const Notification_Message&) = default;
};

namespace detail {

template <>
struct Serializer<Notification_Message>
    : Object_Serializer<
          Notification_Message,
          COWEL_MEMBER_DESCRIPTION(Notification_Message, jsonrpc),
          COWEL_MEMBER_DESCRIPTION(Notification_Message, method),
          COWEL_MEMBER_DESCRIPTION(Notification_Message, params)> { };

} // namespace detail

template <json_serializable T>
    requires(!std::is_same_v<detail::json_type_t<T>, json::Value>)
std::optional<T> from_json(const detail::json_type_t<T>& val, Deserialize_Storage& storage)
{
    return detail::Serializer<T>::from_json(val, storage);
}
template <json_serializable T>
std::optional<T> from_json(const json::Value& val, Deserialize_Storage& storage)
{
    if constexpr (std::is_same_v<detail::json_type_t<T>, json::Value>) {
        return detail::Serializer<T>::from_json(val, storage);
    }
    else {
        const auto* const alternative = std::get_if<detail::json_type_t<T>>(&val);
        if (alternative) {
            return detail::Serializer<T>::from_json(*alternative, storage);
        }
        return {};
    }
}

template <json_serializable T>
detail::json_type_t<T> to_json(const T& val, std::pmr::memory_resource* const memory)
{
    return detail::Serializer<T>::to_json(val, memory);
}

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.18/specification/#markupContent
namespace markup_kind {

inline constexpr auto plain_text = u8"plaintext"sv;
inline constexpr auto markdown = u8"markdown"sv;

} // namespace markup_kind

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.18/specification/#markupContentInnerDefinition
struct Markup_Content {
    /// The type of the markup.
    std::u8string_view kind;
    /// The content itself
    std::u8string_view value;
};

namespace detail {

template <>
struct Serializer<Markup_Content>
    : Object_Serializer<
          Markup_Content,
          COWEL_MEMBER_DESCRIPTION(Markup_Content, kind),
          COWEL_MEMBER_DESCRIPTION(Markup_Content, value)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.18/specification/#hoverClientCapabilities
struct Hover_Client_Capabilities {
    /// Whether hover supports dynamic registration.
    std::optional<bool> dynamic_registration = {};
    /// Client supports the following content formats
    /// if the content property refers to a `literal of type MarkupContent`.
    /// The order describes the preferred format of the client.
    std::span<const std::u8string_view> content_format;
};

namespace detail {

template <>
struct Serializer<Hover_Client_Capabilities>
    : Object_Serializer<
          Hover_Client_Capabilities,
          COWEL_MEMBER_DESCRIPTION(Hover_Client_Capabilities, dynamic_registration),
          COWEL_MEMBER_DESCRIPTION(Hover_Client_Capabilities, content_format)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.18/specification/#completionItemKind
enum struct CompletionItemKind : unsigned char {
    Text = 1,
    Method = 2,
    Function = 3,
    Constructor = 4,
    Field = 5,
    Variable = 6,
    Class = 7,
    Interface = 8,
    Module = 9,
    Property = 10,
    Unit = 11,
    Value = 12,
    Enum = 13,
    Keyword = 14,
    Snippet = 15,
    Color = 16,
    File = 17,
    Reference = 18,
    Folder = 19,
    EnumMember = 20,
    Constant = 21,
    Struct = 22,
    Event = 23,
    Operator = 24,
    TypeParameter = 25,
};

namespace detail {

template <>
struct Serializer<CompletionItemKind> : Enum_Serializer<CompletionItemKind> { };

} // namespace detail

struct Client_Completion_Item_Options_Kind {
    /// The completion item kind values the client supports.
    /// When this property exists the client also guarantees that it will
    /// handle values outside its set gracefully and falls back
    /// to a default value when unknown.
    ///
    /// If this property is not present the client only supports
    /// the completion items kinds from `Text` to `Reference` as defined in
    /// the initial version of the protocol.
    std::optional<std::span<const CompletionItemKind>> value_set = {};
};

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.18/specification/#clientCompletionItemOptions
struct Client_Completion_Item_Options {
    /// Client supports snippets as insert text.
    ///
    /// A snippet can define tab stops and placeholders with `$1`, `$2` and `${3:foo}`.
    /// `$0` defines the final tab stop,
    /// it defaults to the end of the snippet.
    /// Placeholders with equal identifiers are linked,
    /// that is typing in one will update others too.
    std::optional<bool> snippet_support = {};
    /// Client supports commit characters on a completion item.
    std::optional<bool> commit_characters_support = {};
    /// Client supports the following content formats for the documentation property.
    /// The order describes the preferred format of the client.
    std::optional<std::span<const std::u8string_view>> documentation_format = {};
    /// Client supports the deprecated property on a completion item.
    std::optional<bool> deprecated_support;
    /// Client supports the preselect property on a completion item.
    std::optional<bool> preselect_support = {};
    /// Client support insert replace edit to control different behavior if a
    /// completion item is inserted in the text or should replace text.
    std::optional<bool> insert_replace_support = {};
};

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.18/specification/#completionClientCapabilities
struct Completion_Client_Capabilities {
    /// Whether completion supports dynamic registration.
    std::optional<bool> dynamic_registration = {};
    /// The client supports sending additional context information
    /// for a `textDocument/completion` request.
    std::optional<bool> context_support = {};
};

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.18/specification/#markdownClientCapabilities
struct Markdown_Client_Capabilities {
    /// The name of the parser.
    std::u8string_view parser;
    /// The version of the parser.
    std::optional<std::u8string_view> version = {};
    /// A list of HTML tags that the client allows / supports in Markdown.
    std::optional<std::span<const std::u8string_view>> allowed_tags = {};
};

namespace detail {

template <>
struct Serializer<Markdown_Client_Capabilities>
    : Object_Serializer<
          Markdown_Client_Capabilities,
          COWEL_MEMBER_DESCRIPTION(Markdown_Client_Capabilities, parser),
          COWEL_MEMBER_DESCRIPTION(Markdown_Client_Capabilities, version),
          COWEL_MEMBER_DESCRIPTION(Markdown_Client_Capabilities, allowed_tags)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.18/specification/#completionTriggerKind
enum struct Completion_Trigger_Kind : unsigned char {
    /// Completion was triggered by typing an identifier (automatic code  complete),
    /// manual invocation (e.g. Ctrl+Space) or via API.
    invoked,
    /// Completion was triggered by a trigger character
    /// specified by the `trigger_characters` properties of the `Completion_Registration_Options`.
    trigger_character,
    /// Completion was re-triggered as the current completion list is incomplete.
    trigger_for_incomplete_completions,
};

namespace detail {

template <>
struct Serializer<Completion_Trigger_Kind> : Enum_Serializer<Completion_Trigger_Kind> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.18/specification/#completionContext
struct Completion_Context {
    /// How the completion was triggered.
    Completion_Trigger_Kind trigger_kind;
    /// The trigger character (a single character) that has triggered code complete.
    /// Is undefined if `triggerKind !== Completion_Trigger_Kind::trigger_character`.
    std::optional<std::u8string_view> trigger_character = {};
};

namespace detail {

template <>
struct Serializer<Completion_Context>
    : Object_Serializer<
          Completion_Context,
          COWEL_MEMBER_DESCRIPTION(Completion_Context, trigger_kind),
          COWEL_MEMBER_DESCRIPTION(Completion_Context, trigger_character)> { };

} // namespace detail

using Progress_Token = std::variant<Integer, std::u8string_view>;

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.18/specification/#completionParams
struct Completion_Params {
    /// The text document.
    Text_Document_Identifier text_document;
    /// The position inside the text document.
    Position position;
    /// An optional token that a server can use to report work done progress.
    std::optional<Progress_Token> work_done_token = {};
    /// An optional token that a server can use to report partial results
    /// (e.g. streaming) to the client.
    std::optional<Progress_Token> partial_result_token = {};
    /// The completion context.
    /// This is only available if the client specifies to send this using the client capability
    /// `completion.context_support` == true`.
    std::optional<Completion_Context> context = {};
};

namespace detail {

template <>
struct Serializer<Completion_Params>
    : Object_Serializer<
          Completion_Params,
          COWEL_MEMBER_DESCRIPTION(Completion_Params, text_document),
          COWEL_MEMBER_DESCRIPTION(Completion_Params, position),
          COWEL_MEMBER_DESCRIPTION(Completion_Params, work_done_token),
          COWEL_MEMBER_DESCRIPTION(Completion_Params, partial_result_token),
          COWEL_MEMBER_DESCRIPTION(Completion_Params, context)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.18/specification/#hoverParams
struct Hover_Params {
    /// The text document.
    Text_Document_Identifier text_document;
    /// The position inside the text document.
    Position position;
    /// An optional token that a server can use to report work done progress.
    std::optional<Progress_Token> work_done_token = {};
};

namespace detail {

template <>
struct Serializer<Hover_Params>
    : Object_Serializer<
          Hover_Params,
          COWEL_MEMBER_DESCRIPTION(Hover_Params, text_document),
          COWEL_MEMBER_DESCRIPTION(Hover_Params, position),
          COWEL_MEMBER_DESCRIPTION(Hover_Params, work_done_token)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.18/specification/#hover
struct Hover {
    /// The hover's content.
    Markup_Content contents;
    /// An optional range is a range inside a text document that is used to visualize a hover,
    /// e.g. by changing the background color.
    std::optional<Range> range = {};
};

namespace detail {

template <>
struct Serializer<Hover>
    : Object_Serializer<
          Hover,
          COWEL_MEMBER_DESCRIPTION(Hover, contents),
          COWEL_MEMBER_DESCRIPTION(Hover, range)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textEdit
struct Text_Edit {
    /// The range of the text document to be manipulated.
    /// To insert text into a document,
    /// create a range where `start === end`.
    Range range;
    /// The string to be inserted.
    /// For delete operations use an empty string.
    std::u8string_view new_text;
};

namespace detail {

template <>
struct Serializer<Text_Edit>
    : Object_Serializer<
          Text_Edit,
          COWEL_MEMBER_DESCRIPTION(Text_Edit, range),
          COWEL_MEMBER_DESCRIPTION(Text_Edit, new_text)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#completionItem
struct Completion_Item {
    /// The label of this completion item.
    ///
    /// The `label` property is also by default the text
    /// that is inserted when selecting this completion.
    ///
    /// If `label` details are provided,
    /// the label itself should be an unqualified name of the completion item.
    std::u8string_view label;
    /// The kind of this completion item.
    std::optional<CompletionItemKind> kind = {};
    /// A human-readable string with additional information about this item,
    /// like type or symbol information.
    std::optional<std::u8string_view> detail = {};
    /// A string that should be used when comparing this item with other items.
    // When omitted the label is used as the sort text for this item.
    std::optional<std::u8string_view> sort_text = {};
    /// A string used when filtering a set of completion items.
    /// When omitted the label is used as the filter text for this item.
    std::optional<std::u8string_view> filter_text = {};
    /// A text edit applied to the document when accepting this completion.
    /// When an edit is provided the value of `insert_text` is ignored.
    std::optional<Text_Edit> text_edit = {};
};

namespace detail {

template <>
struct Serializer<Completion_Item>
    : Object_Serializer<
          Completion_Item,
          COWEL_MEMBER_DESCRIPTION(Completion_Item, label),
          COWEL_MEMBER_DESCRIPTION(Completion_Item, kind),
          COWEL_MEMBER_DESCRIPTION(Completion_Item, detail),
          COWEL_MEMBER_DESCRIPTION(Completion_Item, sort_text),
          COWEL_MEMBER_DESCRIPTION(Completion_Item, filter_text),
          COWEL_MEMBER_DESCRIPTION(Completion_Item, text_edit)> { };

} // namespace detail

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.18/specification/#completionList
struct Completion_List {
    /// This list is not complete.
    /// Further typing should result in recomputing this list.
    bool is_incomplete;
    /// The completion items.
    std::span<const Completion_Item> items;
};

namespace detail {

template <>
struct Serializer<Completion_List>
    : Object_Serializer<
          Completion_List,
          COWEL_MEMBER_DESCRIPTION(Completion_List, is_incomplete),
          COWEL_MEMBER_DESCRIPTION(Completion_List, items)> { };

} // namespace detail

} // namespace cowel::lsp

#endif
