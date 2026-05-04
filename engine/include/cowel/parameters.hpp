#ifndef COWEL_PARAMETERS_HPP
#define COWEL_PARAMETERS_HPP

#include <algorithm>
#include <memory_resource>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/function_ref.hpp"
#include "cowel/util/small_vector.hpp"
#include "cowel/util/source_position.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"
#include "cowel/value.hpp"

namespace cowel {

enum struct Optionality : bool {
    mandatory,
    optional,
};

struct Was_Matched {

    [[nodiscard]]
    Was_Matched()
        = default;

    [[nodiscard]]
    virtual bool was_matched() const
        = 0;

    /// @brief Resets the matcher for re-matching in the future.
    /// After a call to `reset()`, `was_matched()` shall be `false`.
    virtual void reset() noexcept = 0;
};

using Fail_Callback = Function_Ref<
    void(const File_Source_Span& location, Char_Sequence8 message, Context& context)>;

struct Match_Fail_Options {
    Fail_Callback emit;
    Processing_Status status;
    const File_Source_Span& location;
};

template <
    Severity fail_severity = Severity::error,
    const std::u8string_view& diagnostic = diagnostic::type_mismatch>
consteval Fail_Callback make_fail_callback() noexcept
{
    static constexpr auto lambda
        = [](const File_Source_Span& location, Char_Sequence8 message, Context& context) {
              context.try_error(diagnostic, location, message);
          };
    return { const_v<lambda> };
}

struct Argument {
public:
    [[nodiscard]]
    static Argument positional(const ast::Group_Member& member)
    {
        COWEL_ASSERT(member.get_kind() == ast::Member_Kind::positional);
        return { Value::null, member };
    }

    [[nodiscard]]
    static Argument block(const ast::Primary& primary)
    {
        COWEL_ASSERT(primary.get_kind() == ast::Primary_Kind::block);
        return Argument { primary };
    }

    [[nodiscard]]
    static Argument named(Value name, const ast::Group_Member& member)
    {
        COWEL_ASSERT(member.get_kind() == ast::Member_Kind::named);
        return { std::move(name), member };
    }

private:
    static Result<Value, Processing_Status>
    group_member_evaluate(const void*, Frame_Index, Context&);
    static Processing_Status group_member_splice(
        const void*, //
        Content_Policy&,
        Frame_Index,
        Context&
    );
    static Processing_Status group_member_splice_to_plaintext(
        const void*, //
        std::pmr::vector<char8_t>& out,
        Frame_Index,
        Context&
    );
    static const File_Source_Span& group_member_get_source_span(const void*) noexcept;
    static const Type& group_member_get_static_type(const void*, Context&) noexcept;

    static Result<Value, Processing_Status> primary_evaluate(const void*, Frame_Index, Context&);
    static Processing_Status primary_splice(
        const void*, //
        Content_Policy&,
        Frame_Index,
        Context&
    );
    static Processing_Status primary_splice_to_plaintext(
        const void*, //
        std::pmr::vector<char8_t>& out,
        Frame_Index,
        Context&
    );
    static const File_Source_Span& primary_get_source_span(const void*) noexcept;
    static const Type& primary_get_static_type(const void*, Context&) noexcept;

    /// @brief The evaluated argument name (in the case of named arguments),
    /// or `null` in the case of positional arguments.
    Value m_name;
    /// @brief A type-erased reference to the AST node.
    const void* m_ast_node = nullptr;
    Result<Value, Processing_Status> (*m_evaluate)(const void*, Frame_Index, Context&) = nullptr;
    Processing_Status (*m_splice)(const void*, Content_Policy&, Frame_Index, Context&) = nullptr;
    Processing_Status (*m_splice_to_plaintext)(
        const void*,
        std::pmr::vector<char8_t>&,
        Frame_Index,
        Context&
    ) = nullptr;
    const File_Source_Span& (*m_get_value_location)(const void*) noexcept = nullptr;
    const Type& (*m_get_static_type)(const void*, Context&) noexcept = nullptr;
    /// @brief The location of the argument as a whole.
    /// Unlike the location of the expression,
    /// this also includes the argument name.
    File_Source_Span m_location;

