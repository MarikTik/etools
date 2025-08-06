#include <gtest/gtest.h>
#include <type_traits> // For std::is_same_v
#include <cstddef>     // For std::size_t
#include <etools/meta/typeset.hpp> // The header file containing the typeset class

// --- Helper Types for Tests ---
struct TypeA {};
struct TypeB {};
struct TypeC {};
struct UnusedType {};


// --- Test Suite for the typeset class ---

TEST(TypesetTest, InitialStateAndSetReset_Positive) {
    // Instantiate a typeset with three distinct types.
    etools::meta::typeset<TypeA, TypeB, TypeC> my_typeset;

    // Initially, all flags should be false.
    EXPECT_FALSE((my_typeset.test<TypeA>()));
    EXPECT_FALSE((my_typeset.test<TypeB>()));
    EXPECT_FALSE((my_typeset.test<TypeC>()));

    // Set the flag for TypeA and verify.
    my_typeset.set<TypeA>();
    EXPECT_TRUE((my_typeset.test<TypeA>()));
    EXPECT_FALSE((my_typeset.test<TypeB>()));
    EXPECT_FALSE((my_typeset.test<TypeC>()));

    // Set the flag for TypeC and verify. TypeA should remain set.
    my_typeset.set<TypeC>();
    EXPECT_TRUE((my_typeset.test<TypeA>()));
    EXPECT_FALSE((my_typeset.test<TypeB>()));
    EXPECT_TRUE((my_typeset.test<TypeC>()));

    // Reset the flag for TypeA and verify.
    my_typeset.reset<TypeA>();
    EXPECT_FALSE((my_typeset.test<TypeA>()));
    EXPECT_FALSE((my_typeset.test<TypeB>()));
    EXPECT_TRUE((my_typeset.test<TypeC>()));

    // Reset the flag for TypeC and verify.
    my_typeset.reset<TypeC>();
    EXPECT_FALSE((my_typeset.test<TypeA>()));
    EXPECT_FALSE((my_typeset.test<TypeB>()));
    EXPECT_FALSE((my_typeset.test<TypeC>()));
}

TEST(TypesetTest, SetAllAndClearAll_Positive) {
    etools::meta::typeset<TypeA, TypeB, TypeC> my_typeset;
    
    // Set all flags by calling set on each type.
    my_typeset.set<TypeA>();
    my_typeset.set<TypeB>();
    my_typeset.set<TypeC>();

    // Verify all flags are set.
    EXPECT_TRUE((my_typeset.test<TypeA>()));
    EXPECT_TRUE((my_typeset.test<TypeB>()));
    EXPECT_TRUE((my_typeset.test<TypeC>()));

    // Reset all flags.
    my_typeset.reset<TypeA>();
    my_typeset.reset<TypeB>();
    my_typeset.reset<TypeC>();

    // Verify all flags are reset.
    EXPECT_FALSE((my_typeset.test<TypeA>()));
    EXPECT_FALSE((my_typeset.test<TypeB>()));
    EXPECT_FALSE((my_typeset.test<TypeC>()));
}

TEST(TypesetTest, EmptyTypeset_Positive) {
    // An empty typeset should compile and work correctly.
    etools::meta::typeset<> my_typeset;
    // An empty typeset should have a bitset of size 0.
    EXPECT_EQ(sizeof(my_typeset), sizeof(std::bitset<0>));
}


// --- Negative test cases (compile-time errors) ---
/*
// This test case is commented out because it is designed to cause a compile-time error.
// The `typeset` class has a static_assert to ensure types are distinct.
TEST(TypesetTest, DuplicateTypes_Negative) {
    // The following line should fail to compile due to the static_assert in typeset.
    etools::meta::typeset<TypeA, TypeB, TypeA> invalid_typeset;
}
*/

/*
// This test case is commented out because it is designed to cause a compile-time error.
// The `type_to_index` helper struct should not find a type that isn't in the pack,
// leading to a compile-time assertion failure.
TEST(TypesetTest, AccessingInvalidType_Negative) {
    etools::meta::typeset<TypeA, TypeB> my_typeset;
    // The following line should fail to compile because UnusedType is not in the typeset.
    my_typeset.test<UnusedType>();
}
*/
