#ifndef COWEL_CALL_STACK_HPP
#define COWEL_CALL_STACK_HPP

#include <memory_resource>
#include <span>
#include <vector>

#include "cowel/util/small_vector.hpp"

#include "cowel/directive_behavior.hpp"
#include "cowel/function_bindings.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"
#include "cowel/syntax/ast.hpp"
#include "cowel/value.hpp"

namespace cowel {

/// @brief The parameter bindings of a function call,
/// stored positionally: `values[i]` is the value bound to `parameters[i]`.
///
/// Unlike a name-keyed map, this representation does not allocate to store names,
/// and it preserves the declaration order of the parameters,
/// which is also the order in which arguments are matched.
struct Call_Bindings {
    /// @brief The parameters of the function which this frame was pushed for,
    /// in declaration order.
    /// This is empty unless the frame belongs to a function call.
    std::span<const ast::Parameter> parameters {};
    /// @brief The values bound to each parameter,
    /// positionally aligned with `parameters`.
    /// This is empty unless the frame belongs to a function call.
    Small_Vector<Value, 8> values {};
};

struct Stack_Frame {
    /// @brief The behavior that was invoked, i.e. a directive, macro, or function.
    const Directive_Behavior& behavior;
    /// @brief The invocation that this frame was pushed for.
    Invocation invocation;
    /// @brief The function parameters bound by this frame.
    /// This is empty unless the frame belongs to a function call.
    /// Content in this frame also sees the bindings of the frame
    /// in which this frame's invocation appears.
    Call_Bindings bindings {};
};

struct Scoped_Frame;

struct Call_Stack {
public:
private:
    /// @brief The frames which are currently active, innermost last.
    ///
    /// A frame is owned jointly by the call stack and by every value which captured it,
    /// e.g. a block which was created in the frame.
    /// This is why popping a frame does not necessarily destroy it,
    /// and why the frames do not grow perpetually throughout a processing pass.
    std::pmr::vector<Frame_Ref> m_frames;

public:
    explicit Call_Stack(std::pmr::memory_resource* const memory)
        : m_frames { memory }
    {
        m_frames.reserve(8);
    }

    [[nodiscard]]
    Scoped_Frame push_scoped(Stack_Frame&& frame);

    /// @brief Returns the frames which are currently active, innermost last.
    [[nodiscard]]
    std::span<const Frame_Ref> get_active_frames() const noexcept
    {
        return m_frames;
    }

    [[nodiscard]]
    bool empty() const noexcept
    {
        return m_frames.empty();
    }

    /// @brief Returns the top stack frame.
    /// `empty()` shall be false.
    [[nodiscard]]
    const Stack_Frame& get_top() const
    {
        COWEL_ASSERT(!empty());
        return *m_frames.back();
    }

    friend Scoped_Frame;
};

struct Scoped_Frame {
private:
    Call_Stack& m_self;
    const Stack_Frame* m_frame {};

public:
    Scoped_Frame(Call_Stack& self, Stack_Frame frame)
        : m_self { self }
    {
        Frame_Ref reference = gc_ref_make<Stack_Frame>(std::move(frame));
        m_frame = &*reference;
        m_self.m_frames.push_back(std::move(reference));
    }

    Scoped_Frame(const Scoped_Frame&) = delete;
    Scoped_Frame& operator=(const Scoped_Frame&) = delete;

    ~Scoped_Frame()
    {
        COWEL_DEBUG_ASSERT(!m_self.m_frames.empty());
        COWEL_DEBUG_ASSERT(&*m_self.m_frames.back() == m_frame);
        // This only destroys the frame if nothing refers to it anymore,
        // i.e. if no block which was created in this frame is still alive.
        m_self.m_frames.pop_back();
    }

    const Stack_Frame& operator*() const
    {
        return *m_frame;
    }

    const Stack_Frame* operator->() const
    {
        return m_frame;
    }

    [[nodiscard]]
    Frame_Ref get_ref() const noexcept
    {
        return m_self.m_frames.back();
    }
};

inline Scoped_Frame Call_Stack::push_scoped(Stack_Frame&& frame)
{
    return { *this, std::move(frame) };
}

} // namespace cowel

#endif
