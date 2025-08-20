#include <gtest/gtest.h>
#include <etools/hashing/llut.hpp>


using etools::hashing::llut;
using etools::hashing::details::llut_impl;

// Convenience aliases for the test key-set {2,5,7}.
using LUT_U8   = llut<std::uint8_t>;
using IMPL_U8  = llut_impl<std::uint8_t, 2, 5, 7>;

// ---------------- Compile-time tests (fire at compile step) ----------------

static_assert(IMPL_U8::keys() == 3,                 "keys() must equal number of Keys");
static_assert(IMPL_U8::not_found() == 3,            "sentinel equals keys()");
static_assert(IMPL_U8::size() == 8,                 "size() == max_key + 1 (7+1)");

// Pull the canonical table as a constexpr reference:
constexpr const auto& Tc = LUT_U8::generate<2,5,7>();

static_assert(Tc.keys() == 3, "keys() == 3");
static_assert(Tc.size()  == 8, "size() == 8");

static_assert(Tc(2) == 0,                 "2 -> 0");
static_assert(Tc(5) == 1,                 "5 -> 1");
static_assert(Tc(7) == 2,                 "7 -> 2");
static_assert(Tc(0) == Tc.not_found(),    "0 absent");
static_assert(Tc(9) == Tc.not_found(),    "9 out of range");

// Uncomment to verify compile-time failure for duplicate keys:
// using BAD = llut_impl<std::uint8_t, 1, 1>;
// constexpr auto bad_tbl = BAD{}; // should fail (distinctness)

// ---------------- Runtime tests (GoogleTest) ----------------

TEST(llut_runtime, structure_constants) {
    const auto& T = LUT_U8::generate<2,5,7>();
    EXPECT_EQ(IMPL_U8::keys(), 3u);
    EXPECT_EQ(IMPL_U8::not_found(), 3u);
    EXPECT_EQ(IMPL_U8::size(), 8u);
    EXPECT_EQ(T.keys(), 3u);
    EXPECT_EQ(T.size(), 8u);
}

TEST(llut_runtime, operator_lookup) {
    const auto& T = LUT_U8::generate<2,5,7>();
    EXPECT_EQ(T(0), T.not_found());
    EXPECT_EQ(T(1), T.not_found());
    EXPECT_EQ(T(2), 0u);
    EXPECT_EQ(T(3), T.not_found());
    EXPECT_EQ(T(4), T.not_found());
    EXPECT_EQ(T(5), 1u);
    EXPECT_EQ(T(6), T.not_found());
    EXPECT_EQ(T(7), 2u);
    EXPECT_EQ(T(8), T.not_found());
    EXPECT_EQ(T(100), T.not_found());
}

TEST(llut_runtime, singleton_identity) {
    const auto& A = LUT_U8::generate<2,5,7>();
    const auto& B = LUT_U8::generate<2,5,7>();
    EXPECT_EQ(&A, &B); // same address -> same object
}