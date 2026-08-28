// SPDX-License-Identifier: MIT
/**
* @file test_static_vector.cpp
*
* @brief Unit tests for etools::memory::static_vector.
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2026-08-27
*
* @copyright
* MIT License
* Copyright (c) 2026 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#include <gtest/gtest.h>
#include <etools/memory/static_vector.hpp>

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using etools::memory::static_vector;

namespace {

    /**
    * @brief Instrumented element: counts every construction and destruction.
    *
    * The counters are what make "did the container destroy each element exactly
    * once" a testable claim rather than a hope.
    */
    struct counted {
        static inline int constructions = 0;
        static inline int destructions = 0;
        static inline int moves = 0;

        int value;

        explicit counted(int v = 0) noexcept : value{v} { ++constructions; }

        counted(const counted& other) noexcept : value{other.value} { ++constructions; }

        counted(counted&& other) noexcept : value{other.value} {
            other.value = -1;
            ++constructions;
            ++moves;
        }

        counted& operator=(counted&& other) noexcept {
            value = other.value;
            other.value = -1;
            ++moves;
            return *this;
        }

        counted& operator=(const counted& other) noexcept {
            value = other.value;
            return *this;
        }

        ~counted() noexcept { ++destructions; }

        static void reset() noexcept {
            constructions = 0;
            destructions = 0;
            moves = 0;
        }

        static int live() noexcept { return constructions - destructions; }
    };

    /// @brief Throws from its constructor on demand, to test the strong guarantee.
    struct throwing {
        int value;

        explicit throwing(int v, bool should_throw) : value{v} {
            if (should_throw)
                throw std::runtime_error("throwing ctor");
        }
    };

    /// @brief Multi-argument, non-default-constructible: exercises perfect forwarding.
    struct two_args {
        int a;
        std::string b;

        two_args(int a_in, std::string b_in) : a{a_in}, b{std::move(b_in)} {}
    };

    /// @brief Over-aligned type, to confirm the storage respects alignment.
    struct alignas(32) over_aligned {
        int value;
        explicit over_aligned(int v = 0) noexcept : value{v} {}
    };

    /// @brief Fixture that keeps the `counted` statics from leaking between tests.
    class static_vector_counted : public ::testing::Test {
    protected:
        void SetUp() override { counted::reset(); }
    };

} // namespace


// --------------------------------------------------------------- basic shape

TEST(static_vector_basics, starts_empty)
{
    static_vector<int, 4> v;

    EXPECT_EQ(v.size(), 0u);
    EXPECT_TRUE(v.empty());
    EXPECT_FALSE(v.full());
    EXPECT_EQ(v.begin(), v.end());
}

TEST(static_vector_basics, capacity_is_a_constant_expression)
{
    static_assert(static_vector<int, 7>::capacity() == 7);
    EXPECT_EQ((static_vector<int, 7>::capacity()), 7u);
}

TEST(static_vector_basics, emplace_back_returns_pointer_to_new_element)
{
    static_vector<int, 4> v;

    int* first = v.try_emplace_back(10);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(*first, 10);
    EXPECT_EQ(v.size(), 1u);
    EXPECT_FALSE(v.empty());
}

TEST(static_vector_basics, fills_to_capacity_then_reports_full)
{
    static_vector<int, 3> v;

    for (int i = 0; i < 3; ++i)
        EXPECT_NE(v.try_emplace_back(i), nullptr);

    EXPECT_TRUE(v.full());
    EXPECT_EQ(v.size(), 3u);
}

TEST(static_vector_basics, emplace_back_on_full_container_returns_nullptr)
{
    static_vector<int, 2> v;

    (void)v.try_emplace_back(1);
    (void)v.try_emplace_back(2);

    EXPECT_EQ(v.try_emplace_back(3), nullptr);
    // The rejection must not disturb the container.
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
}

TEST(static_vector_basics, forwards_multiple_constructor_arguments)
{
    static_vector<two_args, 2> v;

    two_args* item = v.try_emplace_back(7, std::string{"seven"});

    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->a, 7);
    EXPECT_EQ(item->b, "seven");
}

TEST(static_vector_basics, indexing_and_front_back)
{
    static_vector<int, 4> v;
    for (int i = 0; i < 3; ++i)
        (void)v.try_emplace_back(i * 10);

    EXPECT_EQ(v[0], 0);
    EXPECT_EQ(v[1], 10);
    EXPECT_EQ(v[2], 20);
    EXPECT_EQ(v.front(), 0);
    EXPECT_EQ(v.back(), 20);
}

