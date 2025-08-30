#include <etools/meta/traits.hpp>
#include <gtest/gtest.h>

// --- Helper Types for Tests ---
struct Foo {};
struct Bar {};
struct Baz {};

enum class Color : std::uint8_t {
    RED = 0,
    GREEN = 1,
    BLUE = 2
};

enum class Status {
    ACTIVE = 10,
    INACTIVE = 20,
};

struct HasStaticMember {
    static constexpr int value = 42;
};

struct HasDifferentStaticMember {
    static constexpr double value = 3.14;
};

struct HasStaticMemberButNotConstexpr {
    static int value;
};
int HasStaticMemberButNotConstexpr::value = 10;

// An extractor for the 'member' trait.
template<typename T>
struct MemberExtractor {
    static constexpr auto value = T::value;
};


// --- Test Suite ---

TEST(TraitsTest, IsDistinct_Positive) {
    // An empty pack should be distinct.
    EXPECT_TRUE((etools::meta::is_distinct_v<>));
    // A single type should be distinct.
    EXPECT_TRUE((etools::meta::is_distinct_v<int>));
    // A pack with unique types should be distinct.
    EXPECT_TRUE((etools::meta::is_distinct_v<int, double, char>));
    EXPECT_TRUE((etools::meta::is_distinct_v<Foo, Bar, Baz>));
}

TEST(TraitsTest, IsDistinct_Negative) {
    // A pack with duplicate types should not be distinct.
    EXPECT_FALSE((etools::meta::is_distinct_v<int, double, int>));
    EXPECT_FALSE((etools::meta::is_distinct_v<Foo, Bar, Foo>));
    EXPECT_FALSE((etools::meta::is_distinct_v<Foo, Foo>));
}

TEST(TraitsTest, UnderlyingV_Positive) {
    // Test with a standard enum class.
    EXPECT_EQ(etools::meta::underlying_v(Color::RED), 0);
    EXPECT_EQ(etools::meta::underlying_v(Color::BLUE), 2);
    // Test with an enum class that has an explicit underlying type.
    EXPECT_EQ(etools::meta::underlying_v(Status::ACTIVE), 10);
    EXPECT_EQ(etools::meta::underlying_v(Status::INACTIVE), 20);
    // Check if the underlying type is correct.
    EXPECT_TRUE((std::is_same_v<decltype(etools::meta::underlying_v(Color::RED)), std::uint8_t>));
    EXPECT_TRUE((std::is_same_v<decltype(etools::meta::underlying_v(Status::ACTIVE)), int>));
}

TEST(TraitsTest, AlwaysFalseV_Positive) {
    // always_false_v should always be false.
    EXPECT_FALSE((etools::meta::always_false_v<int>));
    EXPECT_FALSE((etools::meta::always_false_v<Foo>));
}

TEST(TraitsTest, TypeIdentity_Positive) {
    // `type_identity` should return the same type.
    EXPECT_TRUE((std::is_same_v<etools::meta::type_identity<int>::type, int>));
    EXPECT_TRUE((std::is_same_v<etools::meta::type_identity<const char*>::type, const char*>));
    EXPECT_TRUE((std::is_same_v<etools::meta::type_identity<Foo>::type, Foo>));
}

TEST(TraitsTest, TypeIdentityT_Positive) {
    // `type_identity_t` should be a convenient alias to the same type.
    EXPECT_TRUE((std::is_same_v<etools::meta::type_identity_t<int>, int>));
    EXPECT_TRUE((std::is_same_v<etools::meta::type_identity_t<const char*>, const char*>));
    EXPECT_TRUE((std::is_same_v<etools::meta::type_identity_t<Foo>, Foo>));
}

TEST(TraitsTest, Member_Positive) {
    // Check that it works for a single type.
    EXPECT_TRUE((std::is_same_v<etools::meta::member_t<MemberExtractor, HasStaticMember>, int>));
    // Check that it works for multiple types with the same member type.
    EXPECT_TRUE((std::is_same_v<etools::meta::member_t<MemberExtractor, HasStaticMember, HasStaticMember>, int>));
}

