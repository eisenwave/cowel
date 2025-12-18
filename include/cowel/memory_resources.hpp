#ifndef COWEL_MEMORY_RESOURCES_HPP
#define COWEL_MEMORY_RESOURCES_HPP

#include <cstddef>
#include <memory_resource>

#include "cowel/util/assert.hpp"

#include "cowel/cowel.h"

namespace cowel {

[[noreturn]]
inline void allocation_failure()
{
#ifdef ULIGHT_EXCEPTIONS
    throw std::bad_alloc();
#else
    COWEL_ASSERT_UNREACHABLE(u8"Allocation failure.");
#endif
}

/// @brief A `pmr::memory_resource` constructed from `cowel_options`.
struct Pointer_Memory_Resource final : std::pmr::memory_resource {
private:
    cowel_alloc_fn* m_alloc;
    const void* m_alloc_data;
    cowel_free_fn* m_free;
    const void* m_free_data;

public:
    [[nodiscard]]
    explicit Pointer_Memory_Resource(
        cowel_alloc_fn* alloc,
        const void* alloc_data,
        cowel_free_fn* free,
        const void* free_data
    )
        : m_alloc { alloc }
        , m_alloc_data { alloc_data }
        , m_free { free }
        , m_free_data { free_data }
    {
    }

    [[nodiscard]]
    explicit Pointer_Memory_Resource(const cowel_options_u8& options)
        : Pointer_Memory_Resource { options.alloc, options.alloc_data, options.free,
                                    options.free_data }
    {
    }

    [[nodiscard]]
    void* do_allocate(std::size_t bytes, std::size_t alignment) final
    {
        void* const result = m_alloc(m_alloc_data, bytes, alignment);
        if (!result) {
            allocation_failure();
        }
        return result;
    }

    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) noexcept final
    {
        m_free(m_free_data, p, bytes, alignment);
    }

    [[nodiscard]]
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept final
    {
        const auto* const other_resource = dynamic_cast<const Pointer_Memory_Resource*>(&other);
        return other_resource && *this == *other_resource;
    }

    friend bool operator==(const Pointer_Memory_Resource&, const Pointer_Memory_Resource&)
        = default;
};

/// @brief A `pmr::memory_resource` which uses the functions
/// `cowel_alloc` and `cowel_free` for allocation and deallocation, respectively.
struct Global_Memory_Resource final : std::pmr::memory_resource {

    /// @brief Returns a pointer to an object of type `Global_Memory_Resource`
    /// with static duration.
    /// Note that all objects of this type are interchangeable,
    /// so `get()` is typically better than creating a new instance.
    [[nodiscard]]
    static Global_Memory_Resource* get() noexcept
    {
        static constinit Global_Memory_Resource instance;
        return &instance;
    }

    [[nodiscard]]
    void* do_allocate(std::size_t bytes, std::size_t alignment) final
    {
        void* const result = cowel_alloc(bytes, alignment);
        if (!result) {
            allocation_failure();
        }
        return result;
    }

    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) noexcept final
    {
        cowel_free(p, bytes, alignment);
    }

    [[nodiscard]]
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept final
    {
        return dynamic_cast<const Global_Memory_Resource*>(&other) != nullptr;
    }
};

/// @brief Like `std::pmr::polymorphic_allocator`,
/// but propagated whenever possible.
template <typename T>
struct Propagated_Polymorphic_Allocator {
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;

    using value_type = T;
    std::pmr::memory_resource* resource;

    [[nodiscard]]
    constexpr Propagated_Polymorphic_Allocator(
        std::pmr::memory_resource* resource = Global_Memory_Resource::get()
    ) noexcept
        : resource { resource }
    {
    }

    template <typename U>
        requires(!std::is_same_v<T, U>)
    Propagated_Polymorphic_Allocator(const Propagated_Polymorphic_Allocator<U>& other)
        : resource { other.resource }
    {
    }

    [[nodiscard]]
    T* allocate(std::size_t n)
    {
        return static_cast<T*>(resource->allocate(n * sizeof(T), alignof(T)));
    }

    void deallocate(T* p, std::size_t n)
    {
        resource->deallocate(p, n * sizeof(T), alignof(T));
    }

    friend bool
    operator==(const Propagated_Polymorphic_Allocator& x, const Propagated_Polymorphic_Allocator& y)
        = default;
};

} // namespace cowel

#endif
