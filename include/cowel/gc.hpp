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
        // Ensure that the alignment is a power of two.
        // Anything else suggests memory errors.
        COWEL_DEBUG_ASSERT((allocation_alignment & (allocation_alignment - 1)) == 0);

        // It is possible that the object is more strictly aligned than the GC_Node.
        // That is, allocation_alignment > alignof(GC_Node).
        // In that case, the allocated storage may not immediately follow the GC_Node.
        // To find the object, we need to compute the next address
        // at which the object may be located.
        const auto result = reinterpret_cast<std::uintptr_t>(this + 1);
        const std::size_t misalignment = (result & (allocation_alignment - 1));
        const std::uintptr_t next_aligned_address
            = misalignment ? result + allocation_alignment - misalignment : result;
        return next_aligned_address;
    }

    [[nodiscard, gnu::always_inline]]
    void* get_object_pointer() const
    {
        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        return reinterpret_cast<void*>(get_object_address());
    }

    void destroy_and_free() noexcept
    {
        COWEL_ASSERT(reference_count == 0);
        auto* const p = get_object_pointer();
        if (destructor) {
            destructor(p, extent);
        }
        gc_free(this, allocation_size, allocation_alignment);
    }

    void add_reference() noexcept
    {
        if (reference_count) {
            reference_count += 1;
        }
    }

    /// @brief Decreases the reference count by one
    /// and returns the remaining reference count.
    /// The current reference count shall be at least one.
    std::size_t drop_reference()
    {
        COWEL_ASSERT(reference_count);
        const std::size_t remaining_references = --reference_count;
        if (remaining_references == 0) {
            destroy_and_free();
        }
        return remaining_references;
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

    /// @brief Claims ownership of an existing node without increasing its reference count.
    /// This constructor is mainly useful for allocating a `GC_Node` separately,
    /// then claiming ownership within a `GC_Ref` as a second stage.
    ///
    /// The constructor should not be used for adding ownership to a `GC_Node`
    /// which is already managed by other `GC_Ref`s.
    [[nodiscard]]
    constexpr explicit GC_Ref(GC_Node* const node) noexcept
        : m_node { node }
    {
        if constexpr (is_debug_build) {
            // To verify memory integrity, we run various plausibility checks for the node.
            COWEL_ASSERT(node->allocation_size >= sizeof(GC_Node));
            const std::size_t align = node->allocation_alignment;
            COWEL_ASSERT(align >= alignof(T));
            COWEL_ASSERT((align & (align - 1)) == 0);
        }
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
            COWEL_DEBUG_ASSERT(m_node->allocation_alignment >= alignof(T));
            m_node->drop_reference();
            m_node = nullptr;
        }
    }

    [[nodiscard]]
    T* operator->() const
    {
        COWEL_ASSERT(m_node);
        COWEL_DEBUG_ASSERT(m_node->allocation_alignment >= alignof(T));
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
        COWEL_DEBUG_ASSERT(m_node->allocation_alignment >= alignof(T));
        return { operator->(), m_node->extent };
    }

    [[nodiscard]]
    constexpr explicit operator bool() const
    {
        return m_node;
    }

    /// @brief Returns the pointer to the held `GC_Node`.
    /// This operation is unsafe because manual modification of the `GC_Node`
    /// (such as modifying the reference counter)
    /// may totally break garbage collection.
    [[nodiscard]]
    GC_Node* unsafe_get_node() const
    {
        return m_node;
    }

    /// @brief Returns the pointer to the held `GC_Node`
    /// and releases ownership of the node.
    [[nodiscard]]
    GC_Node* unsafe_release_node()
    {
        return std::exchange(m_node, nullptr);
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
        .allocation_size = sizeof(Allocation),
        .allocation_alignment = alignof(Allocation),
        .destructor = detail::gc_destructor<T>,
    } };
    if constexpr (is_debug_build) {
        const std::uintptr_t computed_address = result->node.get_object_address();
        const auto actual_address = reinterpret_cast<std::uintptr_t>(&result->storage);
        COWEL_ASSERT(computed_address == actual_address);
    }
    new (result->storage) T(std::forward<Args>(args)...);
    return GC_Ref<T> { &result->node };
}

template <typename T, class R>
    requires requires(const R& r) { r.size(); }
[[nodiscard]]
GC_Ref<T> gc_ref_from_range(const R& r)
{
    using Allocation = GC_Allocation<T>;
    const std::size_t extent = r.size();
    const std::size_t allocation_size = sizeof(Allocation) + (extent * sizeof(T));
    const std::size_t allocation_alignment = alignof(Allocation);
    auto* result = static_cast<Allocation*>(gc_alloc(allocation_size, allocation_alignment));
    COWEL_ASSERT(result);

    result = new (result) Allocation { GC_Node {
        .reference_count = 1,
        .extent = extent,
        .allocation_size = allocation_size,
        .allocation_alignment = allocation_alignment,
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
    return GC_Ref<T> { &result->node };
}

} // namespace cowel

#endif
