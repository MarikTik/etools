// SPDX-License-Identifier: MIT
/**
* @file fks.tpp
*
* @brief Definition of fks.hpp methods.
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2025-08-19
*
* @copyright
* MIT License
* Copyright (c) 2025 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_HASHING_FKS_TPP_
#define ETOOLS_HASHING_FKS_TPP_
#include "fks.hpp"
#include "../meta/utility.hpp"
namespace etools::hashing{
        namespace details{
        template <typename KeyType, std::size_t N, std::size_t BucketCount>
        constexpr std::array<std::size_t, BucketCount> compute_bucket_counts(
            const std::array<KeyType, N> &keys
        ) noexcept {
            std::array<std::size_t, BucketCount> counts{};
            for (std::size_t i = 0; i < N; ++i) {
                const std::size_t b = bucket_of<KeyType>(keys[i], BucketCount);
                counts[b]++;
            }
            return counts;
        }

        template <std::size_t BucketCount>
        constexpr std::array<std::size_t, BucketCount + 1> offsets_from_counts(
            const std::array<std::size_t, BucketCount> &counts
        ) noexcept{
            std::array<std::size_t, BucketCount + 1> off{};
            off[0] = 0;
            for (std::size_t b = 0; b < BucketCount; ++b) {
                off[b + 1] = off[b] + counts[b];
            }
            return off;
        }

        template <typename KeyType, std::size_t N, std::size_t BucketCount>
        constexpr std::array<std::size_t, N> items_csr(
            const std::array<KeyType, N> &keys,
            const std::array<std::size_t, BucketCount + 1> &off
        ) noexcept
        {
            std::array<std::size_t, N> items{};
            std::array<std::size_t, BucketCount> fill{};
            for (std::size_t i = 0; i < N; ++i) {
                const std::size_t b = bucket_of<KeyType>(keys[i], BucketCount);
                const std::size_t pos = off[b] + (fill[b]++);
                items[pos] = i;
            }
            return items;
        }

        template <std::size_t BucketCount>
        constexpr std::array<std::uint8_t, BucketCount> compute_rbits(const std::array<std::size_t, BucketCount> &counts) noexcept{
            std::array<std::uint8_t, BucketCount> r{};
            for (std::size_t b = 0; b < BucketCount; ++b) {
                const std::size_t s = counts[b];
                const std::size_t target = (s <= 1) ? std::size_t{1} : (s * s);
                r[b] = static_cast<std::uint8_t>(ceil_log2<std::size_t>(target));
            }
            return r;
        }

        template <std::size_t BucketCount>
        constexpr std::size_t total_slots_from_rbits(const std::array<std::uint8_t, BucketCount> &r) noexcept{
            std::size_t T = 0;
            for (std::size_t b = 0; b < BucketCount; ++b) {
                T += (std::size_t{1} << r[b]);
            }
            return T;
        }
        template <std::size_t BucketCount>
        constexpr std::array<std::size_t, BucketCount> base_from_rbits(const std::array<std::uint8_t, BucketCount> &r) noexcept{
            std::array<std::size_t, BucketCount> base{};
            std::size_t acc = 0;
            for (std::size_t b = 0; b < BucketCount; ++b) {
                base[b] = acc;
                acc += (std::size_t{1} << r[b]);
            }
            return base;
        }
    }
    template <typename KeyType, std::size_t N, std::size_t BucketCount, std::size_t TotalSlots>
    constexpr std::size_t fks_table<KeyType, N, BucketCount, TotalSlots>::operator()(KeyType key) const noexcept {
            const std::size_t mixed  = mix_native(key);
            const std::size_t b = mixed & (BucketCount - 1);
            const std::uint8_t r = _local_bits[b];
            const std::size_t  a = _local_multiplier[b];
            const std::size_t  base = _base_offset[b];
            const std::size_t local = top_bits<std::size_t>(mixed * a, r);
            const std::size_t pos = base + local;
            const std::size_t idx = static_cast<std::size_t>(_slot_to_index[pos]);
            if (idx == not_found()) return not_found();
            return (_keys_by_index[idx] == key) ? idx : not_found();
    }

    template <typename KeyType, std::size_t N, std::size_t BucketCount, std::size_t TotalSlots>
    constexpr std::size_t fks_table<KeyType, N, BucketCount, TotalSlots>::not_found() noexcept
    {
        return N;
    }

    template <typename KeyType, std::size_t N, std::size_t BucketCount, std::size_t TotalSlots>
    constexpr std::size_t fks_table<KeyType, N, BucketCount, TotalSlots>::size() noexcept
    {
        return N;
    }

    template <typename KeyType, std::size_t N, std::size_t BucketCount, std::size_t TotalSlots>
    constexpr std::size_t fks_table<KeyType, N, BucketCount, TotalSlots>::bucket_count() noexcept
    {
        return BucketCount;
    }

    template <typename KeyType>
    template <KeyType... Keys>
    constexpr auto fks_hash_gen<KeyType>::generate(){
        using namespace details;
        constexpr std::size_t N = sizeof...(Keys);
        static_assert(N > 0, "At least one key is required");
        // Keys in pack order (final indices will be 0..N-1)
        constexpr std::array<KeyType, N> keys{ { Keys... } };
        // Ensure distinct keys
        static_assert(meta::all_distinct_fast(keys), "Keys must be distinct");
        // 1) First-level: bucketization (power-of-two bucket count)
        constexpr std::size_t M = ceil_pow2<std::size_t>(N);  // >= 1
        static_assert(M && ((M & (M - 1)) == 0), "BucketCount must be power-of-two");
        constexpr auto counts = compute_bucket_counts<KeyType, N, M>(keys);
        constexpr auto offs   = offsets_from_counts<M>(counts);
        constexpr auto items  = items_csr<KeyType, N, M>(keys, offs);
        // 2) Second-level sizes and layout
        constexpr auto rbits  = compute_rbits<M>(counts);
        constexpr std::size_t total = total_slots_from_rbits<M>(rbits);
        constexpr auto base    = base_from_rbits<M>(rbits);
        // 3) Final table type (all params are constant-expr)
        using table_t = fks_table<KeyType, N, M, total>;
        table_t table{};
        // Copy metadata (loop; whole-array assignment isn't constexpr in C++17)
        for (std::size_t b = 0; b < M; ++b) {
            table._local_bits[b]      = rbits[b];
            table._base_offset[b]     = base[b];
            table._local_multiplier[b]= std::size_t{1}; // set later
        }
        // Initialize slots to sentinel N
        for (std::size_t i = 0; i < total; ++i) {
            table._slot_to_index[i] = static_cast<typename table_t::index_t>(N);
        }
        // Membership array: original key at its final index (pack order)
        for (std::size_t i = 0; i < N; ++i) {
            table._keys_by_index[i] = keys[i];
        }
        // Scratch for per-bucket local positions (size N is sufficient)
        std::array<std::size_t, N> local_pos{};
        // 4) For each bucket, find an odd multiplier 'a' with collision-free local positions
        for (std::size_t b = 0; b < M; ++b) {
            const std::size_t s = counts[b];
            if (s == 0) {
                table._local_multiplier[b] = std::size_t{1};
                continue;
            }
            const std::uint8_t r   = rbits[b];
            const std::size_t  base_pos = base[b];
            const std::size_t  start = offs[b];
            std::size_t chosen = std::size_t{1};
            for (std::size_t seed = 1;; ++seed) {
                const std::size_t a = (mix_native(seed) | std::size_t{1}); // odd
                // Compute candidate local positions for this bucket
                for (std::size_t j = 0; j < s; ++j) {
                    const std::size_t key_index = items[start + j];
                    const std::size_t mixed = mix_native(keys[key_index]);
                    local_pos[j] = top_bits<std::size_t>(mixed * a, r);
                }
                // Check for duplicates (O(s^2); s is tiny on average)
                bool ok = true;
                for (std::size_t i = 0; i < s && ok; ++i) {
                    for (std::size_t j = i + 1; j < s; ++j) {
                        if (local_pos[i] == local_pos[j]) { ok = false; break; }
                    }
                }
                if (ok) { chosen = a; break; }
            }
            table._local_multiplier[b] = chosen;
            // Commit the keys into the global slots (index = pack order)
            for (std::size_t j = 0; j < s; ++j) {
                const std::size_t key_index = items[start + j];        // 0..N-1
                const std::size_t pos = base_pos + local_pos[j];
                table._slot_to_index[pos] =
                    static_cast<typename table_t::index_t>(key_index);
            }
        }
        return table; // NRVO; usable in constexpr
    }
}
#endif // ETOOLS_HASHING_FKS_TPP_