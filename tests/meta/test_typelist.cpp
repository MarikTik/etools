#include <etools/meta/typelist.hpp>
#include <gtest/gtest.h>
#include <tuple>
#include <type_traits>

using namespace etools::meta;

// ---- Lists under test ----

using none  = typelist<>;
using one   = typelist<int>;
using three = typelist<int, char, bool>;
using dupes = typelist<int, int, char>;

// ---- Targets for apply ----

template<typename...> struct manager {};
template<typename...> struct other_manager {};

// ---- A predicate of our own, to prove any_of/all_of are not tied to <type_traits> ----

struct tagged   { static constexpr bool marked = true; };
struct untagged { static constexpr bool marked = false; };

template<typename T>
struct is_marked : std::bool_constant<T::marked> {};


// =========================================================================
// The type itself stays a zero-cost tag
// =========================================================================

TEST(typelist, remains_empty_and_trivial) {
    static_assert(std::is_empty_v<three>);
    static_assert(std::is_trivially_default_constructible_v<three>);
    static_assert(std::is_aggregate_v<three>);
    static_assert(sizeof(three) == sizeof(typelist<>));

    EXPECT_TRUE(std::is_empty_v<three>);
}


// =========================================================================
// size / is_empty
// =========================================================================

TEST(typelist, size_counts_elements) {
    EXPECT_EQ(none::size(), 0u);
    EXPECT_EQ(one::size(), 1u);
    EXPECT_EQ(three::size(), 3u);
}

TEST(typelist, size_counts_repeats_separately) {
    EXPECT_EQ(dupes::size(), 3u);
}

TEST(typelist, is_empty_only_for_the_empty_list) {
    static_assert(none::is_empty());
    static_assert(not one::is_empty());
    static_assert(not three::is_empty());

    EXPECT_TRUE(none::is_empty());
    EXPECT_FALSE(three::is_empty());
}


// =========================================================================
// contains
// =========================================================================

TEST(typelist, contains_finds_present_types) {
    EXPECT_TRUE(three::contains<int>());
    EXPECT_TRUE(three::contains<char>());
    EXPECT_TRUE(three::contains<bool>());
}

TEST(typelist, contains_rejects_absent_types) {
    EXPECT_FALSE(three::contains<double>());
    EXPECT_FALSE(one::contains<char>());
}

TEST(typelist, empty_list_contains_nothing) {
    static_assert(not none::contains<int>());
    EXPECT_FALSE(none::contains<int>());
}

TEST(typelist, contains_distinguishes_qualified_types) {
    EXPECT_FALSE(one::contains<const int>());
    EXPECT_FALSE(one::contains<int&>());
    EXPECT_TRUE(typelist<const int>::contains<const int>());
}

TEST(typelist, contains_is_usable_without_an_object) {
    // The syntax this interface exists for: no instance, no helper alias.
    static_assert(typelist<int, bool, float>::contains<bool>());
    EXPECT_TRUE((typelist<int, bool, float>::contains<bool>()));
}


// =========================================================================
// at
// =========================================================================

TEST(typelist, at_retrieves_by_index) {
    EXPECT_TRUE((std::is_same_v<three::at<0>, int>));
    EXPECT_TRUE((std::is_same_v<three::at<1>, char>));
    EXPECT_TRUE((std::is_same_v<three::at<2>, bool>));
}

TEST(typelist, at_handles_a_singleton) {
    static_assert(std::is_same_v<one::at<0>, int>);
    EXPECT_TRUE((std::is_same_v<one::at<0>, int>));
}

TEST(typelist, at_preserves_qualifiers) {
    using quals = typelist<const int, int&, volatile char>;

    EXPECT_TRUE((std::is_same_v<quals::at<0>, const int>));
    EXPECT_TRUE((std::is_same_v<quals::at<1>, int&>));
    EXPECT_TRUE((std::is_same_v<quals::at<2>, volatile char>));
}

