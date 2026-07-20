#include <etools/meta/sort.hpp>
#include <gtest/gtest.h>
#include <cstdint>
#include <type_traits>

using namespace etools::meta;

// ---- Helper types with predictable size and alignment ----

// Sizes: 1, 2, 4, 8 (exact on any platform where intN_t is defined)
using s1 = std::int8_t;
using s2 = std::int16_t;
using s4 = std::int32_t;
using s8 = std::int64_t;

// Distinct wrapper types so we can test stability among same-size types.
// Two A-variants and two B-variants, all the same size as s4 (4 bytes).
struct A1 { std::int32_t x; };   // sizeof=4
struct A2 { std::int32_t x; };   // sizeof=4, same as A1 -> tests stability

struct B1 { std::int64_t x; };   // sizeof=8
struct B2 { std::int64_t x; };   // sizeof=8, same as B1 -> tests stability

// A type with non-trivial constructor, for the custom-comparator test.
struct NonTrivial {
    NonTrivial() {}
    int x{};
};

// Custom comparator: trivial types strictly before non-trivial ones.
// Among types with the same trivial-ness, neither is ordered before the other
// (equivalent), so their relative order in the output is determined by stability.
template<typename T, typename U>
struct prefer_trivial : std::bool_constant<
    std::is_trivial_v<T> && !std::is_trivial_v<U>
> {};

// Statically verify that our size assumptions hold on this platform.
static_assert(sizeof(s1) == 1);
static_assert(sizeof(s2) == 2);
static_assert(sizeof(s4) == 4);
static_assert(sizeof(s8) == 8);
static_assert(sizeof(A1) == 4);
static_assert(sizeof(A2) == 4);
static_assert(sizeof(B1) == 8);
static_assert(sizeof(B2) == 8);


// =========================================================================
// Edge cases
// =========================================================================

TEST(SortTest, EmptyPack) {
    EXPECT_TRUE((std::is_same_v<sort_t<size_greater>, typelist<>>));
    EXPECT_TRUE((std::is_same_v<sort_t<size_less>,    typelist<>>));
    EXPECT_TRUE((std::is_same_v<sort_t<align_greater>, typelist<>>));
}

TEST(SortTest, Singleton) {
    EXPECT_TRUE((std::is_same_v<sort_t<size_greater, s4>, typelist<s4>>));
    EXPECT_TRUE((std::is_same_v<sort_t<size_less,    s4>, typelist<s4>>));
}

TEST(SortTest, TwoElements_AlreadyOrdered) {
    // s8 (8) > s1 (1) -- already in descending order
    EXPECT_TRUE((std::is_same_v<sort_t<size_greater, s8, s1>, typelist<s8, s1>>));
    // s1 (1) < s8 (8) -- already in ascending order
    EXPECT_TRUE((std::is_same_v<sort_t<size_less,    s1, s8>, typelist<s1, s8>>));
}

TEST(SortTest, TwoElements_NeedsSwap) {
    // s1, s8: need to swap for descending sort
    EXPECT_TRUE((std::is_same_v<sort_t<size_greater, s1, s8>, typelist<s8, s1>>));
    // s8, s1: need to swap for ascending sort
    EXPECT_TRUE((std::is_same_v<sort_t<size_less,    s8, s1>, typelist<s1, s8>>));
}


// =========================================================================
// size_greater: largest sizeof first
// =========================================================================

TEST(SortTest, SizeGreater_FourDistinctSizes) {
    // s1=1, s2=2, s4=4, s8=8
    // Expected descending: s8, s4, s2, s1
    EXPECT_TRUE((std::is_same_v<
        sort_t<size_greater, s1, s2, s4, s8>,
        typelist<s8, s4, s2, s1>
    >));
    // Already reversed input
    EXPECT_TRUE((std::is_same_v<
        sort_t<size_greater, s8, s4, s2, s1>,
        typelist<s8, s4, s2, s1>
    >));
    // Mixed input
    EXPECT_TRUE((std::is_same_v<
        sort_t<size_greater, s4, s8, s1, s2>,
        typelist<s8, s4, s2, s1>
    >));
}

TEST(SortTest, SizeGreater_Stability_EqualSizes) {
    // A1 and A2 both have sizeof=4. Input order A1,A2 must be preserved.
    EXPECT_TRUE((std::is_same_v<
        sort_t<size_greater, A1, A2>,
        typelist<A1, A2>
    >));
    // Reversed input: A2 was first, A2 must remain first.
    EXPECT_TRUE((std::is_same_v<
        sort_t<size_greater, A2, A1>,
        typelist<A2, A1>
    >));
    // B1 and B2 mixed with A1 and A2: Bs (size 8) before As (size 4),
    // B1 before B2 (B1 was first in the B group), A1 before A2 (A1 was first).
    EXPECT_TRUE((std::is_same_v<
        sort_t<size_greater, A1, B1, A2, B2>,
        typelist<B1, B2, A1, A2>
    >));
    // Reverse the B pair: B2 before B1 in input -> B2 before B1 in output.
    EXPECT_TRUE((std::is_same_v<
        sort_t<size_greater, A1, B2, A2, B1>,
        typelist<B2, B1, A1, A2>
    >));
}

