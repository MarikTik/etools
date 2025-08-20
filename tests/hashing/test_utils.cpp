// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>
#include <type_traits>
#include <limits>
#include <array>
#include <etools/hashing/utils.hpp> 

using namespace etools::hashing;

// ------------------------------
// Compile-time sanity checks
// ------------------------------

// mix_* zero behavior is deterministic with your implementations.
static_assert(mix_u8 (uint8_t{0})  == uint8_t{0},  "mix_u8(0) must be 0");
static_assert(mix_u16(uint16_t{0}) == uint16_t{0}, "mix_u16(0) must be 0");
static_assert(mix_u32(uint32_t{0}) == uint32_t{0}, "mix_u32(0) must be 0");
static_assert(mix_u64(uint64_t{0}) == uint64_t{0}, "mix_u64(0) must be 0");

// mix_width and mix_native should be usable in constexpr context
constexpr uint32_t mw32_cx = mix_width<uint32_t>(uint8_t{0});
static_assert(mw32_cx == 0u, "mix_width<uint32_t>(0) constexpr ok");

constexpr std::size_t mn_cx = mix_native(uint16_t{0});
static_assert(mn_cx == std::size_t{0}, "mix_native(0) constexpr ok");

// ceil_pow2 constexpr checks
static_assert(ceil_pow2<uint32_t>(0) == 1u,  "ceil_pow2(0) == 1");
static_assert(ceil_pow2<uint32_t>(1) == 1u,  "ceil_pow2(1) == 1");
static_assert(ceil_pow2<uint32_t>(2) == 2u,  "ceil_pow2(2) == 2");
static_assert(ceil_pow2<uint32_t>(3) == 4u,  "ceil_pow2(3) == 4");
static_assert(ceil_pow2<uint32_t>(1024) == 1024u, "ceil_pow2(1024) == 1024");

// ceil_pow2_saturate constexpr checks (no wrap)
static_assert(ceil_pow2_saturate<uint8_t>(0) == uint8_t{1}, "sat(0)==1");
static_assert(ceil_pow2_saturate<uint8_t>(1) == uint8_t{1}, "sat(1)==1");
static_assert(ceil_pow2_saturate<uint8_t>(128) == uint8_t{128}, "sat(128)==128");
static_assert(ceil_pow2_saturate<uint8_t>(129) == uint8_t{128}, "sat(129)==128");

// bit_width / ceil_log2 constexpr checks
static_assert(bit_width<std::size_t>(uint8_t{0}) == std::size_t{0}, "bw(0)==0");
static_assert(bit_width<std::size_t>(uint8_t{1}) == std::size_t{1}, "bw(1)==1");
static_assert(bit_width<std::size_t>(uint8_t{2}) == std::size_t{2}, "bw(2)==2");
static_assert(bit_width<std::size_t>(uint8_t{3}) == std::size_t{2}, "bw(3)==2");
static_assert(ceil_log2<std::size_t>(uint8_t{0}) == std::size_t{0}, "cl2(0)==0");
static_assert(ceil_log2<std::size_t>(uint8_t{1}) == std::size_t{0}, "cl2(1)==0");
static_assert(ceil_log2<std::size_t>(uint8_t{2}) == std::size_t{1}, "cl2(2)==1");
static_assert(ceil_log2<std::size_t>(uint8_t{3}) == std::size_t{2}, "cl2(3)==2");
static_assert(ceil_log2<std::size_t>(uint8_t{129}) == std::size_t{8}, "cl2(129)==8");

// ------------------------------
// Runtime tests
// ------------------------------

TEST(Mixers, TypesAndBasicBehavior) {
    // Return types
    static_assert(std::is_same<decltype(mix_u8 (uint8_t{1})),  uint8_t>::value,  "mix_u8 type");
    static_assert(std::is_same<decltype(mix_u16(uint16_t{1})), uint16_t>::value, "mix_u16 type");
    static_assert(std::is_same<decltype(mix_u32(uint32_t{1})), uint32_t>::value, "mix_u32 type");
    static_assert(std::is_same<decltype(mix_u64(uint64_t{1})), uint64_t>::value, "mix_u64 type");

    // Non-identity for a handful of values (0 is a known fixed point)
    EXPECT_NE(mix_u8 (uint8_t{1}),  uint8_t{1});
    EXPECT_NE(mix_u16(uint16_t{1}), uint16_t{1});
    EXPECT_NE(mix_u32(uint32_t{1}), uint32_t{1});
    EXPECT_NE(mix_u64(uint64_t{1}), uint64_t{1});

    EXPECT_NE(mix_u32(0x12345678u), 0x12345678u);
    EXPECT_NE(mix_u64(0x0123456789ABCDEFULL), 0x0123456789ABCDEFULL);
}

