// tests/meta/test_tuple.cpp
// SPDX-License-Identifier: MIT
//
// `meta::tuple` exists for one reason - to keep instantiation depth O(1) where
// `std::tuple`'s is O(N) - so the tests are in two halves. The first is ordinary
// container behaviour, which must hold whatever the layout is. The second pins
// the properties that *are* the point, so a later "simplification" back onto a
// recursive layout fails here rather than silently in a build time.

#include <gtest/gtest.h>

#include <etools/meta/tuple.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace {

    using etools::meta::tuple;
    using etools::meta::get;
    using etools::meta::for_each;

    /// Not default-constructible: naming a tuple over it must still be legal.
    struct no_default {
        int v;
        explicit no_default(int x) noexcept : v{x} {}
    };

    /// Counts its own destruction, to prove elements are really destroyed.
    struct counted {
        static inline int live = 0;
        counted() noexcept { ++live; }
        counted(const counted&) noexcept { ++live; }
        ~counted() noexcept { --live; }
    };

} // namespace

// ------------------------------------------------------------------ behaviour

TEST(MetaTuple, StoresAndReturnsEachElement)
{
    tuple<int, double, char> t{};
    get<0>(t) = 7;
    get<1>(t) = 2.5;
    get<2>(t) = 'x';

    EXPECT_EQ(get<0>(t), 7);
    EXPECT_DOUBLE_EQ(get<1>(t), 2.5);
    EXPECT_EQ(get<2>(t), 'x');
}

TEST(MetaTuple, ValueInitialisationZeroesScalars)
{
    // `leaf` deliberately has no default member initialiser (see the header), so
    // this checks that value-init still reaches the members through the bases.
    tuple<int, double> t{};
    EXPECT_EQ(get<0>(t), 0);
    EXPECT_DOUBLE_EQ(get<1>(t), 0.0);
}

TEST(MetaTuple, RepeatedTypesAreDistinctElements)
{
    // The index in `leaf<I, T>` is what keeps two `int`s from being one base.
    tuple<int, int, int> t{};
    get<0>(t) = 1;
    get<1>(t) = 2;
    get<2>(t) = 3;

    EXPECT_EQ(get<0>(t), 1);
    EXPECT_EQ(get<1>(t), 2);
    EXPECT_EQ(get<2>(t), 3);
}

TEST(MetaTuple, ConstAccessYieldsConstReference)
{
    const tuple<int> t{};
    static_assert(std::is_same_v<decltype(get<0>(t)), const int&>);
    EXPECT_EQ(get<0>(t), 0);
}

TEST(MetaTuple, AccessReturnsAReferenceNotACopy)
{
    tuple<int> t{};
    get<0>(t) = 42;
    int& ref = get<0>(t);
    ref = 43;
    EXPECT_EQ(get<0>(t), 43) << "get must alias the stored element";
}

TEST(MetaTuple, SizeReportsTheElementCount)
{
    static_assert(tuple<>::size == 0);
    static_assert(tuple<int>::size == 1);
    static_assert(tuple<int, char, double>::size == 3);
}

TEST(MetaTuple, HoldsNonDefaultConstructibleTypes)
{
    // The regression this guards: a default member initialiser in `leaf` made
    // *naming* the type ill-formed, not merely default-constructing it.
    using t_t = tuple<no_default>;
    static_assert(not std::is_default_constructible_v<t_t>);

    t_t* never_built = nullptr;   // naming and pointing at it must compile
    EXPECT_EQ(never_built, nullptr);
}

TEST(MetaTuple, DestroysEveryElement)
{
    ASSERT_EQ(counted::live, 0);
    {
        tuple<counted, counted, counted> t{};
        EXPECT_EQ(counted::live, 3);
    }
    EXPECT_EQ(counted::live, 0) << "every element must be destroyed with the tuple";
}

// ------------------------------------------------------------------- for_each

TEST(MetaTuple, ForEachVisitsEveryElementInOrder)
{
    tuple<int, int, int> t{};
    get<0>(t) = 1;
    get<1>(t) = 2;
    get<2>(t) = 3;

    std::string seen;
    for_each(t, [&seen](auto& v) { seen += std::to_string(v); });
    EXPECT_EQ(seen, "123");
}

TEST(MetaTuple, ForEachCanMutate)
{
    tuple<int, int> t{};
    for_each(t, [](auto& v) { v = 5; });
    EXPECT_EQ(get<0>(t), 5);
    EXPECT_EQ(get<1>(t), 5);
}

TEST(MetaTuple, ForEachOnAnEmptyTupleIsANoOp)
{
    tuple<> t{};
    int calls = 0;
    for_each(t, [&calls](auto&) { ++calls; });
    EXPECT_EQ(calls, 0);
}

// -------------------------------------------------- the properties it exists for

TEST(MetaTuple, InstantiationDepthDoesNotGrowWithElementCount)
{
    // Every leaf is a *direct* base of the tuple, so the hierarchy is two levels
    // deep whatever N is. If someone reintroduces a recursive layout,
    // `leaf<0, T>` stops being a direct base of the 8-element case and this
    // fails - which is the whole contract.
    using small_t = tuple<int>;
    using large_t = tuple<int, int, int, int, int, int, int, int>;

    static_assert(std::is_base_of_v<etools::meta::detail::leaf<0, int>, small_t>);
    static_assert(std::is_base_of_v<etools::meta::detail::leaf<0, int>, large_t>);
    static_assert(std::is_base_of_v<etools::meta::detail::leaf<7, int>, large_t>);
}

TEST(MetaTuple, ScalesPastTheRecursiveTemplateDepthLimit)
{
    // A recursive tuple of this width exceeds GCC's default -ftemplate-depth of
    // 900 and does not compile at all. That this test builds is the assertion;
    // the runtime checks just keep it honest.
    constexpr std::size_t width = 1200;

    // 1200 elements, expressed without writing 1200 type names.
    auto make = [](auto... is) {
        return tuple<decltype(static_cast<void>(is), std::size_t{})...>{};
    };
    auto wide = [&make]<std::size_t... Is>(std::index_sequence<Is...>) {
        return make(std::integral_constant<std::size_t, Is>{}...);
    }(std::make_index_sequence<width>{});

    static_assert(decltype(wide)::size == width);

    get<0>(wide) = 1;
    get<width - 1>(wide) = 2;
    EXPECT_EQ(get<0>(wide), 1u);
    EXPECT_EQ(get<width - 1>(wide), 2u);

    std::size_t visited = 0;
    for_each(wide, [&visited](auto&) { ++visited; });
    EXPECT_EQ(visited, width);
}

TEST(MetaTuple, HoldsTheShapeDispatchFactoryUses)
{
    // The actual client: one array of optionals per registered type.
    using slots_t = tuple<std::array<std::optional<int>, 2>,
                          std::array<std::optional<char>, 1>>;
    slots_t slots{};

    EXPECT_FALSE(get<0>(slots)[0].has_value());
    get<0>(slots)[0].emplace(9);
    EXPECT_TRUE(get<0>(slots)[0].has_value());
    EXPECT_EQ(*get<0>(slots)[0], 9);

    bool all_empty = true;
    for_each(slots, [&all_empty](const auto& arr) {
        for (const auto& opt : arr) all_empty = all_empty and not opt.has_value();
    });
    EXPECT_FALSE(all_empty) << "for_each must observe the element that was filled";
}