TEST(SortTest, SizeGreater_EightElements) {
    // Fully reversed input; merge sort must handle deeper recursion correctly.
    EXPECT_TRUE((std::is_same_v<
        sort_t<size_greater, s1, s1, s2, s2, s4, s4, s8, s8>,
        typelist<s8, s8, s4, s4, s2, s2, s1, s1>
    >));
}


// =========================================================================
// size_less: smallest sizeof first
// =========================================================================

TEST(SortTest, SizeLess_FourDistinctSizes) {
    EXPECT_TRUE((std::is_same_v<
        sort_t<size_less, s8, s4, s2, s1>,
        typelist<s1, s2, s4, s8>
    >));
    EXPECT_TRUE((std::is_same_v<
        sort_t<size_less, s1, s2, s4, s8>,
        typelist<s1, s2, s4, s8>
    >));
    EXPECT_TRUE((std::is_same_v<
        sort_t<size_less, s4, s1, s8, s2>,
        typelist<s1, s2, s4, s8>
    >));
}

TEST(SortTest, SizeLess_Stability_EqualSizes) {
    EXPECT_TRUE((std::is_same_v<
        sort_t<size_less, A1, A2>,
        typelist<A1, A2>
    >));
    EXPECT_TRUE((std::is_same_v<
        sort_t<size_less, A2, A1>,
        typelist<A2, A1>
    >));
}


// =========================================================================
// align_greater / align_less
// =========================================================================

TEST(SortTest, AlignGreater_FourDistinct) {
    // s1 align=1, s2 align=2, s4 align=4, s8 align=8
    EXPECT_TRUE((std::is_same_v<
        sort_t<align_greater, s1, s2, s4, s8>,
        typelist<s8, s4, s2, s1>
    >));
    EXPECT_TRUE((std::is_same_v<
        sort_t<align_greater, s4, s8, s1, s2>,
        typelist<s8, s4, s2, s1>
    >));
}

TEST(SortTest, AlignLess_FourDistinct) {
    EXPECT_TRUE((std::is_same_v<
        sort_t<align_less, s8, s4, s2, s1>,
        typelist<s1, s2, s4, s8>
    >));
    EXPECT_TRUE((std::is_same_v<
        sort_t<align_less, s4, s1, s8, s2>,
        typelist<s1, s2, s4, s8>
    >));
}


// =========================================================================
// typelist<Ts...> input specialization
// =========================================================================

TEST(SortTest, TypelistInput_EquivalentToBarePack) {
    using via_pack = sort_t<size_greater, s1, s8, s2, s4>;
    using via_list = sort_t<size_greater, typelist<s1, s8, s2, s4>>;
    EXPECT_TRUE((std::is_same_v<via_pack, via_list>));
}

TEST(SortTest, TypelistInput_Empty) {
    EXPECT_TRUE((std::is_same_v<sort_t<size_greater, typelist<>>, typelist<>>));
}

TEST(SortTest, TypelistInput_Singleton) {
    EXPECT_TRUE((std::is_same_v<sort_t<size_greater, typelist<s4>>, typelist<s4>>));
}


// =========================================================================
// Struct form: sort<Cmp, Ts...>::type
// =========================================================================

TEST(SortTest, StructForm_BarePackAndTypelist) {
    using from_pack = sort<size_greater, s1, s8, s4>::type;
    using from_list = sort<size_greater, typelist<s1, s8, s4>>::type;
    using expected  = typelist<s8, s4, s1>;
    EXPECT_TRUE((std::is_same_v<from_pack, expected>));
    EXPECT_TRUE((std::is_same_v<from_list, expected>));
}


// =========================================================================
// Custom comparator
// =========================================================================

TEST(SortTest, CustomComparator_PreferTrivial) {
    // Trivial types (s4, s2) must appear before NonTrivial.
    // Among equivalents (both trivial, or both non-trivial), input order holds.
    EXPECT_TRUE((std::is_same_v<
        sort_t<prefer_trivial, NonTrivial, s4, s2>,
        typelist<s4, s2, NonTrivial>
    >));
    EXPECT_TRUE((std::is_same_v<
        sort_t<prefer_trivial, s4, NonTrivial, s2>,
        typelist<s4, s2, NonTrivial>
    >));
    // Already in preferred order: trivial first.
    EXPECT_TRUE((std::is_same_v<
        sort_t<prefer_trivial, s4, s2, NonTrivial>,
        typelist<s4, s2, NonTrivial>
    >));
}

TEST(SortTest, CustomComparator_AllEquivalent_PreservesOrder) {
    // prefer_trivial<T,U> is false when both T and U are trivial, making them all
    // equivalent. The sort must be a pure no-op and preserve input order exactly.
    EXPECT_TRUE((std::is_same_v<
        sort_t<prefer_trivial, s1, s2, s4>,
        typelist<s1, s2, s4>
    >));
    EXPECT_TRUE((std::is_same_v<
        sort_t<prefer_trivial, s4, s1, s2>,
        typelist<s4, s1, s2>
    >));
}