TEST(typelist, at_reaches_repeated_elements_independently) {
    EXPECT_TRUE((std::is_same_v<dupes::at<0>, int>));
    EXPECT_TRUE((std::is_same_v<dupes::at<1>, int>));
    EXPECT_TRUE((std::is_same_v<dupes::at<2>, char>));
}

TEST(typelist, at_agrees_with_nth_t_on_the_underlying_pack) {
    EXPECT_TRUE((std::is_same_v<three::at<1>, nth_t<1, int, char, bool>>));
}

TEST(typelist, last_element_is_at_size_minus_one) {
    EXPECT_TRUE((std::is_same_v<three::at<three::size() - 1>, bool>));
}


// =========================================================================
// apply
// =========================================================================

TEST(typelist, apply_instantiates_target_with_list_types) {
    EXPECT_TRUE((std::is_same_v<three::apply<manager>, manager<int, char, bool>>));
    EXPECT_TRUE((std::is_same_v<typelist<int, char>::apply<std::tuple>,
                                std::tuple<int, char>>));
}

TEST(typelist, apply_preserves_element_order) {
    EXPECT_FALSE((std::is_same_v<typelist<int, char>::apply<manager>,
                                 manager<char, int>>));
}

TEST(typelist, apply_on_empty_list_yields_empty_instantiation) {
    static_assert(std::is_same_v<none::apply<manager>, manager<>>);
    EXPECT_TRUE((std::is_same_v<none::apply<manager>, manager<>>));
}

TEST(typelist, apply_to_different_targets_yields_different_types) {
    EXPECT_FALSE((std::is_same_v<three::apply<manager>, three::apply<other_manager>>));
}


// =========================================================================
// any_of / all_of
// =========================================================================

TEST(typelist, any_of_detects_a_single_satisfying_element) {
    using nums = typelist<int, double, char>;

    EXPECT_TRUE(nums::any_of<std::is_floating_point>());
    EXPECT_TRUE(nums::any_of<std::is_arithmetic>());
}

TEST(typelist, any_of_false_when_nothing_satisfies) {
    using nums = typelist<int, double, char>;

    EXPECT_FALSE(nums::any_of<std::is_pointer>());
}

TEST(typelist, any_of_false_on_empty_list) {
    static_assert(not none::any_of<std::is_floating_point>());
    EXPECT_FALSE(none::any_of<std::is_floating_point>());
}

TEST(typelist, all_of_requires_every_element) {
    using nums = typelist<int, double, char>;

    EXPECT_TRUE(nums::all_of<std::is_arithmetic>());
    EXPECT_FALSE(nums::all_of<std::is_floating_point>());
}

TEST(typelist, all_of_vacuously_true_on_empty_list) {
    static_assert(none::all_of<std::is_floating_point>());
    static_assert(none::all_of<std::is_pointer>());
    EXPECT_TRUE(none::all_of<std::is_floating_point>());
}

TEST(typelist, all_of_false_when_the_last_element_fails) {
    EXPECT_FALSE((typelist<double, float, int>::all_of<std::is_floating_point>()));
}

TEST(typelist, predicates_may_be_user_defined) {
    EXPECT_TRUE((typelist<tagged, tagged>::all_of<is_marked>()));
    EXPECT_TRUE((typelist<untagged, tagged>::any_of<is_marked>()));
    EXPECT_FALSE((typelist<untagged, tagged>::all_of<is_marked>()));
    EXPECT_FALSE((typelist<untagged, untagged>::any_of<is_marked>()));
}


// =========================================================================
// disjoint
// =========================================================================

TEST(typelist, disjoint_when_no_type_is_shared) {
    static_assert(typelist<int, char>::disjoint<typelist<bool, float>>());
    EXPECT_TRUE((typelist<int, char>::disjoint<typelist<bool, float>>()));
}

TEST(typelist, not_disjoint_when_a_type_is_shared) {
    EXPECT_FALSE((typelist<int, char>::disjoint<typelist<char>>()));
    EXPECT_FALSE((typelist<int>::disjoint<typelist<double, int>>()));
}