TEST(TraitsTest, Member_Negative) {
    // This test case would fail to compile because the member types differ.
    // Uncommenting the next line will produce a compile-time error:
    // static_assert(std::is_same_v<etools::meta::member_t<MemberExtractor, HasStaticMember, HasDifferentStaticMember>, int>);
}

TEST(TraitsTest, Nth_Positive) {
    // Get the first type.
    EXPECT_TRUE((std::is_same_v<etools::meta::nth<0, int, double, char>::type, int>));
    // Get a middle type.
    EXPECT_TRUE((std::is_same_v<etools::meta::nth<1, int, double, char>::type, double>));
    // Get the last type.
    EXPECT_TRUE((std::is_same_v<etools::meta::nth<2, int, double, char>::type, char>));
}

TEST(TraitsTest, Nth_Preserves_Qualifiers){
    EXPECT_TRUE((std::is_same_v<etools::meta::nth_t<0, int&, double, char>, int&>));
    EXPECT_TRUE((std::is_same_v<etools::meta::nth_t<0, int&&, double, char>, int&&>));
    EXPECT_TRUE((std::is_same_v<etools::meta::nth_t<2, int&, double, char&>, char&>));
    EXPECT_TRUE((std::is_same_v<etools::meta::nth_t<1, int&, double, char&>, double>));
    EXPECT_TRUE((std::is_same_v<etools::meta::nth_t<1, int&, const double, char&>, const double>));
    EXPECT_TRUE((std::is_same_v<etools::meta::nth_t<1, int&, double*, char&>, double*>));
    EXPECT_TRUE((std::is_same_v<etools::meta::nth_t<1, int&, const double*, char&>, const double*>));
    EXPECT_TRUE((std::is_same_v<etools::meta::nth_t<1, int&, const double&, char>, const double&>));
    EXPECT_TRUE((std::is_same_v<etools::meta::nth_t<1, int&, const double&&, char&>, const double&&>));
}
TEST(TraitsTest, Nth_Negative) {
    // An index out of bounds will cause a static_assert failure.
    // Uncommenting the next line will produce a compile-time error:
    // using OutOfBoundsType = etools::meta::nth<3, int, double, char>::type;
}

TEST(TraitsTest, SmallestUintT_Positive) {
    // A value that fits in uint8_t.
    EXPECT_TRUE((std::is_same_v<etools::meta::smallest_uint_t<100>, std::uint8_t>));
    // A value that fits in uint16_t.
    EXPECT_TRUE((std::is_same_v<etools::meta::smallest_uint_t<60000>, std::uint16_t>));
    // A value that fits in uint32_t.
    EXPECT_TRUE((std::is_same_v<etools::meta::smallest_uint_t<3000000000>, std::uint32_t>));
    // A value that fits in uint64_t.
    EXPECT_TRUE((std::is_same_v<etools::meta::smallest_uint_t<std::numeric_limits<std::uint64_t>::max()>, std::uint64_t>));
}

TEST(TraitsTest, SmallestUintT_Negative) {
    // A value larger than uint64_t::max() will cause a compile-time error.
    // Uncommenting the next line will produce a compile-time error:
    // using TooLarge = etools::meta::smallest_uint_t<std::numeric_limits<std::uint64_t>::max() + 1>;
}

TEST(TraitsTest, AddConstIf_Positive) {
    // The condition is true, so a const qualifier is added.
    EXPECT_TRUE((std::is_same_v<etools::meta::add_const_if_t<int, true>, const int>));
    EXPECT_TRUE((std::is_same_v<etools::meta::add_const_if_t<Foo, true>, const Foo>));
}

TEST(TraitsTest, AddConstIf_Negative) {
    // The condition is false, so no const qualifier is added.
    EXPECT_TRUE((std::is_same_v<etools::meta::add_const_if_t<int, false>, int>));
    // The type is already const, so the qualifier should not be added again.
    EXPECT_TRUE((std::is_same_v<etools::meta::add_const_if_t<const int, true>, const int>));
    EXPECT_TRUE((std::is_same_v<etools::meta::add_const_if_t<const int, false>, const int>));
}
