#ifndef COWEL_GC_HPP
#define COWEL_GC_HPP

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <new>
#include <span>
#include <utility>

#include "cowel/util/assert.hpp"
#include "cowel/util/function_ref.hpp"
#include "cowel/util/meta.hpp"

#include "cowel/memory_resources.hpp"
#include "cowel/settings.hpp"

namespace cowel {
namespace detail {

[[nodiscard]]
inline std::pmr::unsynchronized_pool_resource& get_gc() noexcept
{
    thread_local std::pmr::unsynchronized_pool_resource gc { Global_Memory_Resource::get() };
    return gc;
}

} // namespace detail

[[nodiscard]]
inline void* gc_alloc(std::size_t size, std::size_t alignment)
{
    return detail::get_gc().allocate(size, alignment);
}

inline void gc_free(void* p, std::size_t size, std::size_t alignment)
{
    detail::get_gc().deallocate(p, size, alignment);
}

using GC_Destructor = Function_Ref<void(void* address, std::size_t extent) noexcept>;

struct GC_Node {
    /// @brief The current reference count of this node.
    std::size_t reference_count;
    /// @brief The amount of elements in this node.
    /// For allocations of single objects, this is `1`,
    /// whereas for arrays it may be any amount.
    const std::size_t extent;
    /// @brief The total size of the allocation containing this node.
    const std::size_t allocation_size;
    /// @brief The alignment of the allocation containing this node.
    const std::size_t allocation_alignment;
    /// @brief The destructor for this allocation,
    /// invoked with `get_object_pointer()` and `extent` once the node is collected.
    const GC_Destructor destructor;

    [[nodiscard]]
    std::uintptr_t get_object_address() const
    {
        const auto result = reinterpret_cast<std::uintptr_t>(this + 1);
        const std::size_t misalignment = (result & (allocation_alignment - 1));
        return misalignment ? result + allocation_alignment - misalignment : result;
    }

    [[nodiscard]]
    void* get_object_pointer() const
    {
        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        return reinterpret_cast<void*>(get_object_address());
    }

    void destroy() noexcept
    {
        COWEL_ASSERT(reference_count == 0);
        auto* const p = get_object_pointer();
        if (destructor) {
            destructor(p, extent);
        }
        gc_free(p, allocation_size, allocation_alignment);
    }

    void add_reference() noexcept
    {
        if (reference_count) {
            reference_count += 1;
        }
    }

    void drop_reference()
    {
        COWEL_ASSERT(reference_count);
        if (--reference_count == 0) {
            destroy();
        }
    }
};

template <class T>
struct GC_Allocation {
    GC_Node node;
    alignas(T) unsigned char storage[sizeof(T)] {};
};

template <class T>
struct GC_Ref;

template <class T>
struct GC_Ref {
private:
    GC_Node* m_node = nullptr;

public:
    [[nodiscard]]
    constexpr GC_Ref()
        = default;

    [[nodiscard]]
    constexpr GC_Ref(GC_Node* node) noexcept
        : m_node { node }
    {
    }

    [[nodiscard]]
    constexpr GC_Ref(const GC_Ref& other) noexcept
        : m_node { other.m_node }
    {
        if (m_node) {
            m_node->add_reference();
        }
    }

    [[nodiscard]]
    constexpr GC_Ref(GC_Ref&& other) noexcept
        : m_node { std::exchange(other.m_node, nullptr) }
    {
    }

    constexpr ~GC_Ref()
    {
        reset();
    }

    constexpr GC_Ref& operator=(const GC_Ref& other) noexcept
    {
        if (this != &other) {
            reset();
            m_node = other.m_node;
            if (m_node) {
                m_node->add_reference();
            }
        }
        return *this;
    }

    constexpr GC_Ref& operator=(GC_Ref&& other) noexcept
    {
        reset();
        m_node = std::exchange(other.m_node, nullptr);
        return *this;
    }

