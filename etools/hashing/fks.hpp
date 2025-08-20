// SPDX-License-Identifier: MIT
/**
* @file fks.hpp
* 
* @brief FKS Perfect Hash (compile-time generator + O(1) lookup)
* 
* @ingroup etools_hashing etools::hashing
*
* Overview
* --------
* This module implements a two-level hash table following the
* Fredman–Komlós–Szemerédi (FKS) scheme to achieve O(1) lookups with no collisions
* over a *fixed* key set. All construction happens at compile time using template
* metaprogramming and `constexpr` evaluation; the runtime artifact is a compact,
* read-only table.
* 
* How it works
* ------------
* 1) First level (bucketing):
*    - Choose M = next power-of-two ≥ N.
*    - Hash each key to a bucket in [0..M-1] using a native-width mixer and a mask.
*    - Let s_b be the number of keys in bucket b.
* 
* 2) Second level (collision-free per bucket):
*    - For each bucket b, choose a table size S_b = 2^r_b where r_b = ceil_log2(s_b^2).
*    - Search for an odd multiplier a_b such that the function
*         g_b(key) = top_bits( mix(key) * a_b, r_b )
*      produces distinct positions for all keys in b.
*    - Place each key at base[b] + g_b(key) in a single flat array of size
*      TOTAL = sum_b S_b. Store its final index and keep the original key in a
*      `_keys_by_index` array for exact membership checks.
* 
* Properties
* ----------
* - Lookup: O(1) time; a couple of array reads and multiplications.
* - Memory: ~ O(N). With M ≈ N, the expected TOTAL is around ~2N–3N.
* - Non-members: return N (use `idx < N` to branchlessly reject).
* - Determinism: given the key set and mixer, the table is immutable/read-only.
* 
* Note
* ----------
* All tables produced are `constexpr` data and live in Flash/ROM. For N≈100,
* expect around 2 KB (exact usage depends on key width and the realized `total` slots.
* 
* Caveats
* -------
* - The scheme assumes non-adversarial keys. For adversarial settings consider
*   salting the mixer or forcing a larger M.
* - On 32-bit targets, if your keys are truly 64-bit and differ only in the upper
*   32 bits, native 32-bit mixing collapses them. In that case, force 64-bit mixing
*   (see the note in this file about hashing width policy).
* 
* References
* ----------
* - Fredman, Komlós, Szemerédi. "Storing a Sparse Table with O(1) Access Time",
*   JACM, 1984. (FKS hashing)
*
* Example
* -------
* using Gen = etools::hashing::fks_hash_gen<std::uint64_t>;
* 
* // Keys are NTTPs; indices are pack order 0..N-1
* constexpr auto FKS = Gen::generate<1ULL, 5ULL, 2ULL, 10ULL, 7ULL>();
* 
* static_assert(FKS.size() == 5, "N");
* static_assert(FKS(1)  == 0);
* static_assert(FKS(5)  == 1);
* static_assert(FKS(2)  == 2);
* static_assert(FKS(10) == 3);
* static_assert(FKS(7)  == 4);
* static_assert(FKS(999) == FKS.not_found);
* 
* // Runtime use:
* std::size_t idx = FKS(10);
* if (idx < FKS.size()) {
*     // found
* } else {
*     // not found
* }
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2025-08-19
*
* @copyright
* MIT License
* Copyright (c) 2025 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_HASHING_FKS_HPP_
#define ETOOLS_HASHING_FKS_HPP_
#include "utils.hpp"
#include "../meta/traits.hpp"
#include <type_traits>
namespace etools::hashing{
    namespace details{
        /**
        * @brief Count how many keys fall into each first-level bucket.
        *
        * Distributes keys into M = BucketCount buckets using `bucket_of` and
        * returns per-bucket counts. Used to size second-level tables.
        *
        * @tparam KeyType Unsigned key type.
        * @tparam N       Number of keys.
        * @tparam BucketCount Power-of-two number of buckets.
        * @param keys Input keys in pack order.
        * @return Array of size BucketCount with counts per bucket.
        */
        template <typename KeyType, std::size_t N, std::size_t BucketCount>
        [[nodiscard]] constexpr std::array<std::size_t, BucketCount>
        compute_bucket_counts(const std::array<KeyType, N>& keys) noexcept;

        /**
        * @brief Compute CSR-style offsets from per-bucket counts.
        *
        * Produces an array `off` where `off[b]` is the starting index in a compact
        * storage region for bucket `b`. The final element `off[BucketCount]` equals
        * the total number of items.
        *
        * @tparam BucketCount Power-of-two number of buckets.
        * @param counts Per-bucket counts.
        * @return Array of size BucketCount+1 with prefix sums.
        */
        template <std::size_t BucketCount>
        [[nodiscard]] constexpr std::array<std::size_t, BucketCount + 1>
        offsets_from_counts(const std::array<std::size_t, BucketCount>& counts) noexcept;

        /**
        * @brief Build a CSR (Compressed Sparse Row) "items" array of key indices grouped by bucket.
        *
        * Returns indices 0..N-1 placed contiguously by their bucket, using
        * the offsets computed by @ref offsets_from_counts.
        *
        * @tparam KeyType Unsigned key type.
        * @tparam N       Number of keys.
        * @tparam BucketCount Power-of-two number of buckets.
        * @param keys Input keys.
        * @param counts Per-bucket counts.
        * @param off CSR offsets.
        * @return Array of size N with key indices grouped by bucket.
        */
        template <typename KeyType, std::size_t N, std::size_t BucketCount>
        [[nodiscard]] constexpr std::array<std::size_t, N>
        items_csr(const std::array<KeyType, N>& keys,
                  const std::array<std::size_t, BucketCount + 1>& off) noexcept;

        /**
        * @brief Decide per-bucket second-level table width `r_b`.
        *
        * For bucket size `s`, chooses `r_b = ceil_log2( s<=1 ? 1 : s*s )`, which
        * corresponds to a second-level table size `S_b = 1 << r_b` at least `s^2`.
        *
        * @tparam BucketCount Power-of-two number of buckets.
        * @param counts Per-bucket counts.
        * @return Array of size BucketCount with `r_b` values in bits.
        */
        template <std::size_t BucketCount>
        [[nodiscard]] constexpr std::array<std::uint8_t, BucketCount>
        compute_rbits(const std::array<std::size_t, BucketCount>& counts) noexcept;

        /**
        * @brief Sum of all second-level table sizes.
        *
        * Computes `total = Σ_b (1 << r_b)`.
        *
        * @tparam BucketCount Power-of-two number of buckets.
        * @param r Per-bucket bit widths `r_b`.
        * @return Total slot count across all buckets.
        */
        template <std::size_t BucketCount>
        [[nodiscard]] constexpr std::size_t
        total_slots_from_rbits(const std::array<std::uint8_t, BucketCount>& r) noexcept;

        /**
        * @brief Compute base offsets for second-level tables.
        *
        * Returns `base[b]` giving the starting offset of bucket b’s second-level
        * table in the flat `slot_to_index` array, using a prefix sum of `(1 << r_b)`.
        *
        * @tparam BucketCount Power-of-two number of buckets.
        * @param r Per-bucket bit widths `r_b`.
        * @return Array of size BucketCount with base offsets.
        */
        template <std::size_t BucketCount>
        [[nodiscard]] constexpr std::array<std::size_t, BucketCount>
        base_from_rbits(const std::array<std::uint8_t, BucketCount>& r) noexcept;
    } // namespace details
    
    /**
    * @brief Read-only lookup artifact for a two-level FKS perfect hash.
    *
    * Holds per-bucket parameters and the flattened second-level table that
    * maps each key to a unique index in [0..N-1]. Non-members return `not_found`
    * (which equals N). Designed to be produced at compile time by @ref fks_hash_gen.
    * 
    * Instances are intended to be produced at compile time by `fks_hash_gen`.
    * 
    * @tparam KeyType     Unsigned key type.
    * @tparam N           Number of keys.
    * @tparam BucketCount Power-of-two first-level bucket count.
    * @tparam TotalSlots  Total size of all second-level tables combined.
    */
    template <
        typename KeyType,
        std::size_t N,
        std::size_t BucketCount,
        std::size_t TotalSlots
    >
    class fks_table {
        /**
        * @brief Compact index storage type for slots.
        *
        * Stores either a valid index in [0..N-1] or the sentinel `N`.
        * Chosen to minimize memory: 16-bit when N ≤ 65535, otherwise a wider type.
        */
        using index_t = meta::smallest_uint_t<N>;
    public:
        /**
        * @brief Return the index for `key`, or the sentinel `not_found()` if absent.
        *
        * Operation:
        * 1) Mix `key` with `mix_native()` and select a first-level bucket using a mask.
        * 2) Read bucket’s multiply–shift parameters (odd multiplier a_b and r_b bits).
        * 3) Compute the local position from the top r_b bits of (mixed * a_b).
        * 4) Load the candidate index from the flat slots array.
        * 5) If it equals the sentinel, return `not_found()`; otherwise verify
        *    the original key via the membership array and return the index if matched.
        *
        * @note Average O(1), no dynamic memory, branch-light.
        *
        * @param[in] key Query key.
        * @return Index in [0..N-1] if present; otherwise not_found() (equals N).
        */
        [[nodiscard]] constexpr std::size_t operator()(KeyType key) const noexcept;

        /**
        * @brief Sentinel index meaning “not a member”.
        *
        * Equals N. This is the value written into empty slots in the flat table.
        */
        [[nodiscard]] static constexpr std::size_t not_found() noexcept;
       
        /**
        * @brief Number of keys (size of the perfect set).
        */
        [[nodiscard]] static constexpr std::size_t size() noexcept;
        
        /**
        * @brief First-level bucket count (power of two).
        */
        [[nodiscard]] static constexpr std::size_t bucket_count() noexcept;

    private:
        /**
        * @brief Per-bucket odd multipliers a_b used in multiply–shift at level 2.
        *
        * For each bucket b, the generator finds an odd a_b so that the top r_b bits
        * of (mix_native(key) * a_b) are collision-free for that bucket’s keys.
        */
        std::array<std::size_t, BucketCount> _local_multiplier{};
        
        /**
        * @brief Per-bucket bit widths r_b; the bucket’s second-level table size is 1 << r_b.
        *
        * FKS sets r_b ≈ ceil_log2(s_b^2) for bucket size s_b > 1 and r_b = 0 for s_b = 0,
        * r_b = 0 or 1 for s_b = 1 (depending on your policy). This ensures room to find
        * a collision-free a_b quickly.
        */
        std::array<std::uint8_t, BucketCount> _local_bits{};    

        /**
        * @brief Base offsets into the flattened slots array for each bucket.
        *
        * The slice [_base_offset[b], _base_offset[b] + (1 << _local_bits[b])) holds the
        * second-level slots for bucket b.
        */
        std::array<std::size_t,  BucketCount> _base_offset{};  // start in slots[]
        
        /**
        * @brief Flattened second-level table storing final indices or the sentinel.
        *
        * If a slot is unoccupied, it holds not_found() (which equals N).
        * Otherwise it stores an index into [0..N-1].
        */
        std::array<index_t, TotalSlots> _slot_to_index{};    // [0..N-1] or N
        
        /**
        * @brief Membership array: original keys placed by their final indices.
        *
        * This provides the definitive equality check on lookups to reject
        * occasional false positives from different buckets sharing slot values.
        */
        std::array<KeyType, N> _keys_by_index{};
        
        /// Defaulted; instances are created by the generator.
        constexpr fks_table() = default;

        // fks_hash_gen populates internals.
        template<typename K> friend class fks_hash_gen;

        // Invariants checked at compile time:
        static_assert(std::is_unsigned<KeyType>::value, "KeyType must be unsigned integral");
        static_assert(BucketCount && ((BucketCount & (BucketCount - 1)) == 0),
                  "BucketCount must be power-of-two and > 0");
    };
    /**
    * @brief Compile-time builder for fks_table from a pack of distinct keys.
    *
    * Given a parameter pack of unsigned keys, selects a first-level bucket count,
    * groups keys by bucket (CSR layout), picks second-level sizes r_b, and then
    * for each bucket searches an odd multiplier a_b so that the per-bucket
    * multiply–shift hash is collision-free. Produces an fks_table that can be
    * used at runtime with O(1) lookups and no dynamic allocation.
    *
    * @tparam KeyType Unsigned key type (uint8_t..uint64_t)
    * Usage:
    * ```cpp
    * using Gen = fks_hash_gen<std::uint16_t>;
    * constexpr auto T = Gen::generate<10, 42, 7, 99>();
    * static_assert(T.size() == 4, "N");
    * static_assert(T(42) == 1, "index of 42");
    * ```
    *
    * Requirements:
    * - KeyType must be an unsigned integral type.
    * - Keys in the pack must be pairwise distinct (checked at compile time unless you disable it).
    */
    template <typename KeyType>
    struct fks_hash_gen {
        static_assert(std::is_unsigned_v<KeyType>, "KeyType must be an unsigned integral type");
        /**
        * @brief Build the perfect hash table for the given key pack.
        *
        * Keys are assigned indices in pack order (0..N-1). The first-level
        * bucket count is `M = ceil_pow2(N)`. For each bucket, an odd multiplier
        * `a_b` is searched until the second-level positions are collision-free.
        *
        * @tparam Keys The key set as NTTPs (must be distinct).
        * @return A fully-initialized `fks_table`.
        *
        * @note Non-member lookups return `table.not_found` (which equals N).
        */
        template <KeyType... Keys>
        static constexpr auto generate();
    };
} // namespace etools::hashing

#include "fks.tpp"
#endif // ETOOLS_HASHING_FKS_HPP_
