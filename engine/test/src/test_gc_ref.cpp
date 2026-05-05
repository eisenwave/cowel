#include <cstddef>
#include <span>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "cowel/gc.hpp"

namespace cowel {
namespace {

// =============================================================================
// Helper: destructor-tracking type
// =============================================================================

/// @brief A type that increments `live` on construction and decrements on destruction.
/// This lets tests verify that GC_Ref properly manages object lifetime.
struct Tracked {
    int* live;
    int id;

    explicit Tracked(int& live_counter, int id = 0)
        : live { &live_counter }
        , id { id }
    {
        ++live_counter;
    }

    Tracked(const Tracked&) = delete;
    Tracked& operator=(const Tracked&) = delete;

    ~Tracked()
    {
        --(*live);
    }
};

// =============================================================================
// GC_Ref — null / default construction
// =============================================================================

TEST(GC_Ref, default_is_null)
{
    const GC_Ref<int> ref;
    EXPECT_FALSE(ref);
    EXPECT_EQ(ref.unsafe_get_node(), nullptr);
}

// =============================================================================
// GC_Ref — gc_ref_make and basic access
// =============================================================================

TEST(GC_Ref, make_int_access)
{
    const GC_Ref<int> ref = gc_ref_make<int>(42);
    ASSERT_TRUE(ref);
    EXPECT_EQ(*ref, 42);
    EXPECT_EQ(ref.unsafe_get_node()->reference_count, 1u);
}

TEST(GC_Ref, make_and_dereference_arrow)
{
    struct Foo {
        int x;
    };
    const GC_Ref<Foo> ref = gc_ref_make<Foo>(Foo { 99 });
    ASSERT_TRUE(ref);
    EXPECT_EQ(ref->x, 99);
}

TEST(GC_Ref, destructor_called_when_last_ref_drops)
{
    int live = 0;
    {
        const GC_Ref<Tracked> ref = gc_ref_make<Tracked>(live, 1);
        EXPECT_EQ(live, 1); // constructed
    }
    // Destructor should have been called when ref went out of scope.
    EXPECT_EQ(live, 0);
}

// =============================================================================
// GC_Ref — copy semantics and reference counting
// =============================================================================

TEST(GC_Ref, copy_increments_refcount)
{
    int live = 0;
    const GC_Ref<Tracked> ref1 = gc_ref_make<Tracked>(live, 1);
    ASSERT_EQ(ref1.unsafe_get_node()->reference_count, 1u);

    const GC_Ref<Tracked> ref2 = ref1; // NOLINT
    EXPECT_EQ(ref1.unsafe_get_node()->reference_count, 2u);
    EXPECT_EQ(ref2.unsafe_get_node(), ref1.unsafe_get_node()); // same node
    EXPECT_EQ(live, 1); // only one object
}

TEST(GC_Ref, copy_destructor_only_when_last_drops)
{
    int live = 0;
    {
        GC_Ref<Tracked> ref1 = gc_ref_make<Tracked>(live, 1);
        {
            const GC_Ref<Tracked> ref2 = ref1; // NOLINT
            EXPECT_EQ(live, 1);
            // ref2 goes out of scope — refcount drops to 1, object survives
        }
        EXPECT_EQ(live, 1);
    }
    // ref1 goes out of scope — object is destroyed
    EXPECT_EQ(live, 0);
}

TEST(GC_Ref, three_copies_sequential_drop)
{
    int live = 0;
    {
        GC_Ref<Tracked> a = gc_ref_make<Tracked>(live, 0);
        GC_Ref<Tracked> b = a; // NOLINT
        GC_Ref<Tracked> c = b; // NOLINT
        EXPECT_EQ(a.unsafe_get_node()->reference_count, 3u);
        EXPECT_EQ(live, 1);
    }
    EXPECT_EQ(live, 0);
}

// =============================================================================
// GC_Ref — move semantics
// =============================================================================

TEST(GC_Ref, move_transfers_ownership)
{
    int live = 0;
    GC_Ref<Tracked> ref1 = gc_ref_make<Tracked>(live, 1);
    GC_Node* const node = ref1.unsafe_get_node();

    GC_Ref<Tracked> ref2 = std::move(ref1);

    EXPECT_FALSE(ref1); // source is null after move
    EXPECT_TRUE(ref2);
    EXPECT_EQ(ref2.unsafe_get_node(), node); // same node
    EXPECT_EQ(node->reference_count, 1u); // refcount unchanged
    EXPECT_EQ(live, 1);
}

TEST(GC_Ref, move_no_double_destroy)
{
    int live = 0;
    {
        GC_Ref<Tracked> ref1 = gc_ref_make<Tracked>(live, 1);
        GC_Ref<Tracked> ref2 = std::move(ref1);
        EXPECT_EQ(live, 1);
    }
    EXPECT_EQ(live, 0); // destroyed exactly once
}

// =============================================================================
// GC_Ref — copy assignment
// =============================================================================

TEST(GC_Ref, copy_assign_to_null)
{
    int live = 0;
    GC_Ref<Tracked> ref1 = gc_ref_make<Tracked>(live, 1);
    GC_Ref<Tracked> ref2;
    ref2 = ref1;
    EXPECT_TRUE(ref2);
    EXPECT_EQ(ref2.unsafe_get_node(), ref1.unsafe_get_node());
    EXPECT_EQ(ref1.unsafe_get_node()->reference_count, 2u);
}

TEST(GC_Ref, copy_assign_to_existing_drops_old)
{
    int live1 = 0;
    int live2 = 0;
    GC_Ref<Tracked> ref1 = gc_ref_make<Tracked>(live1, 1);
    GC_Ref<Tracked> ref2 = gc_ref_make<Tracked>(live2, 2);

    // ref2 currently holds live2's object
    EXPECT_EQ(live2, 1);

    ref2 = ref1; // should drop ref2's old object

    EXPECT_EQ(live2, 0); // old object in ref2 was destroyed
    EXPECT_EQ(live1, 1); // ref1's object is still alive
    EXPECT_EQ(ref2.unsafe_get_node(), ref1.unsafe_get_node());
    EXPECT_EQ(ref1.unsafe_get_node()->reference_count, 2u);
}

TEST(GC_Ref, copy_assign_self)
{
    int live = 0;
    GC_Ref<Tracked> ref = gc_ref_make<Tracked>(live, 1);
    // NOLINTNEXTLINE(clang-diagnostic-self-assign-overloaded)
    ref = ref;
    EXPECT_TRUE(ref);
    EXPECT_EQ(live, 1);
    EXPECT_EQ(ref.unsafe_get_node()->reference_count, 1u);
}

// =============================================================================
// GC_Ref — move assignment
// =============================================================================

TEST(GC_Ref, move_assign_drops_old)
{
    int live1 = 0;
    int live2 = 0;
    GC_Ref<Tracked> ref1 = gc_ref_make<Tracked>(live1, 1);
    GC_Ref<Tracked> ref2 = gc_ref_make<Tracked>(live2, 2);

    ref2 = std::move(ref1);

    EXPECT_EQ(live2, 0); // ref2's old object was destroyed
    EXPECT_EQ(live1, 1); // ref1's object transferred to ref2
    EXPECT_FALSE(ref1);
    EXPECT_TRUE(ref2);
}

// =============================================================================
// GC_Ref — reset
// =============================================================================

TEST(GC_Ref, reset_drops_last_reference)
{
    int live = 0;
    GC_Ref<Tracked> ref = gc_ref_make<Tracked>(live, 1);
    EXPECT_EQ(live, 1);
    ref.reset();
    EXPECT_FALSE(ref);
    EXPECT_EQ(live, 0);
}

TEST(GC_Ref, reset_keeps_other_references)
{
    int live = 0;
    GC_Ref<Tracked> ref1 = gc_ref_make<Tracked>(live, 1);
    GC_Ref<Tracked> ref2 = ref1;
    ref1.reset();
    EXPECT_FALSE(ref1);
    EXPECT_TRUE(ref2);
    EXPECT_EQ(live, 1);
    EXPECT_EQ(ref2.unsafe_get_node()->reference_count, 1u);
}

TEST(GC_Ref, reset_null_is_noop)
{
    GC_Ref<int> ref;
    ref.reset(); // must not crash
    EXPECT_FALSE(ref);
}

// =============================================================================
// GC_Ref — operator[]  and as_span via gc_ref_from_range
// =============================================================================

TEST(GC_Ref, from_range_access)
{
    const std::vector<int> arr = { 10, 20, 30 };
    const GC_Ref<int> ref = gc_ref_from_range<int>(arr);
    ASSERT_TRUE(ref);
    EXPECT_EQ(ref[0], 10);
    EXPECT_EQ(ref[1], 20);
    EXPECT_EQ(ref[2], 30);
}

TEST(GC_Ref, as_span)
{
    const std::vector<int> arr = { 1, 2, 3, 4, 5 };
    const GC_Ref<int> ref = gc_ref_from_range<int>(arr);
    const std::span<const int> sp = ref.as_span();
    ASSERT_EQ(sp.size(), 5u);
    for (std::size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(sp[i], i + 1);
    }
}

TEST(GC_Ref, from_range_destructor_tracking)
{
    int live = 0;
    {
        // Build a small array of Tracked objects using a custom range adaptor.
        // We allocate and track two objects.
        struct TrackedFactory {
            int* live;
            int n;

            struct Iterator {
                int* live;
                int i;
                int n;

                bool operator!=(const Iterator& o) const
                {
                    return i != o.i;
                }
                Iterator& operator++()
                {
                    ++i;
                    return *this;
                }
                Tracked operator*() const
                {
                    return Tracked { *live, i };
                }
            };

            [[nodiscard]]
            std::size_t size() const
            {
                return std::size_t(n);
            }
            [[nodiscard]]
            Iterator begin() const
            {
                return { live, 0, n };
            }
            [[nodiscard]]
            Iterator end() const
            {
                return { live, n, n };
            }
        };

        TrackedFactory factory { &live, 3 };
        // gc_ref_from_range copies each element via the dereference operator,
        // so 3 Tracked objects will be constructed inside the GC allocation.
        GC_Ref<Tracked> ref = gc_ref_from_range<Tracked>(factory);
        EXPECT_EQ(live, 3);
        EXPECT_EQ(ref[0].id, 0);
        EXPECT_EQ(ref[1].id, 1);
        EXPECT_EQ(ref[2].id, 2);
    }
    // All 3 should be destroyed.
    EXPECT_EQ(live, 0);
}

// =============================================================================
// GC_Ref — const conversion
// =============================================================================

TEST(GC_Ref, const_conversion_from_nonconst)
{
    int live = 0;
    GC_Ref<Tracked> ref = gc_ref_make<Tracked>(live, 7);
    GC_Ref<const Tracked> cref = std::move(ref);
    EXPECT_FALSE(ref);
    EXPECT_TRUE(cref);
    EXPECT_EQ(cref->id, 7);
    EXPECT_EQ(live, 1);
}

// =============================================================================
// GC_Ref — unsafe_release_node
// =============================================================================

TEST(GC_Ref, release_node_leaves_null)
{
    int live = 0;
    GC_Ref<Tracked> ref = gc_ref_make<Tracked>(live, 1);
    GC_Node* node = ref.unsafe_release_node();
    EXPECT_FALSE(ref);
    EXPECT_NE(node, nullptr);
    EXPECT_EQ(live, 1); // object still alive — we own the node now

    // Manually drop the reference to avoid leak.
    node->drop_reference();
    EXPECT_EQ(live, 0);
}

// =============================================================================
// GC_Ref — trivially-destructible type (destructor field is null)
// =============================================================================

TEST(GC_Ref, trivially_destructible)
{
    const GC_Ref<int> ref = gc_ref_make<int>(123);
    ASSERT_TRUE(ref);
    // The destructor field should be null for trivially-destructible types.
    EXPECT_FALSE(ref.unsafe_get_node()->destructor);
    EXPECT_EQ(*ref, 123);
}

// =============================================================================
// GC_Ref — GC_Node::add_reference skips when count is 0 (immortal / static node)
// =============================================================================

TEST(GC_Node, add_reference_skips_when_zero)
{
    // A node with reference_count == 0 represents a "static" or immortal allocation.
    // add_reference must not increment it.
    GC_Node node {
        .reference_count = 0,
        .extent = 0,
        .allocation_size = 0,
        .allocation_alignment = 0,
        .destructor = {},
    };
    node.add_reference();
    EXPECT_EQ(node.reference_count, 0u);
}

} // namespace
} // namespace cowel
