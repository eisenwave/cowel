#ifndef COWEL_DIRECTIVE_BEHAVIOR_HPP
#define COWEL_DIRECTIVE_BEHAVIOR_HPP

#include "cowel/util/result.hpp"

#include "cowel/content_status.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"
#include "cowel/tooltip.hpp"
#include "cowel/type.hpp"
#include "cowel/value.hpp"

namespace cowel {

enum struct Evaluation_Result_Kind : Default_Underlying {
    /// @brief Evaluation yielded a value.
    value,
    /// @brief Evaluation results in a block;
    /// blocks are evaluated lazily,
    /// so nothing actually happens.
    block,
};

// using Value_Variant = std::variant<std::monostate, >;

/// @brief Implements behavior that one or multiple directives should have.
struct Directive_Behavior {
private:
    Type m_static_type;
    Tooltip_Article m_tooltip_article {};

public:
    [[nodiscard]]
    constexpr explicit Directive_Behavior(const Type& static_type) noexcept
        : m_static_type { static_type }
    {
    }
    [[nodiscard]]
    constexpr explicit Directive_Behavior(Type&& static_type) noexcept
        : m_static_type { std::move(static_type) }
    {
    }
    [[nodiscard]]
    constexpr Directive_Behavior(
        const Type& static_type,
        const Tooltip_Article& tooltip_article
    ) noexcept
        : m_static_type { static_type }
        , m_tooltip_article { tooltip_article }
    {
    }
    [[nodiscard]]
    constexpr Directive_Behavior(
        Type&& static_type,
        const Tooltip_Article& tooltip_article
    ) noexcept
        : m_static_type { std::move(static_type) }
        , m_tooltip_article { tooltip_article }
    {
    }

    /// @brief Evaluates the directive in a scripting context.
    /// That is, when providing the directive as an argument to another directive, etc.
    /// Evaluation does not result in splicing, so blocks are not processed.
    /// However, directives that return boolean/integer values and such things
    /// are processed by this.
    [[nodiscard]]
    virtual Result<Value, Processing_Status> evaluate(const Invocation&, Context&) const
        = 0;

    [[nodiscard]]
    virtual Processing_Status splice(Content_Policy& out, const Invocation&, Context&) const;

    /// @brief Returns the static type of this behavior,
    /// or `nullptr` if the type is dynamic.
    /// Most builtin directives have a static type;
    /// exceptions include `\cowel_put` or anything else that fetches values of varying type.
    [[nodiscard]]
    const Type& get_static_type() const
    {
        return m_static_type;
    }

    /// @brief Returns the structured tooltip article for this behavior.
    /// The subject is empty when no article is defined.
    [[nodiscard]]
    constexpr const Tooltip_Article& get_tooltip_article() const noexcept
    {
        return m_tooltip_article;
    }

protected:
    /// @brief Updates the stored tooltip article.
    /// Intended for use by derived classes that own the article strings
    /// and must set the article after their owning members are initialized.
    void set_tooltip_article(const Tooltip_Article& article) noexcept
    {
        m_tooltip_article = article;
    }
};

/// @brief The behavior of directives that produce no output when spliced
/// and which return no value in a scripting context.
/// For example, `\cowel_alias`.
///
/// The static type is `Type::void`.
struct Unit_Directive_Behavior : Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Unit_Directive_Behavior(const Tooltip_Article& tooltip_article = {}) noexcept
        : Directive_Behavior { Type::unit, tooltip_article }
    {
    }

    [[nodiscard]]
    Result<Value, Processing_Status> evaluate(const Invocation& call, Context& context) const final
    {
        const Processing_Status result = do_evaluate(call, context);
        if (result != Processing_Status::ok) {
            return result;
        }
        return Value::unit;
    }

    [[nodiscard]]
    Processing_Status splice(Content_Policy&, const Invocation& call, Context& context) const final
    {
        return do_evaluate(call, context);
    }

protected:
    [[nodiscard]]
    virtual Processing_Status do_evaluate(const Invocation&, Context&) const
        = 0;
};

struct Bool_Directive_Behavior : Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Bool_Directive_Behavior(const Tooltip_Article& tooltip_article = {}) noexcept
        : Directive_Behavior { Type::boolean, tooltip_article }
    {
    }

    [[nodiscard]]
    Result<Value, Processing_Status> evaluate(const Invocation& call, Context& context) const final;
    [[nodiscard]]
    Processing_Status
    splice(Content_Policy& out, const Invocation& call, Context& context) const final;

protected:
    [[nodiscard]]
    virtual Result<bool, Processing_Status>
    do_evaluate(const Invocation& call, Context& context) const = 0;
};

struct Int_Directive_Behavior : Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Int_Directive_Behavior(const Tooltip_Article& tooltip_article = {}) noexcept
        : Directive_Behavior { Type::integer, tooltip_article }
    {
    }

    [[nodiscard]]
    Result<Value, Processing_Status> evaluate(const Invocation& call, Context& context) const final;
    [[nodiscard]]
    Processing_Status
    splice(Content_Policy& out, const Invocation& call, Context& context) const final;

protected:
    [[nodiscard]]
    virtual Result<Big_Int, Processing_Status>
    do_evaluate(const Invocation& call, Context& context) const = 0;
};

struct Float_Directive_Behavior : Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Float_Directive_Behavior(
        const Tooltip_Article& tooltip_article = {}
    ) noexcept
        : Directive_Behavior { Type::floating, tooltip_article }
    {
    }

    [[nodiscard]]
    Result<Value, Processing_Status> evaluate(const Invocation& call, Context& context) const final;
    [[nodiscard]]
    Processing_Status
    splice(Content_Policy& out, const Invocation& call, Context& context) const final;

protected:
    [[nodiscard]]
    virtual Result<Float, Processing_Status>
    do_evaluate(const Invocation& call, Context& context) const = 0;
};

/// @brief The behavior of a directive that returns a value of type `str`
/// which is guaranteed to fit into a `Short_String_Value`.
///
/// For example, `cowel_char_by_num` yields a single code point as `str`,
/// so it is at most 4 code units long.
/// This may be slightly more efficient than a plain `Directive_Behavior`
/// because splicing bypasses the creation of a `Value` object.
struct Short_String_Directive_Behavior : Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Short_String_Directive_Behavior(
        const Tooltip_Article& tooltip_article = {}
    ) noexcept
        : Directive_Behavior { Type::str, tooltip_article }
    {
    }

    [[nodiscard]]
    Result<Value, Processing_Status> evaluate(const Invocation& call, Context& context) const final;
    [[nodiscard]]
    Processing_Status
    splice(Content_Policy& out, const Invocation& call, Context& context) const final;

protected:
    [[nodiscard]]
    virtual Result<Short_String_Value, Processing_Status>
    do_evaluate(const Invocation& call, Context& context) const = 0;
};

/// @brief The behavior of a directive that returns a value of type `block`.
/// This is the behavior of macros, many legacy builtin directives, etc.
///
/// The static type is `Type::block`.
struct Block_Directive_Behavior : Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Block_Directive_Behavior(
        const Tooltip_Article& tooltip_article = {}
    ) noexcept
        : Directive_Behavior { Type::block, tooltip_article }
    {
    }

    [[nodiscard]]
    Result<Value, Processing_Status> evaluate(const Invocation& call, Context& context) const final;
};

} // namespace cowel

#endif
