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
#include "cowel/util/source_position.hpp"

#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"

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

// VALUE ===========================================================================================

struct Value_Matcher : virtual Was_Matched {
    [[nodiscard]]
    explicit Value_Matcher()
        : Was_Matched {}
    {
    }

    /// @brief Attempts matching the value contained in `argument`
    /// according to this matcher's behavior.
    /// @param argument The argument to match.
    /// @param frame The frame index of the argument.
    /// @param context The current context.
    /// @return A `Processing_Status` if content generation failed during the matching process.
    [[nodiscard]]
    virtual Processing_Status match_value(
        const ast::Member_Value& argument,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    ) = 0;
};

/// @brief Matches a lazy value of single specific kind.
/// This is typically used for blocks and quoted strings.
struct Lazy_Value_Of_Type_Matcher final : Value_Matcher {
private:
    const Type* m_expected_type;
    const ast::Member_Value* m_markup = nullptr;
    Frame_Index m_markup_frame { -2 };

public:
    [[nodiscard]]
    explicit Lazy_Value_Of_Type_Matcher(const Type* expected_type)
        : m_expected_type { expected_type }
    {
        COWEL_ASSERT(expected_type);
    }

    [[nodiscard]]
    bool was_matched() const override
    {
        return m_markup != nullptr;
    }

    void reset() noexcept override
    {
        m_markup = nullptr;
    }

    [[nodiscard]]
    const ast::Member_Value& get() const
    {
        COWEL_ASSERT(was_matched());
        return *m_markup;
    }

    [[nodiscard]]
    const Type& get_expected_type() const
    {
        return *m_expected_type;
    }

    [[nodiscard]]
    Frame_Index get_frame() const
    {
        return m_markup_frame;
    }

    [[nodiscard]]
    const ast::Member_Value* get_or_null() const
    {
        return m_markup;
    }

    [[nodiscard]]
    Processing_Status match_value(
        const ast::Member_Value& argument,
        Frame_Index frame,
        Context&,
        const Match_Fail_Options& //
    ) override;
};

struct Textual_Matcher : Value_Matcher {

    [[nodiscard]]
    explicit Textual_Matcher()
        = default;

    [[nodiscard]]
    Processing_Status match_value(
        const ast::Member_Value& argument,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    ) override;

    [[nodiscard]]
    virtual bool match_string(
        const ast::Member_Value& argument,
        std::u8string_view str,
        Context& context,
        Fail_Callback on_fail
    ) = 0;
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

struct Value_Of_Type_Matcher : Value_Matcher, Value_Holder<Value> {
private:
    const Type* m_expected_type;

public:
    [[nodiscard]]
    explicit Value_Of_Type_Matcher(const Type* expected_type)
        : m_expected_type { expected_type }
    {
        COWEL_ASSERT(expected_type);
    }

    [[nodiscard]]
    Processing_Status match_value(
        const ast::Member_Value& argument,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    ) override;
};

template <const auto& type>
    requires requires { Value_Of_Type_Matcher(&type); }
struct Value_Of_Constant_Type_Matcher final : Value_Of_Type_Matcher {

    [[nodiscard]]
    explicit Value_Of_Constant_Type_Matcher()
        : Value_Of_Type_Matcher(&type)
    {
    }
};

/// @brief Matches any spliceable value and splices it into a string.
struct Spliceable_To_String_Matcher final : Value_Matcher, Value_Holder<std::u8string_view> {
private:
    std::pmr::vector<char8_t> m_data;

public:
    [[nodiscard]]
    explicit Spliceable_To_String_Matcher(std::pmr::memory_resource* memory)
        : m_data { memory }
    {
    }

    [[nodiscard]]
    Processing_Status match_value(
        const ast::Member_Value& argument,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    ) override;
};

struct String_Matcher final : Value_Matcher, Value_Holder<std::u8string_view> {
private:
    std::pmr::vector<char8_t> m_data;

public:
    [[nodiscard]]
    explicit String_Matcher(std::pmr::memory_resource* memory)
        : m_data { memory }
    {
    }

    [[nodiscard]]
    Processing_Status match_value(
        const ast::Member_Value& argument,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    ) override;
};

struct Boolean_Matcher final : Value_Matcher, Value_Holder<bool> {

    [[nodiscard]]
    explicit Boolean_Matcher()
        = default;

    [[nodiscard]]
    Processing_Status match_value(
        const ast::Member_Value& argument,
        Frame_Index frame,
        Context&,
        const Match_Fail_Options& on_fail
    ) override;
};

struct Integer_Matcher final : Value_Matcher, Value_Holder<Integer> {

    [[nodiscard]]
    explicit Integer_Matcher()
        = default;

    [[nodiscard]]
    Processing_Status match_value(
        const ast::Member_Value& argument,
        Frame_Index frame,
        Context&,
        const Match_Fail_Options& on_fail
    ) override;
};

/// @brief Matches one of the given constant (sorted) string options.
struct Sorted_Options_Matcher final : Textual_Matcher {
private:
    std::span<const std::u8string_view> m_options;
    std::ptrdiff_t m_index = -1;

public:
    [[nodiscard]]
    explicit Sorted_Options_Matcher(std::span<const std::u8string_view> options)
        : m_options { options }
    {
        COWEL_DEBUG_ASSERT(std::ranges::is_sorted(options));
    }

