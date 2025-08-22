// SPDX-License-Identifier: MIT
/**
* @file fks.hpp
*
* @brief Perfect hashing over a fixed key set (FKS): two-level structure with O(1) lookups.
*
* @details
* This header provides a header-only, compile-time generator for a two-level
* Fredman–Komlós–Szemerédi (FKS) perfect hash structure. Given a fixed set of
* unsigned keys as non-type template parameters (NTTPs), it constructs a compact,
* read-only lookup artifact that maps those keys to dense indices `[0..N-1]` in
* the order they are provided. The resulting table performs O(1) lookups without
* collisions and returns a sentinel (`N`) for non-members.
*
* **Why this exists**
* - Constant-time membership and indexing for *fixed* key sets
* - Completely static/constexpr — lives in ROM/Flash on embedded targets
* - No dynamic allocation, no runtime initialization order issues
*
* **How it works (high-level)**
* - First level (bucketing): choose a power-of-two bucket count `M≈N`, and bucketize
*   keys by a fast native-width mixer.
* - Second level (per bucket): choose a small table size `S_b` and find an odd
*   multiplier `a_b` such that the multiply–shift hash places all keys of that
*   bucket without collisions.
* - The buckets are laid out in a flat array; lookups combine a bucket base with
*   a local position and verify the original key.
*
* **Surface**
* - `etools::hashing::fks<Key>` — Facade. Use `fks<Key>::instance<Keys...>()` to
*   obtain a `constexpr const&` to the canonical, per-key-set singleton.
* - `etools::hashing::details::fks_impl<Key, Keys...>` — implementation. Immutable,
*   constexpr-constructible structure that owns the arrays and exposes `operator()`,
*   `size()`, `not_found()`, `buckets()`, and `slots()`.
*
* **References**
* - Fredman, Komlós, Szemerédi. “Storing a Sparse Table with O(1) Access Time.”
*   *Journal of the ACM*, 1984.
*
* **Example**
* @code
* #include <cstdint>
* #include "etools/hashing/fks.hpp"
*
* using F = etools::hashing::fks<std::uint64_t>;
*
* // Keys are NTTPs; indices follow the pack order.
* constexpr const auto& H = F::instance<1ULL, 5ULL, 2ULL, 10ULL, 7ULL>();
*
* static_assert(H.size() == 5);
* static_assert(H(1)  == 0);
* static_assert(H(5)  == 1);
* static_assert(H(2)  == 2);
* static_assert(H(10) == 3);
* static_assert(H(7)  == 4);
* static_assert(H(999) == H.not_found());
*
* // Runtime pattern:
* std::size_t idx = H(10);
* if (idx < H.size()) {
*   // found
* } else {
*   // not found
* }
* @endcode
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2025-08-20
*
* @copyright
* MIT License
* Copyright (c) 2025 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_HASHING_FKS_HPP_
#define ETOOLS_HASHING_FKS_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "utils.hpp"            // mix_native, top_bits, ceil_pow2, etc. (constexpr)
#include "../meta/traits.hpp"   // meta::smallest_uint_t<N>
#include "../meta/utility.hpp"  // meta::all_distinct(...)

namespace etools::hashing {
    
    /**
    * @brief Forward reference to facade for FKS perfect hashing over a fixed key set.
    *
    * @tparam KeyType Unsigned integral key type.
    */
    template <typename KeyType>
    class fks;
    
    /**
    * @brief Internal implementation namespace.
    *
    * @warning Entities in `details` are implementation details and are not part of the
    *          stable public API.
    */
    namespace details {
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
        
        /**
        * @brief Implementation type: immutable, constexpr-built FKS table for a fixed key pack.
        *
        * @tparam KeyType Unsigned integral key type.
        * @tparam Keys Distinct keys; pack order defines dense indices `[0..N-1]`.
        *
        * ```cpp
        * using Impl = etools::hashing::details::fks_impl<std::uint8_t, 2, 5, 7>;
        * // Prefer: using F = etools::hashing::fks<std::uint8_t>;
        * //          constexpr const auto& H = F::instance<2,5,7>();
        * ```
        *
        * @note This class is constructed at compile time. Instances are exposed via the facade.
        * @note Lookups are O(1). Non-members return `not_found()` (equal to `size()`).
        * @warning Do not instantiate directly; use `fks<KeyType>::instance<Keys...>()`.
        */
        template <typename KeyType, KeyType... Keys>
        class fks_impl {    
        public:
            /**
            * @brief Number of keys in the perfect set (also used as the sentinel).
            *
            * @return Count of NTTP keys `N`.
            */
            [[nodiscard]] static constexpr std::size_t size() noexcept;
            
            /**
            * @brief Sentinel index meaning “not a member”.
            *
            * @return `size()` (i.e., `N`).
            */
            [[nodiscard]] static constexpr std::size_t not_found() noexcept;
            
            /**
            * @brief First-level bucket count (power of two).
            *
            * @return `M = ceil_pow2(N)` (at least 1).
            */
            [[nodiscard]] static constexpr std::size_t buckets() noexcept;
            
            /**
            * @brief Total slots across all second-level tables.
            *
            * @return `Σ_b (1 << r_b)` with `r_b` chosen per bucket.
            */
            [[nodiscard]] static constexpr std::size_t slots() noexcept;
            
            /**
            * @brief Constant-time lookup.
            *
            * @param[in] key Query key of type `KeyType`.
            * @return Dense index in `[0..size()-1]` if present; otherwise `not_found()`.
            *
            * ```cpp
            * using F = etools::hashing::fks<std::uint32_t>;
            * constexpr const auto& H = F::instance<10,20,30>();
            * std::size_t i = H(20);                   // 1
            * bool found = (i < H.size());             // true
            * ```
            */
            [[nodiscard]] constexpr std::size_t operator()(KeyType key) const noexcept;
            
            /**
            * @brief Deleted copy constructor (non-copyable).
            *
            * @warning The structure is immutable and exposed as a singleton reference.
            */
            fks_impl(const fks_impl&) = delete;

            /**
            * @brief Deleted copy assignment (non-assignable).
            */
            fks_impl& operator=(const fks_impl&) = delete;

            /**
            * @brief Deleted move constructor (non-movable).
            */
            fks_impl(fks_impl&&) = delete;

            /**
            * @brief Deleted move assignment (non-movable).
            */
            fks_impl& operator=(fks_impl&&) = delete;
            
        private:
            /**
            * @typedef index_t
            *
            * @brief Compact index storage type for slots.
            *
            * Chosen as the smallest unsigned type able to represent `[0..size()]`.
            * This type is private by design and not part of the public API surface.
            */
            using index_t = meta::smallest_uint_t<size()>; // public API does not expose it
            
            /**
            * @brief Per-bucket odd multipliers `a_b` for the multiply–shift hash at level 2.
            *
            * Size equals `buckets()`.
            */
            std::array<std::size_t, buckets()> _local_multiplier{}; // a_b (odd)

            /**
            * @brief Per-bucket bit widths `r_b` such that second-level size is `1 << r_b`.
            *
            * Size equals `buckets()`.
            */
            std::array<std::uint8_t, buckets()> _local_bits{};       // r_b

            /**
            * @brief Base offsets into the flattened slots array for each bucket.
            *
            * Size equals `buckets()`. The slice
            * `[ _base_offset[b], _base_offset[b] + (1 << _local_bits[b]) )`
            * holds bucket `b`’s second-level table.
            */
            std::array<std::size_t, buckets()> _base_offset{};      // base[b]

            /**
            * @brief Flattened second-level table storing final indices or the sentinel.
            *
            * Size equals `slots()`. Each slot is either an index in `[0..size()-1]`
            * or `not_found()` (equal to `size()`).
            */
            std::array<index_t, slots()> _slot_to_index{};    // [0..N-1] or N

            /**
            * @brief Membership array: original keys placed by their final indices.
            *
            * Used to verify equality and reject accidental false positives.
            * Size equals `size()`.
            */
            std::array<KeyType, size()> _keys_by_index{};    // original keys at final indices
            
            /**
            * @brief Private constexpr constructor; builds the entire structure.
            *
            * @note Construction is constexpr; no dynamic memory is used.
            */
            constexpr fks_impl() noexcept;
        
            /**
            * @brief Compute the local position within a bucket for a given key.
            *
            * @param[in] b   Bucket index in `[0..buckets()-1]`.
            * @param[in] key Query key.
            * @return Local offset in `[0..(1<<_local_bits[b]) - 1]`.
            */
            [[nodiscard]] constexpr std::size_t local_pos(std::size_t b, KeyType key) const noexcept;
            
            /**
            * @brief Friend factory permitted to invoke the private constructor.
            *
            * @return A value-initialized implementation object (used to form the singleton).
            */
            template <typename K, K... Ks> friend constexpr fks_impl<K, Ks...> make_fks_impl() noexcept;

            /**
            * @brief facade friendship for access to the canonical singleton.
            */
            template <typename K>          friend class ::etools::hashing::fks;
            
            /**
            * @brief Compile-time sanity: `buckets()` is a nonzero power of two.
            */
            static_assert(buckets() && ((buckets() & (buckets() - 1)) == 0),
            "BucketCount must be a nonzero power of two");
            
            /**
            * @brief Compile-time sanity: key type is unsigned integral.
            */
            static_assert(std::is_unsigned_v<KeyType>, "KeyType must be unsigned integral");
        };
        
        /**
        * @brief constexpr factory: constructs an `fks_impl` by value.
        *
        * @tparam Key Unsigned integral key type.
        * @tparam Keys Distinct keys; pack order defines indices.
        * @return A value-initialized `fks_impl<Key, Keys...>`.
        */
        template <typename Key, Key... Keys>
        constexpr fks_impl<Key, Keys...> make_fks_impl() noexcept;
        
        /**
        * @brief Canonical singleton for a given key set.
        *
        * @tparam Key Unsigned integral key type.
        * @tparam Keys Distinct keys; pack order defines indices.
        *
        * @note `inline constexpr` variable template with static storage duration.
        * @warning Internal — prefer using the facade `fks<Key>::instance<Keys...>()`.
        */
        template <typename Key, Key... Keys>
        inline constexpr auto fks_impl_singleton = make_fks_impl<Key, Keys...>();
            
    } // namespace details
        
    /**
    * @brief Facade for FKS perfect hashing over a fixed key set.
    *
    * @tparam KeyType Unsigned integral key type (e.g., `std::uint8_t`, `std::uint16_t`, ...).
    *
    * ```cpp
    * using F = etools::hashing::fks<std::uint16_t>;
    * constexpr const auto& H = F::instance<10, 42, 7, 99>();
    * static_assert(H(42) == 1);
    * ```
    *
    * @note The returned reference denotes a per-key-set singleton with static storage.
    * @warning The facade itself is non-instantiable and non-copyable.
    */
    template <typename KeyType>
    class fks {
    public:
        /**
        * @brief Obtain the canonical lookup instance for a fixed key set.
        *
        * @tparam Keys Distinct keys; the order defines dense indices `[0..N-1]`.
        * @return `constexpr const details::fks_impl<KeyType, Keys...>&` to the singleton.
        *
        * ```cpp
        * constexpr const auto& H = etools::hashing::fks<std::uint16_t>::instance<10,20,30>();
        * auto i = H(20);                  // 1
        * bool ok = (i < H.size());        // true if present
        * ```
        *
        * @note Every call with the same `<KeyType, Keys...>` returns a reference to the same object.
        */
        template <KeyType... Keys>
        [[nodiscard]] static constexpr const details::fks_impl<KeyType, Keys...>& instance() noexcept;
        
        /**
        * @brief Deleted copy constructor.
        *
        * @warning The facade is a purely static namespace-like holder.
        */
        fks(const fks&) = delete;

        /**
        * @brief Deleted copy assignment.
        */
        fks& operator=(const fks&) = delete;

        /**
        * @brief Deleted move constructor.
        */
        fks(fks&&)  = delete;

        /**
        * @brief Deleted move assignment.
        */
        fks& operator=(fks&&) = delete;
        
    private:
        /**
        * @brief Private default constructor — facade is not meant to be instantiated.
        */
        constexpr fks() noexcept = default;
    };
} // namespace etools::hashing

#include "fks.tpp"
#endif // ETOOLS_HASHING_FKS_HPP_
    