TEST(Mixers, WidthDispatch) {
    uint8_t  k8  = 37;
    uint16_t k16 = 900;
    uint32_t k32 = 0xDEADBEEF;
    uint64_t k64 = 0x0123456789ULL;

    EXPECT_EQ(mix_width<uint8_t >(k32),  mix_u8 (static_cast<uint8_t >(k32)));
    EXPECT_EQ(mix_width<uint16_t>(k8),   mix_u16(static_cast<uint16_t>(k8)));
    EXPECT_EQ(mix_width<uint32_t>(k16),  mix_u32(static_cast<uint32_t>(k16)));
    EXPECT_EQ(mix_width<uint64_t>(k32),  mix_u64(static_cast<uint64_t>(k32)));
    EXPECT_EQ(mix_width<uint64_t>(k64),  mix_u64(k64));
}

TEST(Mixers, NativeDispatch) {
    // mix_native should be equivalent to mix_width<std::size_t>
    uint32_t k = 0xCAFEBABE;
    auto a = mix_native(k);
    auto b = mix_width<std::size_t>(k);
    EXPECT_EQ(a, b);

#if SIZE_MAX > 0xFFFFFFFFULL
    // 64-bit platform: ensure it's actually using the 64-bit path semantics
    EXPECT_EQ(mix_native(uint32_t{0}), std::size_t{mix_u64(0)});
#else
    // 32-bit (or smaller) platform
    EXPECT_EQ(mix_native(uint32_t{0}), std::size_t{mix_u32(0)});
#endif
}

TEST(Pow2, CeilPow2Basic) {
    EXPECT_EQ(ceil_pow2<uint32_t>(0), 1u);
    EXPECT_EQ(ceil_pow2<uint32_t>(1), 1u);
    EXPECT_EQ(ceil_pow2<uint32_t>(2), 2u);
    EXPECT_EQ(ceil_pow2<uint32_t>(3), 4u);
    EXPECT_EQ(ceil_pow2<uint32_t>(4), 4u);
    EXPECT_EQ(ceil_pow2<uint32_t>(5), 8u);

    EXPECT_EQ(ceil_pow2<uint8_t>(uint8_t{0}), uint8_t{1});
    EXPECT_EQ(ceil_pow2<uint8_t>(uint8_t{1}), uint8_t{1});
    EXPECT_EQ(ceil_pow2<uint8_t>(uint8_t{2}), uint8_t{2});
    EXPECT_EQ(ceil_pow2<uint8_t>(uint8_t{3}), uint8_t{4});
    EXPECT_EQ(ceil_pow2<uint8_t>(uint8_t{128}), uint8_t{128});

    // Overflow wrap case for non-saturating variant (129 -> wrap to 0 for uint8_t)
    EXPECT_EQ(ceil_pow2<uint8_t>(uint8_t{129}), uint8_t{0});
}

TEST(Pow2, CeilPow2Saturate) {
    EXPECT_EQ(ceil_pow2_saturate<uint8_t>(uint8_t{0}),   uint8_t{1});
    EXPECT_EQ(ceil_pow2_saturate<uint8_t>(uint8_t{1}),   uint8_t{1});
    EXPECT_EQ(ceil_pow2_saturate<uint8_t>(uint8_t{128}), uint8_t{128});
    EXPECT_EQ(ceil_pow2_saturate<uint8_t>(uint8_t{129}), uint8_t{128}); // clamped

    // For a larger type, the clamp threshold is 2^(W-1)
    constexpr uint32_t max_pow2_u32 = (uint32_t{1} << 31);
    EXPECT_EQ(ceil_pow2_saturate<uint32_t>(max_pow2_u32 - 3), max_pow2_u32);
    EXPECT_EQ(ceil_pow2_saturate<uint32_t>(max_pow2_u32),     max_pow2_u32);
    EXPECT_EQ(ceil_pow2_saturate<uint32_t>(max_pow2_u32 + 1), max_pow2_u32);
}