    [[nodiscard]]
    Argument(Value name, const ast::Group_Member& arg) noexcept
        : m_name { std::move(name) }
        , m_ast_node { &arg }
        , m_evaluate { group_member_evaluate }
        , m_splice { group_member_splice }
        , m_splice_to_plaintext { group_member_splice_to_plaintext }
        , m_get_value_location { group_member_get_source_span }
        , m_get_static_type { group_member_get_static_type }
        , m_location { arg.get_source_span() }
    {
    }

    [[nodiscard]]
    explicit Argument(const ast::Primary& arg) noexcept
        : m_name { Value::null }
        , m_ast_node { &arg }
        , m_evaluate { primary_evaluate }
        , m_splice { primary_splice }
        , m_splice_to_plaintext { primary_splice_to_plaintext }
        , m_get_value_location { primary_get_source_span }
        , m_get_static_type { primary_get_static_type }
        , m_location { arg.get_source_span() }
    {
    }

public:
    [[nodiscard]]
    Argument()
        = default;

    [[nodiscard]]
    const Value& get_name() const
    {
        return m_name;
    }
    [[nodiscard]]
    const File_Source_Span& get_location() const
    {
        return m_location;
    }

    [[nodiscard]]
    Result<Value, Processing_Status> evaluate(const Frame_Index frame, Context& context) const
    {
        return m_evaluate(m_ast_node, frame, context);
    }
    [[nodiscard]]
    Processing_Status splice(Content_Policy& out, const Frame_Index frame, Context& context) const
    {
        return m_splice(m_ast_node, out, frame, context);
    }
    [[nodiscard]]
    Processing_Status splice_to_plaintext(
        std::pmr::vector<char8_t>& out,
        const Frame_Index frame,
        Context& context
    ) const
    {
        return m_splice_to_plaintext(m_ast_node, out, frame, context);
    }
    [[nodiscard]]
    const File_Source_Span& get_value_location() const
    {
        return m_get_value_location(m_ast_node);
    }
    [[nodiscard]]
    const Type& get_static_type(Context& context) const
    {
        return m_get_static_type(m_ast_node, context);
    }

    [[nodiscard]]
    constexpr bool is_named() const
    {
        return !m_name.is_null();
    }
    [[nodiscard]]
    constexpr bool is_positional() const
    {
        return m_name.is_null();
    }
};

template <typename T>
struct Value_And_Location {
    T value;
    Basic_File_Source_Span<File_Id> location;
};

template <typename T>
struct Value_Holder : virtual Was_Matched {
    using value_type = T;

protected:
    std::optional<Value_And_Location<T>> m_value;

public:
    [[nodiscard]]
    explicit Value_Holder()
        = default;

    [[nodiscard]]
    bool was_matched() const final
    {
        return has_value();
    }

    void reset() noexcept override
    {
        m_value.reset();
    }

    [[nodiscard]]
    bool has_value() const
    {
        return m_value.has_value();
    }

    [[nodiscard]]
    const File_Source_Span& get_location() const
    {
        return m_value->location;
    }

    [[nodiscard]]
    T& get() &
    {
        return m_value->value;
    }
    [[nodiscard]]
    const T& get() const&
    {
        return m_value->value;
    }
    [[nodiscard]]
    T&& get() &&
    {
        return std::move(m_value->value);
    }
    [[nodiscard]]
    const T&& get() const&&
    {
        return std::move(m_value->value);
    }

    [[nodiscard]]
    T get_or_default(const T& fallback) const
    {
        return m_value ? m_value->value : fallback;
    }
};

struct Value_Matcher : virtual Was_Matched {
private:
    Type m_type;
    static_assert(
        std::is_trivially_copyable_v<Type>,
        "This design needs to be reconsidered if Type is expensive to copy."
    );

public:
    [[nodiscard]]
    explicit Value_Matcher(const Type& type)
        : Was_Matched {}
        , m_type { type }
    {
        COWEL_DEBUG_ASSERT(type.is_canonical());
    }

