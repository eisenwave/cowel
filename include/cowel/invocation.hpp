#ifndef COWEL_INVOCATION_HPP
#define COWEL_INVOCATION_HPP

#include <compare>
#include <cstddef>
#include <ranges>
#include <span>
#include <string_view>
#include <type_traits>

#include "cowel/util/assert.hpp"
#include "cowel/util/function_ref.hpp"
#include "cowel/util/meta.hpp"

#include "cowel/ast.hpp"
#include "cowel/fwd.hpp"

namespace cowel {

struct Argument_Ref {
    const ast::Argument& ast_node;
    Frame_Index frame_index;
};

struct Arguments_View_Iterator;

/// @brief A type-erased, non-owning random access range to arguments of an invocation.
/// Each of these arguments can have a different frame index.
struct Arguments_View {
    using Accessor_Fn = Argument_Ref(std::size_t);
    using Accessor = Function_Ref<Accessor_Fn>;
    using iterator = Arguments_View_Iterator;
    using const_iterator = iterator;

private:
    std::size_t m_size;
    Accessor m_accessor;

public:
    [[nodiscard]]
    Arguments_View(std::size_t length, Accessor accessor)
        : m_size { length }
        , m_accessor { accessor }
    {
        COWEL_ASSERT(accessor);
    }

    [[nodiscard]]
    std::size_t size() const noexcept
    {
        return m_size;
    }
    [[nodiscard]]
    std::size_t length() const noexcept
    {
        return m_size;
    }

    [[nodiscard]]
    bool empty() const noexcept
    {
        return m_size == 0;
    }

    [[nodiscard]]
    Argument_Ref operator[](std::size_t index) const
    {
        COWEL_ASSERT(index < m_size);
        return m_accessor(index);
    }

    [[nodiscard]]
    Argument_Ref front() const
    {
        COWEL_ASSERT(!empty());
        return m_accessor(0);
    }

    [[nodiscard]]
    Argument_Ref back() const
    {
        COWEL_ASSERT(!empty());
        return m_accessor(m_size - 1);
    }

    [[nodiscard]]
    iterator begin() const;
    [[nodiscard]]
    iterator cbegin() const;

    [[nodiscard]]
    iterator end() const;
    [[nodiscard]]
    iterator cend() const;
};

struct Arguments_View_Iterator {
    using value_type = Argument_Ref;
    using self_type = Arguments_View_Iterator;

private:
    const Arguments_View* m_self = nullptr;
    std::ptrdiff_t m_offset = 0;

public:
    [[nodiscard]]
    constexpr Arguments_View_Iterator(const Arguments_View& self, std::size_t index)
        : m_self { &self }
        , m_offset { std::ptrdiff_t(index) }
    {
    }

    [[nodiscard]]
    constexpr Arguments_View_Iterator()
        = default;

    [[nodiscard]]
    constexpr Argument_Ref operator*() const
    {
        return (*m_self)[std::size_t(m_offset)];
    }

    [[nodiscard]]
    constexpr Argument_Ref operator[](std::ptrdiff_t offset) const
    {
        return *(*this + offset);
    }

    constexpr self_type& operator++()
    {
        ++m_offset;
        return *this;
    }

    constexpr self_type operator++(int)
    {
        auto copy = *this;
        ++*this;
        return copy;
    }

    constexpr self_type& operator--()
    {
        --m_offset;
        return *this;
    }

    constexpr self_type operator--(int)
    {
        auto copy = *this;
        --*this;
        return copy;
    }

    [[nodiscard]]
    friend constexpr bool operator==(const self_type& x, const self_type& y)
    {
        COWEL_DEBUG_ASSERT(x.m_self == y.m_self);
        return x.m_offset == y.m_offset;
    }

    [[nodiscard]]
    friend constexpr std::strong_ordering operator<=>(const self_type& x, const self_type& y)
    {
        COWEL_DEBUG_ASSERT(x.m_self == y.m_self);
        return x.m_offset <=> y.m_offset;
    }

    constexpr self_type& operator+=(std::ptrdiff_t offset)
    {
        m_offset += offset;
        return *this;
    }

    constexpr self_type& operator-=(std::ptrdiff_t offset)
    {
        m_offset -= offset;
        return *this;
    }