TEST(Pow2, CeilPow2IsPowerOfTwo) {
    constexpr uint8_t max_pow2 = static_cast<uint8_t>(1u << 7); // 128 for uint8_t

    // Exhaustive for uint8_t
    for (unsigned x = 0; x <= 255; ++x) {
        const uint8_t xu = static_cast<uint8_t>(x);

        // Non-saturating
        uint8_t y = ceil_pow2<uint8_t>(xu);
        if (y != 0) {
            // y should be a power of two
            EXPECT_EQ(static_cast<uint8_t>(y & static_cast<uint8_t>(y - 1)), 0u)
                << "x=" << x << " y=" << +y;
            // except x==0, y should be >= x
            if (x != 0) {
                EXPECT_GE(y, xu) << "x=" << x << " y=" << +y;
            }
        }

        // Saturating
        uint8_t ys = ceil_pow2_saturate<uint8_t>(xu);
        // saturated y must be a non-zero power of two
        EXPECT_NE(ys, 0u);
        EXPECT_EQ(static_cast<uint8_t>(ys & static_cast<uint8_t>(ys - 1)), 0u)
            << "sat x=" << x << " y=" << +ys;

        if (x <= max_pow2) {
            // In-range: should be >= x
            if (x != 0) {  // avoid comparing 0 for the x==0 special-case
                EXPECT_GE(ys, xu) << "sat x=" << x << " y=" << +ys;
            }
        } else {
            // Out-of-range: must clamp to max_pow2 (which is < x)
            EXPECT_EQ(ys, max_pow2) << "sat x=" << x << " y=" << +ys;
        }
    }
}

TEST(LogBits, BitWidthBasic) {
    EXPECT_EQ((bit_width<std::size_t>(uint8_t{0})), std::size_t{0});
    EXPECT_EQ((bit_width<std::size_t>(uint8_t{1})), std::size_t{1});
    EXPECT_EQ((bit_width<std::size_t>(uint8_t{2})), std::size_t{2});
    EXPECT_EQ((bit_width<std::size_t>(uint8_t{3})), std::size_t{2});
    EXPECT_EQ((bit_width<std::size_t>(uint8_t{255})), std::size_t{8});

    // Powers of two for 32-bit
    for (unsigned e = 0; e <= 31; ++e) {
        uint32_t v = (e == 31) ? (uint32_t{1} << 31) : (uint32_t{1} << e);
        auto bw = bit_width<std::size_t>(v);
        EXPECT_EQ(bw, static_cast<std::size_t>(e + 1));
    }
}

TEST(LogBits, CeilLog2Basic) {
    EXPECT_EQ((ceil_log2<std::size_t>(uint8_t{0})), std::size_t{0});
    EXPECT_EQ((ceil_log2<std::size_t>(uint8_t{1})), std::size_t{0});
    EXPECT_EQ((ceil_log2<std::size_t>(uint8_t{2})), std::size_t{1});
    EXPECT_EQ((ceil_log2<std::size_t>(uint8_t{3})), std::size_t{2});
    EXPECT_EQ((ceil_log2<std::size_t>(uint8_t{4})), std::size_t{2});
    EXPECT_EQ((ceil_log2<std::size_t>(uint8_t{5})), std::size_t{3});

    // For 8-bit inputs > 128, ceil_log2 should be 8 (since x-1 has bit_width 8)
    for (unsigned x = 129; x <= 255; ++x) {
        EXPECT_EQ((ceil_log2<std::size_t>(static_cast<uint8_t>(x))), std::size_t{8});
    }
}

TEST(LogBits, CeilPow2RelationWhenSafe) {
    // Where identity holds: for x in [1 .. max_pow2], ceil_pow2(x) == (1 << ceil_log2(x))
    // Test this for uint8_t domain.
    constexpr uint8_t max_pow2 = uint8_t{1} << 7;
    for (uint8_t x = 1; x <= max_pow2; ++x) {
        auto r = ceil_log2<std::size_t>(x);
        uint8_t val = static_cast<uint8_t>(std::size_t{1} << r);
        EXPECT_EQ(ceil_pow2<uint8_t>(x), val) << "x=" << +x;
    }
}

TEST(LogBits, CeilLog2Bounds) {
    // For x > 0: 2^(r-1) < = x <= 2^r, with equality on the left only when x is a power of two
    for (uint32_t x = 1; x <= 5000; ++x) {
        auto r = ceil_log2<std::size_t>(x);
        auto pow_r   = (std::size_t{1} << r);
        auto pow_rm1 = (r == 0) ? std::size_t{0} : (std::size_t{1} << (r - 1));
        EXPECT_LE(pow_rm1, static_cast<std::size_t>(x));
        EXPECT_GE(pow_r,   static_cast<std::size_t>(x));
    }
}


// all_distinct constexpr
constexpr std::array<int, 3> AD_OK  { {1, 2, 3} };
constexpr std::array<int, 3> AD_BAD { {1, 2, 1} };
constexpr std::array<unsigned, 0> AD_EMPTY { };

static_assert(all_distinct(AD_OK),    "all_distinct true at compile time");
static_assert(!all_distinct(AD_BAD),  "all_distinct false at compile time");
static_assert(all_distinct(AD_EMPTY), "all_distinct handles empty arrays");