    [[nodiscard]]
    bool match_string(
        const ast::Member_Value& argument,
        std::u8string_view str,
        Context&,
        Fail_Callback on_fail
    ) override;

    [[nodiscard]]
    bool was_matched() const override
    {
        return m_index >= 0;
    }

    void reset() noexcept override
    {
        m_index = -1;
    }

    [[nodiscard]]
    std::u8string_view get_or_default(std::u8string_view fallback) const
    {
        return m_index < 0 ? fallback : m_options[std::size_t(m_index)];
    }

    [[nodiscard]]
    std::size_t get_index_or_default(std::size_t fallback) const
    {
        return m_index < 0 ? fallback : std::size_t(m_index);
    }
};

// GROUP MEMBER ====================================================================================

struct Group_Member_Matcher {
private:
    std::u8string_view m_name;
    Optionality m_optionality;
    Value_Matcher& m_value_matcher;

public:
    [[nodiscard]]
    constexpr explicit Group_Member_Matcher(
        std::u8string_view name,
        Optionality optionality,
        Value_Matcher& value_matcher
    )
        : m_name { name }
        , m_optionality { optionality }
        , m_value_matcher { value_matcher }
    {
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
};

// PACK ============================================================================================

struct Pack_Matcher {
public:
    [[nodiscard]]
    explicit Pack_Matcher()
        = default;

    [[nodiscard]]
    virtual Processing_Status match_pack(
        std::span<ast::Group_Member const> members,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    ) = 0;
};

struct Pack_Usual_Matcher final : Pack_Matcher {
private:
    std::span<Group_Member_Matcher* const> m_member_matchers;

public:
    [[nodiscard]]
    explicit Pack_Usual_Matcher(std::span<Group_Member_Matcher* const> member_matchers)
        : m_member_matchers { member_matchers }
    {
        if constexpr (is_debug_build) {
            for (const auto* const matcher : member_matchers) {
                COWEL_ASSERT(matcher != nullptr);
            }
        }
    }

    [[nodiscard]]
    Processing_Status match_pack(
        std::span<ast::Group_Member const> members,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    ) override;

private:
    [[nodiscard]]
    Processing_Status do_match(
        std::span<const ast::Group_Member> members,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail,
        std::span<int> argument_indices_by_parameter,
        std::size_t cumulative_arg_index
    );
};

struct Empty_Pack_Matcher final : Pack_Matcher {
public:
    [[nodiscard]]
    explicit Empty_Pack_Matcher()
        = default;

    [[nodiscard]]
    Processing_Status match_pack(
        std::span<ast::Group_Member const> members,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    ) override;
};

// GROUP ===========================================================================================

struct Group_Matcher : Value_Matcher {
    [[nodiscard]]
    explicit Group_Matcher()
        = default;

    [[nodiscard]]
    Processing_Status match_value(
        const ast::Member_Value& argument,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    ) override;

    /// @brief Matches a group.
    /// @param group The group, or a null pointer in the event of an artificial empty group
    /// such as the one in a directive invocation with no group.
    [[nodiscard]]
    virtual Processing_Status match_group(
        const ast::Primary* group,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    ) = 0;
};

struct Group_Pack_Lazy_Any_Matcher final : Group_Matcher {
private:
    const ast::Primary* m_group = nullptr;
    Frame_Index m_group_frame;

public:
    [[nodiscard]]
    explicit Group_Pack_Lazy_Any_Matcher()
        = default;

    [[nodiscard]]
    bool was_matched() const override
    {
        return m_group != nullptr;
    }

    void reset() noexcept override
    {
        m_group = nullptr;
    }

    [[nodiscard]]
    const ast::Primary& get() const
    {
        COWEL_ASSERT(was_matched());
        return *m_group;
    }

    [[nodiscard]]
    Frame_Index get_frame() const
    {
        COWEL_ASSERT(was_matched());
        return m_group_frame;
    }

    [[nodiscard]]
    Processing_Status match_group(
        const ast::Primary* group,
        Frame_Index frame,
        Context&,
        const Match_Fail_Options& //
    ) override
    {
        COWEL_ASSERT(!group || group->get_kind() == ast::Primary_Kind::group);

        m_group = group;
        m_group_frame = frame;
        return Processing_Status::ok;
    }
};

struct Group_Pack_Named_Lazy_Any_Matcher : Group_Matcher {
    using Filter = bool(const ast::Group_Member&);

private:
    const ast::Primary* m_group = nullptr;
    Frame_Index m_group_frame;
    Filter* m_filter = nullptr;

public:
    [[nodiscard]]
    explicit Group_Pack_Named_Lazy_Any_Matcher()
        = default;

    [[nodiscard]]
    explicit Group_Pack_Named_Lazy_Any_Matcher(Filter* filter) noexcept
        : m_filter { filter }
    {
    }

