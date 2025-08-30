// tests/hashing/test_distinct.cpp
#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <iterator>
#include <type_traits>
#include <limits>

// TODO: replace with your actual header that declares the four functions
// and their dependencies (mix_native, ceil_pow2, ...).
#include <etools/meta/utility.hpp>
using namespace etools::meta;

enum class E8 : std::uint8_t { A = 1, B = 200, C = 7 };
static_assert(tpack_max<E8, E8::A, E8::B, E8::C>() == E8::B);
static_assert(tpack_max<int,  1, 5, 2, 10, 7>() == 10, "int pack max");
static_assert(tpack_max<unsigned, 0u, 255u, 42u>() == 255u, "unsigned pack max");
static_assert(tpack_max<long long, -5LL, -2LL, -9LL>() == -2LL, "negative pack max");

TEST(TPackMax, RuntimeSanity) {
    // Also check it plays nicely with standard usage at runtime (no code-gen surprises)
    constexpr auto m1 = tpack_max<int, 3, 10, 6>();
    EXPECT_EQ(m1, 10);
}

// ===========================================================
// all_distinct_bitmap — ≤16‑bit keys, array-based, constexpr
// ===========================================================

TEST(AllDistinctBitmap, EmptyAndSingleton) {
    constexpr std::array<std::uint8_t, 0> a0{};
    constexpr std::array<std::uint16_t, 1> a1{{42}};

    static_assert(all_distinct_bitmap(a0), "empty is distinct");
    static_assert(all_distinct_bitmap(a1), "singleton is distinct");

    EXPECT_TRUE(all_distinct_bitmap(a0));
    EXPECT_TRUE(all_distinct_bitmap(a1));
}

TEST(AllDistinctBitmap, DistinctUint8_CompileAndRun) {
    constexpr std::array<std::uint8_t, 5> keys{{1, 5, 2, 10, 7}};
    static_assert(all_distinct_bitmap(keys), "uint8 distinct at constexpr");
    EXPECT_TRUE(all_distinct_bitmap(keys));
}

TEST(AllDistinctBitmap, DuplicateUint8_Run) {
    constexpr std::array<std::uint8_t, 6> dup{{1, 2, 3, 4, 5, 3}};
    // Avoid static_assert(false); check at runtime:
    EXPECT_FALSE(all_distinct_bitmap(dup));
}

TEST(AllDistinctBitmap, DistinctUint16_CompileAndRun) {
    constexpr std::array<std::uint16_t, 7> keys{{0, 17, 1024, 4096, 655, 123, 65530}};
    static_assert(all_distinct_bitmap(keys), "uint16 distinct at constexpr");
    EXPECT_TRUE(all_distinct_bitmap(keys));
}

TEST(AllDistinctBitmap, DuplicateUint16_Run) {
    constexpr std::array<std::uint16_t, 6> dup{{0, 17, 1024, 4096, 655, 1024}};
    EXPECT_FALSE(all_distinct_bitmap(dup));
}

// ======================================================
// all_distinct_probe — generic (32/64‑bit), array-based
// ======================================================

TEST(AllDistinctProbe, DistinctUint32_CompileAndRun) {
    constexpr std::array<std::uint32_t, 5> keys{{0xDEADBEEF, 7u, 42u, 9999u, 123456789u}};
    static_assert(all_distinct_probe(keys), "uint32 distinct at constexpr");
    EXPECT_TRUE(all_distinct_probe(keys));
}

TEST(AllDistinctProbe, DuplicateUint32_Run) {
    constexpr std::array<std::uint32_t, 6> dup{{7u, 42u, 7u, 9001u, 1u, 2u}};
    EXPECT_FALSE(all_distinct_probe(dup));
}

TEST(AllDistinctProbe, DistinctUint64_CompileAndRun) {
    constexpr std::array<std::uint64_t, 4> keys{{1ull, 3ull, 5ull, 7ull}};
    static_assert(all_distinct_probe(keys), "uint64 distinct at constexpr");
    EXPECT_TRUE(all_distinct_probe(keys));
}

TEST(AllDistinctProbe, DuplicateUint64_Run) {
    constexpr std::array<std::uint64_t, 4> dup{{9ull, 11ull, 11ull, 13ull}};
    EXPECT_FALSE(all_distinct_probe(dup));
}

// ==================================================
// all_distinct_fast — dispatcher, array-based tests
// ==================================================

TEST(AllDistinctFast, DispatchesToBitmapForUint8_CompileAndRun) {
    constexpr std::array<std::uint8_t, 4> keys{{0, 1, 2, 3}};
    static_assert(all_distinct_fast(keys), "fast distinct (bitmap path)");
    EXPECT_TRUE(all_distinct_fast(keys));
}

TEST(AllDistinctFast, DispatchesToProbeForUint32_CompileAndRun) {
    constexpr std::array<std::uint32_t, 4> keys{{10u, 20u, 30u, 40u}};
    static_assert(all_distinct_fast(keys), "fast distinct (probe path)");
    EXPECT_TRUE(all_distinct_fast(keys));
}

TEST(AllDistinctFast, DuplicateDetectsInBothPaths) {
    constexpr std::array<std::uint16_t, 5> small_dup{{1, 2, 3, 4, 2}}; // bitmap path
    constexpr std::array<std::uint32_t, 5> large_dup{{1, 2, 3, 4, 3}}; // probe path
    EXPECT_FALSE(all_distinct_fast(small_dup));
    EXPECT_FALSE(all_distinct_fast(large_dup));
}

// =====================
// Moderate “strain” run
// =====================

TEST(AllDistinct_Strains, Uint16_1kDistinct) {
    std::array<std::uint16_t, 1024> seq{};
    for (std::size_t i = 0; i < seq.size(); ++i) seq[i] = static_cast<std::uint16_t>(i);
    // bitmap path, should be very fast
    EXPECT_TRUE(all_distinct_fast(seq));
}

TEST(AllDistinct_Strains, Uint32_1kDistinct) {
    std::array<std::uint32_t, 1024> seq{};
    // spread values using a multiplicative step (distinct in 32-bit)
    for (std::size_t i = 0; i < seq.size(); ++i)
        seq[i] = static_cast<std::uint32_t>(i * 2654435761u);
    EXPECT_TRUE(all_distinct_fast(seq));
}

TEST(AllDistinct_Strains, Uint32_1kWithOneDuplicate) {
    std::array<std::uint32_t, 1024> seq{};
    for (std::size_t i = 0; i < seq.size(); ++i) seq[i] = static_cast<std::uint32_t>(i);
    seq[777] = seq[42]; // inject a duplicate
    EXPECT_FALSE(all_distinct_fast(seq));
}