// bucket_of constexpr basic properties
static_assert(bucket_of<std::uint32_t>(0u, 1u) == 0u, "bucket_of with 1 bucket is always 0");
static_assert(bucket_of<std::uint8_t>(0u, 8u) == 0u,  "0 maps to bucket 0");

// top_bits constexpr
static_assert(top_bits<std::uint8_t>(0xF0u, 4) == 0x0Fu, "top 4 bits of 0xF0");
static_assert(top_bits<std::uint8_t>(0xF0u, 0) == 0u,    "r=0 returns 0");
static_assert(top_bits<std::uint32_t>(0x80000000u, 1) == 1u, "MSB extract");
static_assert(top_bits<std::uint32_t>(0xFFFFFFFFu, 32) == static_cast<std::size_t>(0xFFFFFFFFu),
              "r == width(T) returns full value");

// On 64-bit, test a known pattern for r=16
#if SIZE_MAX > 0xFFFFFFFFULL
static_assert(top_bits<std::uint64_t>(0x0123456789ABCDEFULL, 16) == 0x0123u,
              "top 16 bits of 0x0123456789ABCDEF");
#endif

 
TEST(AllDistinct, HandlesBasicCases) {
    std::array<int,5> u {{1,2,3,4,5}};
    std::array<int,5> d1{{1,2,3,4,1}};
    std::array<int,5> d2{{2,2,2,2,2}};
    std::array<unsigned,0> e{};

    EXPECT_TRUE(all_distinct(u));
    EXPECT_FALSE(all_distinct(d1));
    EXPECT_FALSE(all_distinct(d2));
    EXPECT_TRUE(all_distinct(e));
}

TEST(BucketOf, RangeAndPowerOfTwoMasking) {
    // Use several bucket sizes (powers of two)
    for (std::size_t m : { std::size_t{1}, std::size_t{2}, std::size_t{4},
                           std::size_t{8}, std::size_t{16}, std::size_t{64} })
    {
        for (std::size_t k = 0; k < 1000; ++k) {
            std::size_t b = bucket_of<std::size_t>(k, m);
            EXPECT_LT(b, m) << "k=" << k << " m=" << m;
        }
    }

    // 1 bucket ⇒ always 0
    for (std::size_t k = 0; k < 256; ++k) {
        EXPECT_EQ(bucket_of<std::uint8_t>(static_cast<std::uint8_t>(k), 1u), 0u);
    }

    // Equal keys must map to equal buckets (sanity)
    EXPECT_EQ(bucket_of<std::uint32_t>(123456u, 64u),
              bucket_of<std::uint32_t>(123456u, 64u));
}

TEST(TopBits, ExtractsExpectedPatterns) {
    // 8-bit patterns
    EXPECT_EQ(top_bits<std::uint8_t>(0xF0u, 4), 0x0Fu);
    EXPECT_EQ(top_bits<std::uint8_t>(0xF0u, 1), 0x01u);
    EXPECT_EQ(top_bits<std::uint8_t>(0x0Fu, 4), 0x00u); // low nibble set, top half zero

    // r = 0 ⇒ 0
    EXPECT_EQ(top_bits<std::uint32_t>(0xDEADBEEFu, 0), 0u);

    // r = width(T) ⇒ full value as size_t
    EXPECT_EQ(top_bits<std::uint32_t>(0xDEADBEEFu, 32),
              static_cast<std::size_t>(0xDEADBEEFu));

    // 32-bit MSB and high-nibble checks
    EXPECT_EQ(top_bits<std::uint32_t>(0x80000000u, 1), 1u);
    EXPECT_EQ(top_bits<std::uint32_t>(0xF0000000u, 4), 0x0Fu);

#if SIZE_MAX > 0xFFFFFFFFULL
    // 64-bit: take top 16 bits
    EXPECT_EQ(top_bits<std::uint64_t>(0x0123456789ABCDEFULL, 16), 0x0123u);
    // r = 64 ⇒ full value
    EXPECT_EQ(top_bits<std::uint64_t>(0x00000000FFFFFFFFULL, 64),
              static_cast<std::size_t>(0x00000000FFFFFFFFULL));
#endif
}

TEST(TopBits, GuardsAgainstBadRAtRuntime) {
    // This test documents expected behavior for valid ranges.
    // r must be in [0, width(T)]. Here we test boundaries.
    EXPECT_EQ(top_bits<std::uint8_t>(0xAAu, 0), 0u);
    EXPECT_EQ(top_bits<std::uint8_t>(0xAAu, 8), static_cast<std::size_t>(0xAAu));
    // We intentionally do NOT call with r > width(T) because that would be UB.
}