    [[nodiscard]]
    bool was_matched() const override
    {
        return m_group != nullptr;
    }

    void reset() noexcept override
    {
        m_group = nullptr;
    }

    [[nodiscard]]
    const ast::Primary& get() const
    {
        COWEL_ASSERT(m_group);
        return *m_group;
    }

    [[nodiscard]]
    Frame_Index get_frame() const
    {
        COWEL_ASSERT(was_matched());
        return m_group_frame;
    }

    [[nodiscard]]
    Processing_Status match_group(
        const ast::Primary* group,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    ) override
    {
        COWEL_ASSERT(!group || group->get_kind() == ast::Primary_Kind::group);

        m_group = group;
        m_group_frame = frame;

        if (!group) {
            return Processing_Status::ok;
        }
        return match_pack(group->get_members(), frame, context, on_fail);
    }

private:
    [[nodiscard]]
    Processing_Status match_pack(
        std::span<const ast::Group_Member> members,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    );
};

[[nodiscard]]
inline bool is_html_attribute_convertible(const ast::Group_Member& member)
{
    return member.get_kind() == ast::Member_Kind::named //
        && member.has_value() //
        && member.get_value().is_spliceable();
}

/// @brief A `Group_Pack_Named_Lazy_Any_Matcher` which uses
/// `is_html_attribute_convertible` as a filter.
struct Group_Pack_Named_Lazy_Spliceable_Matcher : Group_Pack_Named_Lazy_Any_Matcher {
    [[nodiscard]]
    explicit Group_Pack_Named_Lazy_Spliceable_Matcher() noexcept
        : Group_Pack_Named_Lazy_Any_Matcher(&is_html_attribute_convertible)
    {
    }
};

struct Group_Pack_Value_Matcher : Group_Matcher {
private:
    bool m_matched = false;
    std::pmr::vector<Value_And_Location<Value>> m_values;

public:
    [[nodiscard]]
    explicit Group_Pack_Value_Matcher(std::pmr::memory_resource* memory)
        : m_values { memory }
    {
    }

    [[nodiscard]]
    bool was_matched() const override
    {
        return m_matched;
    }

    void reset() noexcept override
    {
        m_matched = false;
    }

    [[nodiscard]]
    std::span<const Value_And_Location<Value>> get_values() const
    {
        COWEL_ASSERT(m_matched);
        return m_values;
    }

    [[nodiscard]]
    Processing_Status match_group(
        const ast::Primary* group,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    ) override
    {
        COWEL_ASSERT(!group || group->get_kind() == ast::Primary_Kind::group);
        COWEL_DEBUG_ASSERT(status_is_error(on_fail.status));

        const auto members = [&] -> std::span<const ast::Group_Member> {
            if (group) {
                return group->get_members();
            }
            return {};
        }();
        const Processing_Status result = match_pack(members, frame, context, on_fail);
        if (result == Processing_Status::ok) {
            m_matched = true;
        }
        return result;
    }

private:
    [[nodiscard]]
    Processing_Status match_pack(
        std::span<const ast::Group_Member> members,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    );
};

struct Group_Pack_Matcher final : Group_Matcher {
private:
    Pack_Matcher& m_pack_matcher;
    const ast::Primary* m_group = nullptr;

public:
    [[nodiscard]]
    explicit Group_Pack_Matcher(Pack_Matcher& pack_matcher)
        : m_pack_matcher { pack_matcher }
    {
    }

    [[nodiscard]]
    bool was_matched() const override
    {
        return m_group != nullptr;
    }

    void reset() noexcept override
    {
        m_group = nullptr;
    }

    [[nodiscard]]
    Processing_Status match_group(
        const ast::Primary* group,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    ) override
    {
        const auto members = [&] -> std::span<const ast::Group_Member> {
            if (group) {
                return group->get_members();
            }
            return {};
        }();
        return m_pack_matcher.match_pack(members, frame, context, on_fail);
    }
};

// CALL ============================================================================================

struct Call_Matcher {
private:
    Group_Matcher& m_group_matcher;

public:
    [[nodiscard]]
    explicit Call_Matcher(Group_Matcher& args)
        : m_group_matcher { args }
    {
    }

    [[nodiscard]]
    Processing_Status match_call(
        const Invocation& call,
        Context& context,
        Fail_Callback on_fail,
        Processing_Status on_fail_status = Processing_Status::error
    )
    {
        return match_group(
            call.arguments, call.content_frame, context,
            Match_Fail_Options {
                .emit = on_fail,
                .status = on_fail_status,
                .location = call.get_arguments_source_span(),
            }
        );
    }

private:
    [[nodiscard]]
    Processing_Status match_group(
        const ast::Primary* group,
        Frame_Index frame,
        Context& context,
        const Match_Fail_Options& on_fail
    )
    {
        COWEL_ASSERT(!group || group->get_kind() == ast::Primary_Kind::group);
        return m_group_matcher.match_group(group, frame, context, on_fail);
    }
};

} // namespace cowel

#endif