TEST(typelist, empty_list_is_disjoint_with_everything) {
    static_assert(none::disjoint<typelist<int>>());
    static_assert(one::disjoint<none>());
    static_assert(none::disjoint<none>());

    EXPECT_TRUE(none::disjoint<typelist<int>>());
    EXPECT_TRUE(one::disjoint<none>());
}

TEST(typelist, disjoint_is_symmetric) {
    EXPECT_EQ((typelist<int, char>::disjoint<typelist<char, bool>>()),
              (typelist<char, bool>::disjoint<typelist<int, char>>()));
}

TEST(typelist, disjoint_unaffected_by_repeats_within_a_list) {
    EXPECT_TRUE(dupes::disjoint<typelist<double>>());
    EXPECT_FALSE(dupes::disjoint<typelist<int>>());
}


// =========================================================================
// concat
// =========================================================================

TEST(typelist, concat_joins_two_lists_in_order) {
    EXPECT_TRUE((std::is_same_v<typelist<int>::concat<typelist<char, bool>>,
                                typelist<int, char, bool>>));
}

TEST(typelist, concat_joins_more_than_two_lists) {
    EXPECT_TRUE((std::is_same_v<
        typelist<int>::concat<typelist<char>, typelist<bool>, typelist<float>>,
        typelist<int, char, bool, float>>));
}

TEST(typelist, concat_handles_empty_operands) {
    EXPECT_TRUE((std::is_same_v<none::concat<typelist<int>>, typelist<int>>));
    EXPECT_TRUE((std::is_same_v<one::concat<none>, typelist<int>>));
    EXPECT_TRUE((std::is_same_v<none::concat<none>, none>));
}

TEST(typelist, concat_of_nothing_leaves_the_list_unchanged) {
    static_assert(std::is_same_v<three::concat<>, three>);
    EXPECT_TRUE((std::is_same_v<three::concat<>, three>));
}

TEST(typelist, concat_keeps_duplicates) {
    EXPECT_TRUE((std::is_same_v<one::concat<typelist<int>>, typelist<int, int>>));
}


// =========================================================================
// Use from a generic context - where the disambiguators are mandatory
// =========================================================================

// This is the shape a real consumer (e.g. a task-tier split) takes: the list
// arrives as a dependent template parameter, so every member access needs
// `template`, and every member alias additionally needs `typename`.
template<typename List>
struct consumer {
    static constexpr std::size_t count   = List::size();
    static constexpr bool        has_int = List::template contains<int>();
    static constexpr bool        numeric = List::template all_of<std::is_arithmetic>();

    using selected = typename List::template apply<manager>;
    using first    = typename List::template at<0>;
};

TEST(typelist, members_are_reachable_through_a_dependent_type) {
    using c = consumer<three>;

    EXPECT_EQ(c::count, 3u);
    EXPECT_TRUE(c::has_int);
    EXPECT_TRUE(c::numeric);
    EXPECT_TRUE((std::is_same_v<c::selected, manager<int, char, bool>>));
    EXPECT_TRUE((std::is_same_v<c::first, int>));
}


// =========================================================================
// Composition - the shapes real consumers build out of these
// =========================================================================

TEST(typelist, operations_compose) {
    using joined = typelist<int, char>::concat<typelist<bool>>;

    static_assert(joined::size() == 3);
    static_assert(joined::all_of<std::is_arithmetic>());
    static_assert(joined::contains<bool>());
    static_assert(std::is_same_v<joined::apply<manager>, manager<int, char, bool>>);

    EXPECT_EQ(joined::size(), 3u);
}

TEST(typelist, empty_list_selects_a_stand_in_target) {
    using selected = std::conditional_t<none::is_empty(),
                                        other_manager<>,
                                        none::apply<manager>>;

    EXPECT_TRUE((std::is_same_v<selected, other_manager<>>));
}
