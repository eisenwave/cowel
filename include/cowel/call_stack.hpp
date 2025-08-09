#ifndef COWEL_CALL_STACK_HPP
#define COWEL_CALL_STACK_HPP

#include <cstddef>
#include <memory_resource>
#include <vector>

#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"

namespace cowel {

struct Stack_Frame {
    const Directive_Behavior& behavior;
    const Invocation& invocation;
};

struct Call_Stack {
public:
private:
    std::pmr::vector<Stack_Frame> m_frames;

public:
    explicit Call_Stack(std::pmr::memory_resource* memory)
        : m_frames { memory }
    {
    }

    [[nodiscard]]
    Scoped_Frame push_scoped(Stack_Frame frame);

    [[nodiscard]]
    const Stack_Frame& operator[](Frame_Index index) const
    {
        COWEL_DEBUG_ASSERT(std::size_t(index) <= m_frames.size());
        return m_frames[std::size_t(index)];
    }

    [[nodiscard]]
    bool empty() const noexcept
    {
        return m_frames.empty();
    }

    [[nodiscard]]
    std::size_t size() const noexcept
    {
        return m_frames.size();
    }

    /// @brief Returns the top stack frame.
    /// `empty()` shall be false.
    [[nodiscard]]
    const Stack_Frame& get_top() const
    {
        COWEL_ASSERT(!empty());
        return m_frames.back();
    }

    /// @brief Returns the index of the topmost frame, if any,
    /// or `Frame_Index::root` if the stack is empty.
    [[nodiscard]]
    Frame_Index get_top_index() const noexcept
    {
        return Frame_Index(int(m_frames.size()) - 1);
    }

    friend Scoped_Frame;
};

struct Scoped_Frame {
private:
    Call_Stack& m_self;
    Frame_Index m_index;

public:
    Scoped_Frame(Call_Stack& self, Stack_Frame frame)
        : m_self { self }
        , m_index { Frame_Index(m_self.m_frames.size()) }
    {
        m_self.m_frames.push_back(frame);
    }

    Scoped_Frame(const Scoped_Frame&) = delete;
    Scoped_Frame& operator=(const Scoped_Frame&) = delete;

    ~Scoped_Frame()
    {
        m_self.m_frames.pop_back();
    }

    const Stack_Frame& operator*() const
    {
        return m_self[m_index];
    }

    const Stack_Frame* operator->() const
    {
        return &m_self[m_index];
    }

    [[nodiscard]]
    Frame_Index get_index() const noexcept
    {
        return m_index;
    }
};

inline Scoped_Frame Call_Stack::push_scoped(Stack_Frame frame)
{
    return { *this, frame };
}

} // namespace cowel

#endif