    /// @brief Attempts matching the value contained in `argument`
    /// according to this matcher's behavior.
    /// @param argument The argument to match.
    /// @param frame The frame index of the argument.
    /// @param context The current context.
    /// @return A `Processing_Status` if content generation failed during the matching process.
    [[nodiscard]]
    virtual Processing_Status match_value(
        const Argument& argument,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    ) = 0;

    [[nodiscard]]
    const Type& get_type() const
    {
        return m_type;
    }
};

/// @brief Matches a lazy value of single specific kind.
/// This is typically used for blocks and quoted strings.
struct Lazy_Value_Of_Type_Matcher final
    : Value_Matcher
    , Value_Holder<Argument> {
private:
    Frame_Index m_markup_frame { -2 };

public:
    [[nodiscard]]
    explicit Lazy_Value_Of_Type_Matcher(const Type& expected_type)
        : Value_Matcher { expected_type }
    {
    }

    [[nodiscard]]
    Processing_Status match_value(
        const Argument& argument,
        Frame_Index frame,
        Context&,
        const Match_Fail_Options& //
    ) override;

    [[nodiscard]]
    Frame_Index get_frame() const
    {
        COWEL_ASSERT(was_matched());
        return m_markup_frame;
    }
};

struct Textual_Matcher : Value_Matcher {

    [[nodiscard]]
    explicit Textual_Matcher()
        : Value_Matcher { Type::str }
    {
    }

    [[nodiscard]]
    Processing_Status match_value(
        const Argument& argument,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    ) override;

