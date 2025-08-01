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
        return dynamic_cast<const Pointer_Memory_Resource*>(&other) != nullptr;
    }
};

} // namespace cowel

#endif