    [[nodiscard]]
    friend constexpr self_type operator+(self_type x, std::ptrdiff_t offset)
    {
        x += offset;
        return x;
    }
    [[nodiscard]]
    friend constexpr self_type operator+(std::ptrdiff_t offset, self_type x)
    {
        x += offset;
        return x;
    }

    [[nodiscard]]
    friend constexpr self_type operator-(self_type x, std::ptrdiff_t offset)
    {
        x -= offset;
        return x;
    }

    [[nodiscard]]
    friend constexpr std::ptrdiff_t operator-(self_type x, self_type y)
    {
        return x.m_offset - y.m_offset;
    }
};

[[nodiscard]]
inline Arguments_View_Iterator Arguments_View::begin() const
{
    return { *this, 0 };
}
[[nodiscard]]
inline Arguments_View_Iterator Arguments_View::cbegin() const
{
    return { *this, 0 };
}
[[nodiscard]]
inline Arguments_View_Iterator Arguments_View::end() const
{
    return { *this, m_size };
}
[[nodiscard]]
inline Arguments_View_Iterator Arguments_View::cend() const
{
    return { *this, m_size };
}

static_assert(std::random_access_iterator<Arguments_View_Iterator>);
static_assert(std::ranges::random_access_range<Arguments_View>);
static_assert(std::is_trivially_copyable_v<Arguments_View>);

struct Invocation {
    /// @brief The name which names the invoked directive.
    /// In the case of e.g. `\x`, this is simply `x`,
    /// but in the case of `\cowel_invoke[x]`, it is `x`.
    std::u8string_view name;
    /// @brief The directive responsible for the invocation.
    /// This may not necessarily be a directive matching the behavior,
    /// but a directive like `\cowel_invoke` which performs that invocation programmatically.
    const ast::Directive& directive;
    /// @brief The arguments with which the directive is invoked.
    /// Note that this is not necessarily the same as `directive->get_arguments()`.
    Arguments_View arguments;
    /// @brief The content with which the directive is invoked.
    /// Note that this is not necessarily the same as `directive->get_content()`.
    std::span<const ast::Content> content;
    /// @brief The stack frame index of the content.
    /// For root content, this is zero.
    /// All content in a macro definition (and arguments of directives within)
    /// have the same frame index as that invocation.
    /// Intuitively, all visible content inside a macro has the same frame index,
    /// just like in a C++ function.
    Frame_Index content_frame;
    /// @brief The stack frame index of the invocation.
    /// This is always at least `1`
    /// because `0` indicates the document top level,
    /// with each level of invocation being one greater than the level below.
    Frame_Index call_frame;
};

/// @brief Simple helper type which can be constructed from an `ast::Directive`
/// and which can be converted to an `Arguments_View` which provides access
/// to the arguments of that directive.
struct Direct_Call_Arguments {
private:
    std::span<const ast::Argument> m_arguments;
    Frame_Index m_frame;

public:
    [[nodiscard]]
    Direct_Call_Arguments(const ast::Directive& d, Frame_Index frame) noexcept
        : m_arguments { d.get_arguments() }
        , m_frame { frame }
    {
    }

    [[nodiscard]] operator Arguments_View() const& noexcept
    {
        const Arguments_View::Accessor accessor
            = { const_v<[](const Direct_Call_Arguments* self, std::size_t i) -> Argument_Ref {
                    return { .ast_node = self->m_arguments[i], .frame_index = self->m_frame };
                }>,
                this };
        return { m_arguments.size(), accessor };
    }
};

/// @brief Creates a new `Invocation` object from a directive,
/// which is what we consider a "direct call".
/// @param d The directive which is invoked.
/// @param content_frame The frame index of the directive.
/// @param call_frame The frame index of the invocation.
/// @param args Arguments for this invocation.
[[nodiscard]]
inline Invocation make_invocation( //
    const ast::Directive& d,
    Frame_Index content_frame,
    Frame_Index call_frame,
    Arguments_View args
)
{
    return {
        .name = d.get_name(),
        .directive = d,
        .arguments = args,
        .content = d.get_content(),
        .content_frame = content_frame,
        .call_frame = call_frame,
    };
}

} // namespace cowel

#endif