TEST(static_vector_basics, const_access_surface)
{
    static_vector<int, 4> v;
    (void)v.try_emplace_back(1);
    (void)v.try_emplace_back(2);

    const auto& cv = v;

    EXPECT_EQ(cv[0], 1);
    EXPECT_EQ(cv.front(), 1);
    EXPECT_EQ(cv.back(), 2);
    EXPECT_EQ(cv.size(), 2u);
    EXPECT_EQ(cv.cend() - cv.cbegin(), 2);
    EXPECT_EQ(std::accumulate(cv.begin(), cv.end(), 0), 3);
}


// ------------------------------------------------------- address stability

TEST(static_vector_stability, element_addresses_survive_later_insertions)
{
    static_vector<int, 8> v;

    int* first = v.try_emplace_back(1);
    ASSERT_NE(first, nullptr);

    std::vector<int*> addresses{first};
    for (int i = 2; i <= 8; ++i) {
        int* p = v.try_emplace_back(i);
        ASSERT_NE(p, nullptr);
        addresses.push_back(p);
    }

    // The whole point of the type: nothing moved.
    EXPECT_EQ(*first, 1);
    for (std::size_t i = 0; i < addresses.size(); ++i) {
        EXPECT_EQ(addresses[i], &v[i]);
        EXPECT_EQ(*addresses[i], static_cast<int>(i) + 1);
    }
}

TEST(static_vector_stability, storage_is_contiguous)
{
    static_vector<int, 4> v;
    for (int i = 0; i < 4; ++i)
        (void)v.try_emplace_back(i);

    EXPECT_EQ(&v[1] - &v[0], 1);
    EXPECT_EQ(&v[3] - &v[0], 3);
    EXPECT_EQ(v.end() - v.begin(), 4);
}

TEST(static_vector_stability, respects_over_alignment)
{
    static_vector<over_aligned, 3> v;
    for (int i = 0; i < 3; ++i)
        ASSERT_NE(v.try_emplace_back(i), nullptr);

    for (std::size_t i = 0; i < v.size(); ++i) {
        auto address = reinterpret_cast<std::uintptr_t>(&v[i]);
        EXPECT_EQ(address % alignof(over_aligned), 0u);
    }
}

TEST(static_vector_stability, holds_no_heap_storage)
{
    // Storage is inline, so the object is at least as large as its elements.
    // A heap-backed container would be pointer-sized regardless of Capacity.
    EXPECT_GE(sizeof(static_vector<int, 16>), 16 * sizeof(int));
    EXPECT_GT(sizeof(static_vector<int, 32>), sizeof(static_vector<int, 4>));
}


// ------------------------------------------------------------- swap_erase

TEST(static_vector_erase, swap_erase_moves_last_into_the_hole)
{
    static_vector<int, 4> v;
    for (int i = 1; i <= 4; ++i)
        (void)v.try_emplace_back(i);

    v.swap_erase(1);   // remove '2'; '4' takes its place

    ASSERT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 4);
    EXPECT_EQ(v[2], 3);
}

TEST(static_vector_erase, swap_erase_of_last_element_is_a_pop)
{
    static_vector<int, 4> v;
    for (int i = 1; i <= 3; ++i)
        (void)v.try_emplace_back(i);

    v.swap_erase(2);   // the last element - no self-move may occur

    ASSERT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
}

TEST(static_vector_erase, swap_erase_of_sole_element_empties_the_container)
{
    static_vector<int, 4> v;
    (void)v.try_emplace_back(42);

    v.swap_erase(0);

    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.begin(), v.end());
}

TEST_F(static_vector_counted, swap_erase_of_last_element_does_not_self_move)
{
    {
        static_vector<counted, 4> v;
        (void)v.try_emplace_back(1);
        (void)v.try_emplace_back(2);

        const int moves_before = counted::moves;
        v.swap_erase(1);   // last element

        // Nothing to relocate, so no move-assignment should have happened.
        EXPECT_EQ(counted::moves, moves_before);
        ASSERT_EQ(v.size(), 1u);
        EXPECT_EQ(v[0].value, 1);
    }
    EXPECT_EQ(counted::live(), 0);
}

TEST_F(static_vector_counted, swap_erase_destroys_exactly_one_element)
{
    {
        static_vector<counted, 4> v;
        for (int i = 0; i < 4; ++i)
            (void)v.try_emplace_back(i);

        const int destroyed_before = counted::destructions;
        v.swap_erase(0);

        EXPECT_EQ(counted::destructions, destroyed_before + 1);
        EXPECT_EQ(counted::live(), 3);
    }
    EXPECT_EQ(counted::live(), 0);
}