    [[nodiscard]]
    virtual bool match_string(
        const Argument& argument,
        std::u8string_view str,
        Context& context,
        Fail_Callback on_fail
    ) = 0;
};

struct Value_Of_Type_Matcher
    : Value_Matcher
    , Value_Holder<Value> {
    [[nodiscard]]
    explicit Value_Of_Type_Matcher(const Type& expected_type)
        : Value_Matcher { expected_type }
    {
    }

    [[nodiscard]]
    Processing_Status match_value(
        const Argument& argument,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    ) override;
};

/// @brief Matches any spliceable value and splices it into a string.
struct Spliceable_To_String_Matcher final
    : Value_Matcher
    , Value_Holder<std::u8string_view> {
private:
    std::pmr::vector<char8_t> m_data;

public:
    [[nodiscard]]
    explicit Spliceable_To_String_Matcher(std::pmr::memory_resource* const memory)
        : Value_Matcher { Type::any }
        , m_data { memory }
    {
    }

    [[nodiscard]]
    Processing_Status match_value(
        const Argument& argument,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    ) override;
};

struct String_Matcher final
    : Value_Matcher
    , Value_Holder<std::u8string_view> {
private:
    std::pmr::vector<char8_t> m_data;
    String_Kind m_string_kind = String_Kind::unknown;

public:
    [[nodiscard]]
    explicit String_Matcher(std::pmr::memory_resource* const memory)
        : Value_Matcher { Type::str }
        , m_data { memory }
    {
    }

    [[nodiscard]]
    Processing_Status match_value(
        const Argument& argument,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    ) override;

    [[nodiscard]]
    String_Kind get_string_kind() const
    {
        COWEL_ASSERT(was_matched());
        return m_string_kind;
    }
};

struct Boolean_Matcher final
    : Value_Matcher
    , Value_Holder<bool> {

    [[nodiscard]]
    explicit Boolean_Matcher()
        : Value_Matcher { Type::boolean }
    {
    }

    [[nodiscard]]
    Processing_Status match_value(
        const Argument& argument,
        Frame_Index frame,
        Context&,
        const Match_Fail_Options& on_fail
    ) override;
};

struct Integer_Matcher final
    : Value_Matcher
    , Value_Holder<Big_Int> {

    [[nodiscard]]
    explicit Integer_Matcher()
        : Value_Matcher { Type::integer }
    {
    }

    [[nodiscard]]
    Processing_Status match_value(
        const Argument& argument,
        Frame_Index frame,
        Context&,
        const Match_Fail_Options& on_fail
    ) override;
};

struct Float_Matcher final
    : Value_Matcher
    , Value_Holder<Float> {

    [[nodiscard]]
    explicit Float_Matcher()
        : Value_Matcher { Type::floating }
    {
    }

    [[nodiscard]]
    Processing_Status match_value(
        const Argument& argument,
        Frame_Index frame,
        Context&,
        const Match_Fail_Options& on_fail
    ) override;
};

/// @brief Matches one of the given constant (sorted) string options.
struct Sorted_Options_Matcher final
    : Textual_Matcher
    , Value_Holder<std::size_t> {
private:
    std::span<const std::u8string_view> m_options;

public:
    [[nodiscard]]
    explicit Sorted_Options_Matcher(std::span<const std::u8string_view> options)
        : m_options { options }
    {
        COWEL_DEBUG_ASSERT(std::ranges::is_sorted(options));
    }

    [[nodiscard]]
    bool match_string(
        const Argument& argument,
        std::u8string_view str,
        Context&,
        Fail_Callback on_fail
    ) override;

    [[nodiscard]]
    std::u8string_view get_option() const
    {
        COWEL_ASSERT(was_matched());
        return m_options[m_value->value];
    }

    [[nodiscard]]
    std::u8string_view get_or_default(std::u8string_view fallback) const
    {
        return was_matched() ? fallback : m_options[m_value->value];
    }

    [[nodiscard]]
    std::size_t get_index_or_default(std::size_t fallback) const
    {
        return was_matched() ? fallback : m_value->value;
    }
};

struct Block_Matcher final
    : Value_Matcher
    , Value_Holder<Value> {

    [[nodiscard]]
    explicit Block_Matcher()
        : Value_Matcher { Type::block }
    {
    }

    [[nodiscard]]
    Processing_Status match_value(
        const Argument& argument,
        Frame_Index frame,
        Context&,
        const Match_Fail_Options& on_fail
    ) override;
};

struct Pack_Of_Type_Matcher : Value_Matcher {
public:
    using value_type = Value_And_Location<Value>;

private:
    Small_Vector<value_type, 16> m_values;

public:
    [[nodiscard]]
    explicit Pack_Of_Type_Matcher(const Type& type)
        : Value_Matcher { type }
    {
        COWEL_ASSERT(type.is_pack());
        COWEL_ASSERT(type.is_canonical());
    }

    [[nodiscard]]
    Processing_Status match_value(
        const Argument& argument,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    ) override;

    [[nodiscard]]
    bool was_matched() const override
    {
        return !m_values.empty();
    }

    void reset() noexcept override
    {
        m_values.clear();
    }

    [[nodiscard]]
    std::span<value_type> get()
    {
        return m_values;
    }
    [[nodiscard]]
    std::span<const value_type> get() const
    {
        return m_values;
    }
};

struct Pack_Lazy_Any_Matcher : Value_Matcher {
public:
    using value_type = Argument;

private:
    Small_Vector<value_type, 16> m_values;

public:
    [[nodiscard]]
    explicit Pack_Lazy_Any_Matcher()
        : Value_Matcher { Type::pack_of(&Type::any) }
    {
    }

    [[nodiscard]]
    Processing_Status match_value(
        const Argument& argument, //
        Frame_Index,
        Context&,
        const Match_Fail_Options&
    ) override
    {
        m_values.push_back(argument);
        return Processing_Status::ok;
    }

    [[nodiscard]]
    bool was_matched() const override
    {
        return !m_values.empty();
    }

    void reset() noexcept override
    {
        m_values.clear();
    }