    constexpr void reset()
    {
        if (m_node) {
            COWEL_DEBUG_ASSERT(m_node->allocation_alignment == alignof(T));
            m_node->drop_reference();
            m_node = nullptr;
        }
    }

    [[nodiscard]]
    T* operator->() const
    {
        COWEL_ASSERT(m_node);
        COWEL_DEBUG_ASSERT(m_node->allocation_alignment == alignof(T));
        const std::uintptr_t address = m_node->get_object_address();
        return std::launder(reinterpret_cast<T*>(address));
    }

    [[nodiscard]]
    T& operator*() const
    {
        return *operator->();
    }

    [[nodiscard]]
    T& operator[](const std::size_t i) const
    {
        COWEL_ASSERT(m_node);
        COWEL_ASSERT(i < m_node->extent);
        return operator->()[i];
    }

    [[nodiscard]]
    std::span<T> as_span() const
    {
        COWEL_ASSERT(m_node);
        COWEL_DEBUG_ASSERT(m_node->allocation_alignment == alignof(T));
        return { operator->(), m_node->extent };
    }

    [[nodiscard]]
    constexpr explicit operator bool() const
    {
        return m_node;
    }
};

namespace detail {

template <typename T>
void gc_do_destroy(void* p, std::size_t extent) noexcept
{
    auto* const object = static_cast<T*>(p);
    for (std::size_t i = 0; i < extent; ++i) {
        object[i].~T();
    }
}

template <typename T>
inline constexpr auto gc_destructor = std::is_trivially_destructible_v<T>
    ? GC_Destructor {}
    : GC_Destructor {
          Constant<[](void* p, std::size_t extent) noexcept { gc_do_destroy<T>(p, extent); }> {}
      };

} // namespace detail

template <typename T, class... Args>
    requires std::is_constructible_v<T, Args&&...>
[[nodiscard]]
GC_Ref<T> gc_ref_make(Args&&... args)
{
    using Allocation = GC_Allocation<T>;
    auto* result = static_cast<Allocation*>(gc_alloc(sizeof(Allocation), alignof(Allocation)));
    COWEL_ASSERT(result);

    result = new (result) Allocation { GC_Node {
        .reference_count = 1,
        .extent = 1,
        .allocation_size = sizeof(T),
        .allocation_alignment = alignof(T),
        .destructor = detail::gc_destructor<T>,
    } };
    if constexpr (is_debug_build) {
        const std::uintptr_t computed_address = result->node.get_object_address();
        const auto actual_address = reinterpret_cast<std::uintptr_t>(&result->storage);
        COWEL_ASSERT(computed_address == actual_address);
    }
    new (result->storage) T(std::forward<Args>(args)...);
    return &result->node;
}

template <typename T, class R>
    requires requires(const R& r) { r.size(); }
[[nodiscard]]
GC_Ref<T> gc_ref_from_range(const R& r)
{
    using Allocation = GC_Allocation<T>;
    const std::size_t extent = r.size();
    auto* result = static_cast<Allocation*>(
        gc_alloc(sizeof(Allocation) + (extent * sizeof(T)), alignof(Allocation))
    );
    COWEL_ASSERT(result);

    result = new (result) Allocation { GC_Node {
        .reference_count = 1,
        .extent = extent,
        .allocation_size = sizeof(T),
        .allocation_alignment = alignof(T),
        .destructor = detail::gc_destructor<T>,
    } };
    if constexpr (is_debug_build) {
        const std::uintptr_t computed_address = result->node.get_object_address();
        const auto actual_address = reinterpret_cast<std::uintptr_t>(&result->storage);
        COWEL_ASSERT(computed_address == actual_address);
    }
    auto it = r.begin();
    [[maybe_unused]]
    auto end
        = r.end();
    for (std::size_t i = 0; i < extent; void(++i), ++it) {
        COWEL_DEBUG_ASSERT(it != end);
        void* const address = result->storage + (i * sizeof(T));
        new (address) T(*it);
    }
    return &result->node;
}

} // namespace cowel

#endif
