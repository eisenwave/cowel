#include <array>
#include <string>

#include <gtest/gtest.h>

#include "cowel/util/small_vector.hpp"

namespace cowel {
namespace {

int object_count = 0;

struct Counted {
    int value = 0;

    Counted() noexcept
    {
        ++object_count;
    }
    Counted(const int value) noexcept
        : value { value }
    {
        ++object_count;
    }
    Counted(const Counted& other) noexcept
        : value { other.value }
    {
        ++object_count;
    }
    Counted& operator=(const Counted&) noexcept = default;
    auto operator<=>(const Counted&) const noexcept = default;
    ~Counted()
    {
        --object_count;
    }
};

} // namespace

template struct Small_Vector<char, 1024>;
template struct Small_Vector<int, 1024>;
template struct Small_Vector<std::string, 16>;
template struct Small_Vector<Counted, 16>;

static_assert(std::ranges::contiguous_range<Small_Vector<Counted, 16>>);

namespace {

TEST(Small_Vector, default_constructor)
{
    object_count = 0;
    {
        Small_Vector<Counted, 4> vec;
        EXPECT_EQ(vec.size(), 0);
        EXPECT_TRUE(vec.empty());
        EXPECT_EQ(vec.capacity(), 4);
        EXPECT_EQ(vec.small_capacity(), 4);
        EXPECT_TRUE(vec.small());
        EXPECT_EQ(object_count, 4);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, copy_constructor_small)
{
    object_count = 0;
    {
        Small_Vector<Counted, 4> vec1;
        vec1.push_back(Counted { 1 });
        vec1.push_back(Counted { 2 });
        EXPECT_TRUE(vec1.small());
        EXPECT_EQ(object_count, 4);

        Small_Vector<Counted, 4> vec2 = vec1;
        EXPECT_EQ(vec2.size(), 2);
        EXPECT_TRUE(vec2.small());
        EXPECT_EQ(object_count, 8);
        EXPECT_EQ(vec2[0].value, 1);
        EXPECT_EQ(vec2[1].value, 2);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, copy_constructor_dynamic)
{
    object_count = 0;
    {
        Small_Vector<Counted, 2> vec1;
        vec1.push_back(Counted { 1 });
        vec1.push_back(Counted { 2 });
        vec1.push_back(Counted { 3 });
        vec1.push_back(Counted { 4 });
        EXPECT_FALSE(vec1.small());
        EXPECT_EQ(object_count, 6);

        Small_Vector<Counted, 2> vec2 = vec1;
        EXPECT_EQ(vec2.size(), 4);
        EXPECT_FALSE(vec2.small());
        EXPECT_EQ(object_count, 12);
        EXPECT_EQ(vec2[0].value, 1);
        EXPECT_EQ(vec2[1].value, 2);
        EXPECT_EQ(vec2[2].value, 3);
        EXPECT_EQ(vec2[3].value, 4);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, move_constructor_small)
{
    object_count = 0;
    {
        Small_Vector<Counted, 4> vec1;
        vec1.push_back(Counted { 1 });
        vec1.push_back(Counted { 2 });
        EXPECT_TRUE(vec1.small());
        EXPECT_EQ(object_count, 4);

        Small_Vector<Counted, 4> vec2 = std::move(vec1);
        EXPECT_EQ(vec2.size(), 2);
        EXPECT_TRUE(vec2.small());
        EXPECT_EQ(vec1.size(), 0);
        EXPECT_EQ(object_count, 8);
        EXPECT_EQ(vec2[0].value, 1);
        EXPECT_EQ(vec2[1].value, 2);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, move_constructor_dynamic)
{
    object_count = 0;
    {
        Small_Vector<Counted, 2> vec1;
        vec1.push_back(Counted { 1 });
        vec1.push_back(Counted { 2 });
        vec1.push_back(Counted { 3 });
        vec1.push_back(Counted { 4 });
        EXPECT_FALSE(vec1.small());
        EXPECT_EQ(object_count, 6);

        Small_Vector<Counted, 2> vec2 = std::move(vec1);
        EXPECT_EQ(vec2.size(), 4);
        EXPECT_FALSE(vec2.small());
        EXPECT_EQ(vec1.size(), 0);
        EXPECT_EQ(object_count, 8);
        EXPECT_EQ(vec2[0].value, 1);
        EXPECT_EQ(vec2[1].value, 2);
        EXPECT_EQ(vec2[2].value, 3);
        EXPECT_EQ(vec2[3].value, 4);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, move_constructor_small_with_dynamic_allocation)
{
    object_count = 0;
    {
        Small_Vector<Counted, 2> vec1;
        vec1.push_back(Counted { 1 });
        vec1.push_back(Counted { 2 });
        vec1.push_back(Counted { 3 });
        vec1.pop_back();
        EXPECT_TRUE(vec1.small());
        EXPECT_EQ(vec1.capacity(), 4);
        EXPECT_EQ(object_count, 2);

        Small_Vector<Counted, 2> vec2 = std::move(vec1);
        EXPECT_EQ(vec2.size(), 2);
        EXPECT_TRUE(vec2.small());
        EXPECT_EQ(vec2.capacity(), 4);
        EXPECT_EQ(object_count, 4);
        EXPECT_EQ(vec2[0].value, 1);
        EXPECT_EQ(vec2[1].value, 2);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, copy_assignment)
{
    object_count = 0;
    {
        Small_Vector<Counted, 4> vec1;
        vec1.push_back(Counted { 1 });
        vec1.push_back(Counted { 2 });
        EXPECT_TRUE(vec1.small());

        Small_Vector<Counted, 4> vec2;
        vec2.push_back(Counted { 9 });
        EXPECT_TRUE(vec2.small());

        vec2 = vec1;
        EXPECT_EQ(vec2.size(), 2);
        EXPECT_TRUE(vec2.small());
        EXPECT_EQ(object_count, 8);
        EXPECT_EQ(vec2[0].value, 1);
        EXPECT_EQ(vec2[1].value, 2);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, copy_assignment_self)
{
    object_count = 0;
    {
        Small_Vector<Counted, 4> vec;
        vec.push_back(Counted { 1 });
        vec.push_back(Counted { 2 });
        EXPECT_TRUE(vec.small());

        // Forming a pointer obfuscates the self-assignment to warnings.
        auto* const vec_pointer = &vec;
        vec = *vec_pointer;
        EXPECT_EQ(vec.size(), 2);
        EXPECT_TRUE(vec.small());
        EXPECT_EQ(object_count, 4);
        EXPECT_EQ(vec[0].value, 1);
        EXPECT_EQ(vec[1].value, 2);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, move_assignment)
{
    object_count = 0;
    {
        Small_Vector<Counted, 4> vec1;
        vec1.push_back(Counted { 1 });
        vec1.push_back(Counted { 2 });
        EXPECT_TRUE(vec1.small());

        Small_Vector<Counted, 4> vec2;
        vec2.push_back(Counted { 9 });
        EXPECT_TRUE(vec2.small());

        vec2 = std::move(vec1);
        EXPECT_EQ(vec2.size(), 2);
        EXPECT_EQ(vec1.size(), 0);
        EXPECT_TRUE(vec2.small());
        EXPECT_EQ(object_count, 8);
        EXPECT_EQ(vec2[0].value, 1);
        EXPECT_EQ(vec2[1].value, 2);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, move_assignment_self)
{
    object_count = 0;
    {
        Small_Vector<Counted, 4> vec;
        vec.push_back(Counted { 1 });
        vec.push_back(Counted { 2 });
        EXPECT_TRUE(vec.small());

        // Forming a pointer obfuscates the self-assignment to warnings.
        auto* const vec_pointer = &vec;
        vec = std::move(*vec_pointer);
        EXPECT_EQ(vec.size(), 2);
        EXPECT_TRUE(vec.small());
        EXPECT_EQ(object_count, 4);
        EXPECT_EQ(vec[0].value, 1);
        EXPECT_EQ(vec[1].value, 2);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, initializer_list_constructor)
{
    object_count = 0;
    {
        Small_Vector<Counted, 4> vec { Counted { 1 }, Counted { 2 }, Counted { 3 } };
        EXPECT_EQ(vec.size(), 3);
        EXPECT_FALSE(vec.empty());
        EXPECT_TRUE(vec.small());
        EXPECT_EQ(vec.capacity(), 4);
        EXPECT_EQ(object_count, 4);
        EXPECT_EQ(vec[0].value, 1);
        EXPECT_EQ(vec[1].value, 2);
        EXPECT_EQ(vec[2].value, 3);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, push_back_small)
{
    object_count = 0;
    {
        Small_Vector<Counted, 4> vec;
        vec.push_back(Counted { 1 });
        EXPECT_EQ(vec.size(), 1);
        EXPECT_FALSE(vec.empty());
        EXPECT_TRUE(vec.small());
        EXPECT_EQ(object_count, 4);
        EXPECT_EQ(vec[0].value, 1);

        vec.push_back(Counted { 2 });
        vec.push_back(Counted { 3 });
        EXPECT_EQ(vec.size(), 3);
        EXPECT_TRUE(vec.small());
        EXPECT_EQ(object_count, 4);
        EXPECT_EQ(vec[1].value, 2);
        EXPECT_EQ(vec[2].value, 3);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, push_back_grow_to_dynamic)
{
    object_count = 0;
    {
        Small_Vector<Counted, 2> vec;
        vec.push_back(Counted { 1 });
        vec.push_back(Counted { 2 });
        EXPECT_TRUE(vec.small());
        EXPECT_EQ(vec.size(), 2);
        EXPECT_EQ(object_count, 2);
        EXPECT_EQ(vec[0].value, 1);
        EXPECT_EQ(vec[1].value, 2);

        vec.push_back(Counted { 3 });
        EXPECT_FALSE(vec.small());
        EXPECT_EQ(vec.size(), 3);
        EXPECT_GE(vec.capacity(), 4);
        EXPECT_EQ(object_count, 5);
        EXPECT_EQ(vec[2].value, 3);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, emplace_back)
{
    object_count = 0;
    {
        Small_Vector<Counted, 2> vec;
        vec.emplace_back(1);
        vec.emplace_back(2);
        EXPECT_EQ(vec.size(), 2);
        EXPECT_TRUE(vec.small());
        EXPECT_EQ(object_count, 2);
        EXPECT_EQ(vec[0].value, 1);
        EXPECT_EQ(vec[1].value, 2);

        vec.emplace_back(3);
        EXPECT_EQ(vec.size(), 3);
        EXPECT_FALSE(vec.small());
        EXPECT_EQ(object_count, 5);
        EXPECT_EQ(vec[2].value, 3);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, pop_back_small)
{
    object_count = 0;
    {
        Small_Vector<Counted, 4> vec;
        vec.push_back(Counted { 1 });
        vec.push_back(Counted { 2 });
        vec.push_back(Counted { 3 });
        EXPECT_EQ(object_count, 4);

        vec.pop_back();
        EXPECT_EQ(vec.size(), 2);
        EXPECT_TRUE(vec.small());
        EXPECT_EQ(object_count, 4);
        EXPECT_EQ(vec[0].value, 1);
        EXPECT_EQ(vec[1].value, 2);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, pop_back_dynamic)
{
    object_count = 0;
    {
        Small_Vector<Counted, 2> vec;
        vec.push_back(Counted { 1 });
        vec.push_back(Counted { 2 });
        vec.push_back(Counted { 3 });
        vec.push_back(Counted { 4 });
        EXPECT_FALSE(vec.small());
        EXPECT_EQ(object_count, 6);

        vec.pop_back();
        EXPECT_EQ(vec.size(), 3);
        EXPECT_FALSE(vec.small());
        EXPECT_EQ(object_count, 5);
        EXPECT_EQ(vec[0].value, 1);
        EXPECT_EQ(vec[1].value, 2);
        EXPECT_EQ(vec[2].value, 3);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, pop_back_transition_to_small)
{
    object_count = 0;
    {
        Small_Vector<Counted, 2> vec;
        vec.push_back(Counted { 1 });
        vec.push_back(Counted { 2 });
        vec.push_back(Counted { 3 });
        EXPECT_FALSE(vec.small());
        EXPECT_EQ(object_count, 5);

        vec.pop_back();
        EXPECT_EQ(vec.size(), 2);
        EXPECT_TRUE(vec.small());
        EXPECT_EQ(object_count, 2);
        EXPECT_EQ(vec[0].value, 1);
        EXPECT_EQ(vec[1].value, 2);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, index_access)
{
    object_count = 0;
    {
        Small_Vector<Counted, 2> vec;
        vec.push_back(Counted { 10 });
        vec.push_back(Counted { 20 });
        EXPECT_TRUE(vec.small());

        Counted& first = vec[0];
        Counted& second = vec[1];
        EXPECT_EQ(&first, &vec[0]);
        EXPECT_EQ(&second, &vec[1]);
        EXPECT_EQ(first.value, 10);
        EXPECT_EQ(second.value, 20);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, index_access_dynamic)
{
    object_count = 0;
    {
        Small_Vector<Counted, 2> vec;
        vec.push_back(Counted { 10 });
        vec.push_back(Counted { 20 });
        vec.push_back(Counted { 30 });
        EXPECT_FALSE(vec.small());

        Counted& first = vec[0];
        Counted& third = vec[2];
        EXPECT_EQ(&first, &vec[0]);
        EXPECT_EQ(&third, &vec[2]);
        EXPECT_EQ(first.value, 10);
        EXPECT_EQ(third.value, 30);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, front_back)
{
    object_count = 0;
    {
        Small_Vector<Counted, 4> vec;
        vec.push_back(Counted { 1 });
        vec.push_back(Counted { 2 });
        vec.push_back(Counted { 3 });
        EXPECT_TRUE(vec.small());

        Counted& first = vec.front();
        Counted& last = vec.back();
        EXPECT_EQ(&first, &vec[0]);
        EXPECT_EQ(&last, &vec[2]);
        EXPECT_EQ(first.value, 1);
        EXPECT_EQ(last.value, 3);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, const_index_access)
{
    object_count = 0;
    {
        Small_Vector<Counted, 2> vec;
        vec.push_back(Counted { 10 });
        vec.push_back(Counted { 20 });
        EXPECT_TRUE(vec.small());

        const Small_Vector<Counted, 2>& cvec = vec;
        const Counted& first = cvec[0];
        const Counted& second = cvec[1];
        EXPECT_EQ(&first, &cvec[0]);
        EXPECT_EQ(&second, &cvec[1]);
        EXPECT_EQ(first.value, 10);
        EXPECT_EQ(second.value, 20);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, const_front_back)
{
    object_count = 0;
    {
        Small_Vector<Counted, 4> vec;
        vec.push_back(Counted { 1 });
        vec.push_back(Counted { 2 });
        EXPECT_TRUE(vec.small());

        const Small_Vector<Counted, 4>& cvec = vec;
        const Counted& first = cvec.front();
        const Counted& last = cvec.back();
        EXPECT_EQ(&first, &cvec[0]);
        EXPECT_EQ(&last, &cvec[1]);
        EXPECT_EQ(first.value, 1);
        EXPECT_EQ(last.value, 2);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, clear_small)
{
    object_count = 0;
    {
        Small_Vector<Counted, 4> vec;
        vec.push_back(Counted { 1 });
        vec.push_back(Counted { 2 });
        EXPECT_EQ(object_count, 4);

        vec.clear();
        EXPECT_EQ(vec.size(), 0);
        EXPECT_TRUE(vec.empty());
        EXPECT_TRUE(vec.small());
        EXPECT_EQ(object_count, 4);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, clear_dynamic)
{
    object_count = 0;
    {
        Small_Vector<Counted, 2> vec;
        vec.push_back(Counted { 1 });
        vec.push_back(Counted { 2 });
        vec.push_back(Counted { 3 });
        vec.push_back(Counted { 4 });
        EXPECT_FALSE(vec.small());
        EXPECT_EQ(object_count, 6);

        vec.clear();
        EXPECT_EQ(vec.size(), 0);
        EXPECT_TRUE(vec.empty());
        EXPECT_TRUE(vec.small());
        EXPECT_EQ(object_count, 2);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, reserve)
{
    object_count = 0;
    {
        Small_Vector<Counted, 2> vec;
        vec.push_back(Counted { 7 });
        EXPECT_TRUE(vec.small());
        EXPECT_EQ(object_count, 2);

        vec.reserve(8);
        EXPECT_FALSE(vec.small());
        EXPECT_GE(vec.capacity(), 8);
        EXPECT_EQ(vec.size(), 1);
        EXPECT_EQ(object_count, 3);
        EXPECT_EQ(vec[0].value, 7);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, reserve_no_op)
{
    object_count = 0;
    {
        Small_Vector<Counted, 4> vec;
        vec.push_back(Counted { 1 });
        vec.push_back(Counted { 2 });
        EXPECT_TRUE(vec.small());

        vec.reserve(2);
        EXPECT_TRUE(vec.small());
        EXPECT_EQ(vec.capacity(), 4);
        EXPECT_EQ(object_count, 4);
        EXPECT_EQ(vec[0].value, 1);
        EXPECT_EQ(vec[1].value, 2);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, insert_small_storage)
{
    object_count = 0;
    {
        Small_Vector<Counted, 4> vec;
        vec.push_back(Counted { 1 });
        vec.push_back(Counted { 4 });

        const std::array<Counted, 2> extra { Counted { 2 }, Counted { 3 } };
        vec.insert(vec.begin() + 1, extra.begin(), extra.end());

        EXPECT_TRUE(vec.small());
        EXPECT_EQ(vec.size(), 4);
        EXPECT_EQ(object_count, 6);
        EXPECT_EQ(vec[0].value, 1);
        EXPECT_EQ(vec[1].value, 2);
        EXPECT_EQ(vec[2].value, 3);
        EXPECT_EQ(vec[3].value, 4);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, insert_to_dynamic_storage)
{
    object_count = 0;
    {
        Small_Vector<Counted, 2> vec;
        vec.push_back(Counted { 1 });
        vec.push_back(Counted { 2 });

        const std::array<Counted, 1> extra { Counted { 9 } };
        vec.insert(vec.begin(), extra.begin(), extra.end());

        EXPECT_FALSE(vec.small());
        EXPECT_EQ(vec.size(), 3);
        EXPECT_GE(vec.capacity(), 4);
        EXPECT_EQ(object_count, 6);
        EXPECT_EQ(vec[0].value, 9);
        EXPECT_EQ(vec[1].value, 1);
        EXPECT_EQ(vec[2].value, 2);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, insert_dynamic_growth)
{
    object_count = 0;
    {
        Small_Vector<Counted, 2> vec;
        vec.push_back(Counted { 1 });
        vec.push_back(Counted { 2 });
        vec.push_back(Counted { 3 });

        const std::array<Counted, 2> extra { Counted { 4 }, Counted { 5 } };
        vec.insert(vec.begin() + 1, extra.begin(), extra.end());

        EXPECT_FALSE(vec.small());
        EXPECT_EQ(vec.size(), 5);
        EXPECT_EQ(vec.capacity(), 8);
        EXPECT_EQ(object_count, 9);
        EXPECT_EQ(vec[0].value, 1);
        EXPECT_EQ(vec[1].value, 4);
        EXPECT_EQ(vec[2].value, 5);
        EXPECT_EQ(vec[3].value, 2);
        EXPECT_EQ(vec[4].value, 3);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, equality_comparison)
{
    object_count = 0;
    {
        Small_Vector<Counted, 2> vec1;
        vec1.push_back(Counted { 1 });
        vec1.push_back(Counted { 2 });

        Small_Vector<Counted, 2> vec2;
        vec2.push_back(Counted { 1 });
        vec2.push_back(Counted { 2 });

        Small_Vector<Counted, 2> vec3;
        vec3.push_back(Counted { 1 });
        vec3.push_back(Counted { 3 });

        EXPECT_TRUE(vec1 == vec2);
        EXPECT_FALSE(vec1 != vec2);
        EXPECT_TRUE(vec1 != vec3);

        vec1.push_back(Counted { 4 });
        vec2.push_back(Counted { 4 });
        EXPECT_FALSE(vec1.small());
        EXPECT_FALSE(vec2.small());
        EXPECT_TRUE(vec1 == vec2);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, ordering_comparison)
{
    object_count = 0;
    {
        Small_Vector<Counted, 2> vec1;
        vec1.push_back(Counted { 1 });
        vec1.push_back(Counted { 2 });

        Small_Vector<Counted, 2> vec2;
        vec2.push_back(Counted { 1 });
        vec2.push_back(Counted { 3 });

        Small_Vector<Counted, 2> vec3;
        vec3.push_back(Counted { 1 });
        vec3.push_back(Counted { 2 });
        vec3.push_back(Counted { 0 });

        EXPECT_TRUE(vec1 < vec2);
        EXPECT_TRUE(vec2 > vec1);
        EXPECT_TRUE(vec1 < vec3);
        EXPECT_TRUE(vec3 > vec1);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, swap_both_small)
{
    object_count = 0;
    {
        Small_Vector<Counted, 4> vec1;
        vec1.push_back(Counted { 1 });
        vec1.push_back(Counted { 2 });

        Small_Vector<Counted, 4> vec2;
        vec2.push_back(Counted { 9 });

        vec1.swap(vec2);
        EXPECT_EQ(vec1.size(), 1);
        EXPECT_EQ(vec2.size(), 2);
        EXPECT_TRUE(vec1.small());
        EXPECT_TRUE(vec2.small());
        EXPECT_EQ(object_count, 8);
        EXPECT_EQ(vec1[0].value, 9);
        EXPECT_EQ(vec2[0].value, 1);
        EXPECT_EQ(vec2[1].value, 2);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, swap_both_small_reverse)
{
    object_count = 0;
    {
        Small_Vector<Counted, 4> vec1;
        vec1.push_back(Counted { 1 });

        Small_Vector<Counted, 4> vec2;
        vec2.push_back(Counted { 2 });
        vec2.push_back(Counted { 3 });
        vec2.push_back(Counted { 4 });

        vec1.swap(vec2);
        EXPECT_EQ(vec1.size(), 3);
        EXPECT_EQ(vec2.size(), 1);
        EXPECT_TRUE(vec1.small());
        EXPECT_TRUE(vec2.small());
        EXPECT_EQ(object_count, 8);
        EXPECT_EQ(vec1[0].value, 2);
        EXPECT_EQ(vec1[1].value, 3);
        EXPECT_EQ(vec1[2].value, 4);
        EXPECT_EQ(vec2[0].value, 1);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, swap_both_dynamic)
{
    object_count = 0;
    {
        Small_Vector<Counted, 2> vec1;
        vec1.push_back(Counted { 1 });
        vec1.push_back(Counted { 2 });
        vec1.push_back(Counted { 3 });

        Small_Vector<Counted, 2> vec2;
        vec2.push_back(Counted { 4 });
        vec2.push_back(Counted { 5 });
        vec2.push_back(Counted { 6 });
        vec2.push_back(Counted { 7 });

        vec1.swap(vec2);
        EXPECT_EQ(vec1.size(), 4);
        EXPECT_EQ(vec2.size(), 3);
        EXPECT_FALSE(vec1.small());
        EXPECT_FALSE(vec2.small());
        EXPECT_EQ(object_count, 11);
        EXPECT_EQ(vec1[0].value, 4);
        EXPECT_EQ(vec1[3].value, 7);
        EXPECT_EQ(vec2[0].value, 1);
        EXPECT_EQ(vec2[2].value, 3);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, swap_small_dynamic)
{
    object_count = 0;
    {
        Small_Vector<Counted, 2> vec1;
        vec1.push_back(Counted { 1 });

        Small_Vector<Counted, 2> vec2;
        vec2.push_back(Counted { 2 });
        vec2.push_back(Counted { 3 });
        vec2.push_back(Counted { 4 });

        vec1.swap(vec2);
        EXPECT_EQ(vec1.size(), 3);
        EXPECT_EQ(vec2.size(), 1);
        EXPECT_FALSE(vec1.small());
        EXPECT_TRUE(vec2.small());
        EXPECT_EQ(object_count, 7);
        EXPECT_EQ(vec1[0].value, 2);
        EXPECT_EQ(vec1[2].value, 4);
        EXPECT_EQ(vec2[0].value, 1);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, swap_dynamic_small)
{
    object_count = 0;
    {
        Small_Vector<Counted, 2> vec1;
        vec1.push_back(Counted { 1 });
        vec1.push_back(Counted { 2 });
        vec1.push_back(Counted { 3 });

        Small_Vector<Counted, 2> vec2;
        vec2.push_back(Counted { 4 });

        vec1.swap(vec2);
        EXPECT_EQ(vec1.size(), 1);
        EXPECT_EQ(vec2.size(), 3);
        EXPECT_TRUE(vec1.small());
        EXPECT_FALSE(vec2.small());
        EXPECT_EQ(object_count, 7);
        EXPECT_EQ(vec1[0].value, 4);
        EXPECT_EQ(vec2[0].value, 1);
        EXPECT_EQ(vec2[2].value, 3);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, swap_self)
{
    object_count = 0;
    {
        Small_Vector<Counted, 4> vec;
        vec.push_back(Counted { 1 });
        vec.push_back(Counted { 2 });
        EXPECT_TRUE(vec.small());

        vec.swap(vec);
        EXPECT_EQ(vec.size(), 2);
        EXPECT_TRUE(vec.small());
        EXPECT_EQ(object_count, 4);
        EXPECT_EQ(vec[0].value, 1);
        EXPECT_EQ(vec[1].value, 2);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, free_swap_function)
{
    object_count = 0;
    {
        Small_Vector<Counted, 4> vec1;
        vec1.push_back(Counted { 1 });
        EXPECT_TRUE(vec1.small());

        Small_Vector<Counted, 4> vec2;
        vec2.push_back(Counted { 2 });
        vec2.push_back(Counted { 3 });
        EXPECT_TRUE(vec2.small());

        swap(vec1, vec2);
        EXPECT_EQ(vec1.size(), 2);
        EXPECT_EQ(vec2.size(), 1);
        EXPECT_TRUE(vec1.small());
        EXPECT_TRUE(vec2.small());
        EXPECT_EQ(object_count, 8);
        EXPECT_EQ(vec1[0].value, 2);
        EXPECT_EQ(vec1[1].value, 3);
        EXPECT_EQ(vec2[0].value, 1);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, iterators)
{
    object_count = 0;
    {
        Small_Vector<Counted, 4> vec;
        vec.push_back(Counted { 1 });
        vec.push_back(Counted { 2 });
        EXPECT_TRUE(vec.small());

        EXPECT_EQ(vec.end() - vec.begin(), 2);
        EXPECT_EQ(vec.cend() - vec.cbegin(), 2);
        EXPECT_EQ(&*vec.begin(), &vec[0]);
        EXPECT_EQ(&*vec.cbegin(), &vec[0]);
        EXPECT_EQ(object_count, 4);
        EXPECT_EQ(vec.begin()->value, 1);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, iterators_dynamic)
{
    object_count = 0;
    {
        Small_Vector<Counted, 2> vec;
        vec.push_back(Counted { 1 });
        vec.push_back(Counted { 2 });
        vec.push_back(Counted { 3 });
        EXPECT_FALSE(vec.small());

        EXPECT_EQ(vec.end() - vec.begin(), 3);
        EXPECT_EQ(vec.cend() - vec.cbegin(), 3);
        EXPECT_EQ(&*vec.begin(), &vec[0]);
        EXPECT_EQ(&*vec.cbegin(), &vec[0]);
        EXPECT_EQ(object_count, 5);
        EXPECT_EQ(vec.begin()->value, 1);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, const_iterators)
{
    object_count = 0;
    {
        Small_Vector<Counted, 4> vec;
        vec.push_back(Counted { 1 });
        vec.push_back(Counted { 2 });
        EXPECT_TRUE(vec.small());

        const auto& cvec = std::as_const(vec);
        EXPECT_EQ(cvec.end() - cvec.begin(), 2);
        EXPECT_EQ(cvec.cend() - cvec.cbegin(), 2);
        EXPECT_EQ(&*cvec.begin(), &cvec[0]);
        EXPECT_EQ(object_count, 4);
        EXPECT_EQ(cvec.begin()->value, 1);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, allocator_constructor)
{
    object_count = 0;
    {
        std::allocator<Counted> alloc;
        Small_Vector<Counted, 4> vec(alloc);
        EXPECT_EQ(vec.size(), 0);
        EXPECT_TRUE(vec.empty());
        EXPECT_TRUE(vec.small());
        EXPECT_EQ(object_count, 4);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, get_allocator)
{
    object_count = 0;
    {
        Small_Vector<Counted, 4> vec;
        void(vec.get_allocator());
        EXPECT_TRUE(vec.small());
        EXPECT_EQ(object_count, 4);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, multiple_growths)
{
    object_count = 0;
    {
        Small_Vector<Counted, 2> vec;
        vec.push_back(Counted { 1 });
        vec.push_back(Counted { 2 });
        EXPECT_TRUE(vec.small());
        EXPECT_EQ(vec.capacity(), 2);
        EXPECT_EQ(object_count, 2);

        vec.push_back(Counted { 3 });
        EXPECT_FALSE(vec.small());
        EXPECT_EQ(vec.capacity(), 4);
        EXPECT_EQ(object_count, 5);

        vec.push_back(Counted { 4 });
        vec.push_back(Counted { 5 });
        EXPECT_EQ(vec.size(), 5);
        EXPECT_EQ(vec.capacity(), 8);
        EXPECT_FALSE(vec.small());
        EXPECT_EQ(object_count, 7);
        EXPECT_EQ(vec[0].value, 1);
        EXPECT_EQ(vec[4].value, 5);
    }
    EXPECT_EQ(object_count, 0);
}

TEST(Small_Vector, grow_with_existing_allocation)
{
    object_count = 0;
    {
        Small_Vector<Counted, 2> vec;
        vec.push_back(Counted { 1 });
        vec.push_back(Counted { 2 });
        vec.push_back(Counted { 3 });
        vec.pop_back();
        EXPECT_TRUE(vec.small());
        EXPECT_EQ(vec.capacity(), 4);
        EXPECT_EQ(object_count, 2);

        vec.push_back(Counted { 4 });
        EXPECT_FALSE(vec.small());
        EXPECT_EQ(vec.capacity(), 4);
        EXPECT_EQ(vec.size(), 3);
        EXPECT_EQ(object_count, 5);
        EXPECT_EQ(vec[0].value, 1);
        EXPECT_EQ(vec[1].value, 2);
        EXPECT_EQ(vec[2].value, 4);
    }
    EXPECT_EQ(object_count, 0);
}

consteval int f()
{
    Small_Vector<int, 16> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    COWEL_ASSERT(v[0] == 1);
    COWEL_ASSERT(v[1] == 2);
    COWEL_ASSERT(v[2] == 3);

    auto x = v;
    return 0;
}

[[maybe_unused]]
constexpr int x
    = f();

} // namespace
} // namespace cowel