    [[nodiscard]]
    std::span<const value_type> get() const
    {
        return m_values;
    }
};

struct Pack_Named_Of_Type_Matcher : Value_Matcher {
    using value_type = Group_Member_Value;

private:
    Small_Vector<value_type, 16> m_values;
    Small_Vector<File_Source_Span, 16> m_locations;

protected:
    const Type& m_element_type;

public:
    [[nodiscard]]
    explicit Pack_Named_Of_Type_Matcher(const Type& pack_named_type)
        : Value_Matcher { pack_named_type }
        , m_element_type { pack_named_type.get_members().front().get_members().front() }
    {
        COWEL_ASSERT(pack_named_type.is_pack());
        COWEL_ASSERT(pack_named_type.get_members().front().is_named());
        COWEL_DEBUG_ASSERT(pack_named_type.is_canonical());
    }

    [[nodiscard]]
    Processing_Status match_value(
        const Argument& argument,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    ) override;

    [[nodiscard]]
    bool was_matched() const override
    {
        return !m_values.empty();
    }

    void reset() noexcept override
    {
        m_values.clear();
    }

    [[nodiscard]]
    std::span<const value_type> get() const
    {
        return m_values;
    }

    [[nodiscard]]
    std::span<const File_Source_Span> get_locations() const
    {
        return m_locations;
    }

    [[nodiscard]]
    const Type& get_element_type() const
    {
        return m_element_type;
    }
};

namespace detail {

inline constexpr auto named_str = Type::named(&Type::str);
inline constexpr auto pack_named_str = Type::pack_of(&named_str);
inline constexpr auto group_pack_named_str = Type::group_of({ &pack_named_str, 1 });

} // namespace detail

struct Pack_Named_Str_Matcher final : Pack_Named_Of_Type_Matcher {

    [[nodiscard]]
    explicit Pack_Named_Str_Matcher()
        : Pack_Named_Of_Type_Matcher { detail::pack_named_str }
    {
    }
};

struct Group_Pack_Named_Str_Matcher final
    : Value_Matcher
    , Value_Holder<Value> {
    using value_type = const ast::Group_Member*;

    [[nodiscard]]
    explicit Group_Pack_Named_Str_Matcher()
        : Value_Matcher { detail::group_pack_named_str }
    {
    }

    [[nodiscard]]
    Processing_Status match_value(
        const Argument& argument,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    ) override;
};

struct Parameter {
private:
    std::u8string_view m_name;
    Optionality m_optionality;
    Value_Matcher& m_value_matcher;

public:
    [[nodiscard]]
    constexpr explicit Parameter(
        std::u8string_view name,
        Optionality optionality,
        Value_Matcher& value_matcher
    )
        : m_name { name }
        , m_optionality { optionality }
        , m_value_matcher { value_matcher }
    {
        COWEL_DEBUG_ASSERT(is_identifier(name));
    }

    [[nodiscard]]
    std::u8string_view get_name() const
    {
        return m_name;
    }

    [[nodiscard]]
    bool is_optional() const
    {
        return m_optionality == Optionality::optional;
    }

    [[nodiscard]]
    bool is_mandatory() const
    {
        return m_optionality == Optionality::mandatory;
    }

    [[nodiscard]]
    Value_Matcher& get_value_matcher()
    {
        return m_value_matcher;
    }
    [[nodiscard]]
    const Value_Matcher& get_value_matcher() const
    {
        return m_value_matcher;
    }

    [[nodiscard]]
    const Type& get_type() const
    {
        return m_value_matcher.get_type();
    }
};

[[nodiscard]]
Processing_Status match_call(
    std::span<Parameter* const> parameters,
    const Invocation& call,
    Context& context,
    Fail_Callback on_fail = make_fail_callback(),
    Processing_Status on_fail_status = Processing_Status::error
);

[[nodiscard]]
inline Processing_Status match_call_fatal_error(
    const std::span<Parameter* const> parameters,
    const Invocation& call,
    Context& context,
    const Fail_Callback on_fail = make_fail_callback<Severity::fatal>()
)
{
    return match_call(parameters, call, context, on_fail, Processing_Status::fatal);
}

} // namespace cowel

#endif