TEST(static_vector_erase, back_to_front_sweep_erases_the_intended_elements)
{
    static_vector<int, 8> v;
    for (int i = 0; i < 8; ++i)
        (void)v.try_emplace_back(i);

    // The documented multi-erase idiom: walk from the back so surviving
    // indices below the cursor keep their meaning.
    for (std::size_t i = v.size(); i-- > 0; )
        if (v[i] % 2 == 0)
            v.swap_erase(i);

    ASSERT_EQ(v.size(), 4u);
    std::vector<int> remaining(v.begin(), v.end());
    std::sort(remaining.begin(), remaining.end());
    EXPECT_EQ(remaining, (std::vector<int>{1, 3, 5, 7}));
}

TEST(static_vector_erase, pop_back_removes_the_last_element)
{
    static_vector<int, 4> v;
    (void)v.try_emplace_back(1);
    (void)v.try_emplace_back(2);

    v.pop_back();

    ASSERT_EQ(v.size(), 1u);
    EXPECT_EQ(v.back(), 1);
}

TEST(static_vector_erase, space_is_reusable_after_erasing)
{
    static_vector<int, 2> v;
    (void)v.try_emplace_back(1);
    (void)v.try_emplace_back(2);
    ASSERT_TRUE(v.full());

    v.swap_erase(0);
    EXPECT_FALSE(v.full());

    int* reused = v.try_emplace_back(3);
    ASSERT_NE(reused, nullptr);
    EXPECT_EQ(v.size(), 2u);
}


// --------------------------------------------------------------- lifetime

TEST_F(static_vector_counted, destructor_destroys_every_live_element)
{
    {
        static_vector<counted, 8> v;
        for (int i = 0; i < 5; ++i)
            (void)v.try_emplace_back(i);

        EXPECT_EQ(counted::live(), 5);
    }
    EXPECT_EQ(counted::live(), 0);
}

TEST_F(static_vector_counted, clear_destroys_every_element)
{
    static_vector<counted, 8> v;
    for (int i = 0; i < 5; ++i)
        (void)v.try_emplace_back(i);

    v.clear();

    EXPECT_EQ(counted::live(), 0);
    EXPECT_TRUE(v.empty());
}

TEST_F(static_vector_counted, clear_is_idempotent)
{
    static_vector<counted, 4> v;
    (void)v.try_emplace_back(1);

    v.clear();
    v.clear();

    EXPECT_EQ(counted::live(), 0);
    EXPECT_TRUE(v.empty());
}

TEST_F(static_vector_counted, rejected_emplace_constructs_nothing)
{
    static_vector<counted, 2> v;
    (void)v.try_emplace_back(1);
    (void)v.try_emplace_back(2);

    const int constructed_before = counted::constructions;
    EXPECT_EQ(v.try_emplace_back(3), nullptr);

    // A full container must not even build the argument object.
    EXPECT_EQ(counted::constructions, constructed_before);
    EXPECT_EQ(counted::live(), 2);
}

TEST_F(static_vector_counted, elements_are_destroyed_in_reverse_order)
{
    static std::vector<int> order;
    order.clear();

    struct recorder {
        int value;
        explicit recorder(int v) noexcept : value{v} {}
        ~recorder() noexcept { order.push_back(value); }
    };

    {
        static_vector<recorder, 4> v;
        for (int i = 0; i < 3; ++i)
            (void)v.try_emplace_back(i);
    }

    EXPECT_EQ(order, (std::vector<int>{2, 1, 0}));
}

TEST(static_vector_lifetime, throwing_constructor_leaves_container_unchanged)
{
    static_vector<throwing, 4> v;
    ASSERT_NE(v.try_emplace_back(1, false), nullptr);

    EXPECT_THROW((void)v.try_emplace_back(2, true), std::runtime_error);

    // Strong guarantee: the failed insertion is invisible.
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0].value, 1);

    // And the container is still usable.
    ASSERT_NE(v.try_emplace_back(3, false), nullptr);
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[1].value, 3);
}


// ---------------------------------------------------------- algorithm interop

TEST(static_vector_algorithms, works_with_standard_algorithms)
{
    static_vector<int, 8> v;
    for (int i = 1; i <= 5; ++i)
        (void)v.try_emplace_back(i);

    EXPECT_EQ(std::count_if(v.begin(), v.end(), [](int x) { return x % 2 == 0; }), 2);
    EXPECT_EQ(std::accumulate(v.begin(), v.end(), 0), 15);

    auto found = std::find_if(v.begin(), v.end(), [](int x) { return x == 3; });
    ASSERT_NE(found, v.end());
    EXPECT_EQ(*found, 3);

    EXPECT_EQ(std::find_if(v.begin(), v.end(), [](int x) { return x == 99; }), v.end());
}

