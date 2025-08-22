#include <gtest/gtest.h>
#include <etools/hashing/fks.hpp>
#include <utility>
#include <cstdint>
using namespace etools::hashing;

namespace test_fks {
    // Affine permutation over modulo M, computed at compile time over integer_sequence.
    template<typename K, std::size_t A, std::size_t C, std::size_t M, std::size_t Offset, class Seq>
    struct affine_seq;

    template<typename K, std::size_t A, std::size_t C, std::size_t M, std::size_t Offset, K... I>
    struct affine_seq<K, A, C, M, Offset, std::integer_sequence<K, I...>> {
        static_assert(M != 0, "Modulus M must be nonzero");
        using type = std::integer_sequence<K,
            static_cast<K>( ( (A * (static_cast<std::size_t>(I) + Offset)) + C ) % M )...
        >;
    };

    // Build table from a sequence of keys (NTTP pack forwarding).
    template<typename K, K... Ks>
    constexpr decltype(auto) make_table_from_seq(std::integer_sequence<K, Ks...>) {
        // Return the canonical singleton by const-ref
        return etools::hashing::fks<K>::template instance<Ks...>();
    }

    // Runtime version of the same affine mapping (for verification loops).
    template<typename K>
    constexpr K lcg_k(std::size_t i, std::size_t A, std::size_t C, std::size_t M, std::size_t Offset=0) {
        return static_cast<K>(((A * (i + Offset)) + C) % M);
    }
} // namespace test_fks


TEST(FKS_Small, TrivialSets) {
    {
        using G = fks<std::uint8_t>;
        constexpr const auto& T = G::instance<42u>();
        static_assert(T.size() == 1, "N");
        EXPECT_EQ(T(42u), 0u);
        EXPECT_EQ(T(41u), T.not_found());
    }
    {
        using G = fks<std::uint16_t>;
        constexpr const auto& T = G::instance<1u,2u,3u,4u,5u>();
        for (std::size_t i = 0; i < 5; ++i) {
            EXPECT_EQ(T(static_cast<std::uint16_t>(i + 1)), i);
        }
        EXPECT_EQ(T(0u), T.not_found());
        EXPECT_EQ(T(999u), T.not_found());
    }
}

TEST(FKS_Medium, DenseSequential_1024_uint16) {
    using K = std::uint16_t;
    // Sequence 0..1023
    constexpr const auto& T = test_fks::make_table_from_seq(
        std::make_integer_sequence<K, 1024>{}
    );

    for (std::size_t i = 0; i < 1024; ++i) {
        EXPECT_EQ(T(static_cast<K>(i)), i) << "i=" << i;
    }
    // Non-members (first 256 outside the set)
    for (std::size_t i = 1024; i < 1280; ++i) {
        EXPECT_EQ(T(static_cast<K>(i)), T.not_found()) << "i=" << i;
    }
}

TEST(FKS_Medium, AffinePermutation_2048_uint16) {
    using K = std::uint16_t;
    constexpr std::size_t M = 1ull << 16;
    constexpr K A = 25173;  // odd -> gcd(A, M)=1 -> permutation
    constexpr K C = 13849;
    using Base = std::make_integer_sequence<K, 2048>;
    using Perm = typename test_fks::affine_seq<K, A, C, M, /*Offset=*/0, Base>::type;

    constexpr const auto& T = test_fks::make_table_from_seq(Perm{});

    for (std::size_t i = 0; i < 2048; ++i) {
        const K key = test_fks::lcg_k<K>(i, A, C, M);
        EXPECT_EQ(T(key), i) << "key=" << key << " i=" << i;
    }
    // next 256 are guaranteed not in first 2048 for a full-period LCG
    for (std::size_t i = 0; i < 256; ++i) {
        const K key = test_fks::lcg_k<K>(2048 + i, A, C, M);
        EXPECT_EQ(T(key), T.not_found()) << "key=" << key;
    }
}

// ---------- Optional stress (compile-time N selectable) ----------
#ifdef ETOOLS_STRESS_N
TEST(FKS_Stress, N_is_configured) {
    using K = std::uint16_t;
    constexpr K M = 1u << 16;
    constexpr K A = 25173;
    constexpr K C = 13849;

    using Base = std::make_integer_sequence<K, ETOOLS_STRESS_N>;
    using Perm = typename test_fks::affine_seq<K, A, C, M, 0, Base>::type;

    constexpr const auto& T = test_fks::make_table_from_seq(Perm{});

    for (std::size_t i = 0; i < ETOOLS_STRESS_N; ++i) {
        const K key = test_fks::lcg_k<K>(i, A, C, M);
        ASSERT_EQ(T(key), i) << "key=" << key << " i=" << i;
    }

    if (ETOOLS_STRESS_N < M) {
        const std::size_t extra = std::min<std::size_t>(std::size_t{1024}, std::size_t{M} - ETOOLS_STRESS_N);
        for (std::size_t i = 0; i < extra; ++i) {
            const K key = test_fks::lcg_k<K>(ETOOLS_STRESS_N + i, A, C, M);
            EXPECT_EQ(T(key), T.not_found()) << "key=" << key;
        }
    }
}
#endif