TEST(static_vector_algorithms, iterators_are_contiguous_random_access)
{
    using iterator = static_vector<int, 4>::iterator;
    using traits = std::iterator_traits<iterator>;

    static_assert(std::is_same_v<iterator, int*>);
    static_assert(std::is_same_v<traits::iterator_category, std::random_access_iterator_tag>);
    SUCCEED();
}

TEST(static_vector_algorithms, range_for_visits_every_element_in_order)
{
    static_vector<int, 4> v;
    for (int i = 0; i < 4; ++i)
        (void)v.try_emplace_back(i);

    std::vector<int> seen;
    for (int x : v)
        seen.push_back(x);

    EXPECT_EQ(seen, (std::vector<int>{0, 1, 2, 3}));
}


// ------------------------------------------------------------- type traits

TEST(static_vector_traits, is_pinned)
{
    using vec = static_vector<int, 4>;

    static_assert(not std::is_copy_constructible_v<vec>);
    static_assert(not std::is_copy_assignable_v<vec>);
    static_assert(not std::is_move_constructible_v<vec>);
    static_assert(not std::is_move_assignable_v<vec>);
    SUCCEED();
}

TEST(static_vector_traits, exposes_the_expected_member_types)
{
    using vec = static_vector<counted, 4>;

    static_assert(std::is_same_v<vec::value_type, counted>);
    static_assert(std::is_same_v<vec::size_type, std::size_t>);
    static_assert(std::is_same_v<vec::iterator, counted*>);
    static_assert(std::is_same_v<vec::const_iterator, const counted*>);
    static_assert(std::is_same_v<vec::reference, counted&>);
    static_assert(std::is_same_v<vec::const_reference, const counted&>);
    SUCCEED();
}

TEST(static_vector_traits, supports_non_movable_elements)
{
    // Nothing in the container relocates an element, so a pinned type is fine
    // as long as swap_erase is not asked to fill a hole from the middle.
    struct pinned {
        int value;
        explicit pinned(int v) noexcept : value{v} {}
        pinned(const pinned&) = delete;
        pinned& operator=(const pinned&) = delete;
        pinned(pinned&&) = delete;
        pinned& operator=(pinned&&) = delete;
    };

    static_vector<pinned, 4> v;
    ASSERT_NE(v.try_emplace_back(1), nullptr);
    ASSERT_NE(v.try_emplace_back(2), nullptr);

    EXPECT_EQ(v[0].value, 1);
    EXPECT_EQ(v.size(), 2u);

    v.pop_back();
    EXPECT_EQ(v.size(), 1u);
}


// ------------------------------------------------------------- boundaries

TEST(static_vector_boundaries, capacity_of_one)
{
    static_vector<int, 1> v;

    ASSERT_NE(v.try_emplace_back(1), nullptr);
    EXPECT_TRUE(v.full());
    EXPECT_EQ(v.try_emplace_back(2), nullptr);

    v.swap_erase(0);
    EXPECT_TRUE(v.empty());
    EXPECT_NE(v.try_emplace_back(3), nullptr);
    EXPECT_EQ(v.front(), 3);
}

TEST_F(static_vector_counted, repeated_fill_and_clear_cycles_are_balanced)
{
    static_vector<counted, 4> v;

    for (int cycle = 0; cycle < 10; ++cycle) {
        for (int i = 0; i < 4; ++i)
            ASSERT_NE(v.try_emplace_back(i), nullptr);
        EXPECT_TRUE(v.full());
        v.clear();
        EXPECT_TRUE(v.empty());
    }

    EXPECT_EQ(counted::live(), 0);
}

TEST(static_vector_boundaries, churn_keeps_size_and_contents_consistent)
{
    static_vector<int, 6> v;
    for (int i = 0; i < 6; ++i)
        (void)v.try_emplace_back(i);

    // Erase and refill repeatedly; the container must never exceed capacity
    // nor lose track of its size.
    for (int round = 0; round < 20; ++round) {
        v.swap_erase(v.size() / 2);
        EXPECT_EQ(v.size(), 5u);
        ASSERT_NE(v.try_emplace_back(100 + round), nullptr);
        EXPECT_EQ(v.size(), 6u);
        EXPECT_TRUE(v.full());
    }

    EXPECT_EQ(v.size(), 6u);
    EXPECT_EQ(v.end() - v.begin(), 6);
}
