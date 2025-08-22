// SPDX-License-Identifier: MIT
/**
* @file fks.tpp
*
* @brief Definition of fks.hpp methods.
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
#ifndef ETOOLS_HASHING_FKS_TPP_
#define ETOOLS_HASHING_FKS_TPP_

#include "fks.tpp"
#include "fks.hpp"
namespace etools::hashing {
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

        template <typename Key, Key... Keys>
        constexpr fks_impl<Key, Keys...> make_fks_impl() noexcept{
            return fks_impl<Key, Keys...>{}; 
        }

        template <typename KeyType, KeyType... Keys>
        constexpr fks_impl<KeyType, Keys...>::fks_impl() noexcept {
            // Distinctness check (no static data members => no ODR headaches)
            #ifndef ETOOLS_SKIP_CONSTEXPR_DISTINCT_CHECK
            {
                constexpr std::array<KeyType, size()> key_set{{ Keys... }};
                static_assert(meta::all_distinct_fast(key_set), "FKS keys must be distinct");
            }
            #endif
            
            // 1) Pack and first-level counts
            constexpr std::array<KeyType, size()> keys{ { Keys... } };
            constexpr auto counts = compute_bucket_counts<KeyType, size(), buckets()>(keys);
            constexpr auto offs = offsets_from_counts<buckets()>(counts);
            
            // 2) Second-level sizing and layout
            constexpr auto rbits = compute_rbits<buckets()>(counts);
            constexpr auto base = base_from_rbits<buckets()>(rbits);
            
            // 3) CSR of key indices grouped by bucket
            constexpr auto items = items_csr<KeyType, size(), buckets()>(keys, offs);
            
            // 4) init metadata + membership
            for (std::size_t b = 0; b < buckets(); ++b) {
                _local_bits[b] = rbits[b];
                _base_offset[b] = base[b];
                _local_multiplier[b] = std::size_t{1}; // set per-bucket below
            }
            for (std::size_t i = 0; i < size(); ++i) {
                _keys_by_index[i] = keys[i];
            }
            for (std::size_t i = 0; i < slots(); ++i) {
                _slot_to_index[i] = static_cast<index_t>(not_found()); // sentinel
            }
            
            // 5) choose per-bucket odd multiplier a_b; place keys
            std::array<std::size_t, size()> local{};
            for (std::size_t b = 0; b < buckets(); ++b) {
                const std::size_t s = counts[b];
                if (s == 0) { _local_multiplier[b] = std::size_t{1}; continue; }
                
                const std::uint8_t r = rbits[b];
                const std::size_t off = offs[b];
                const std::size_t b0 = base[b];
                
                std::size_t chosen = std::size_t{1};
                for (std::size_t seed = 1;; ++seed) {
                    const std::size_t a = (mix_native(seed) | std::size_t{1}); // odd
                    // candidate local positions
                    for (std::size_t j = 0; j < s; ++j) {
                        const std::size_t idx = items[off + j];
                        const std::size_t m = mix_native(keys[idx]);
                        local[j] = top_bits<std::size_t>(m * a, r);
                    }
                    // check distinct
                    bool ok = true;
                    for (std::size_t i = 0; i < s && ok; ++i)
                    for (std::size_t j = i + 1; j < s; ++j)
                    if (local[i] == local[j]) { ok = false; break; }
                    if (ok) { chosen = a; break; }
                }
                
                _local_multiplier[b] = chosen;
                for (std::size_t j = 0; j < s; ++j) {
                    const std::size_t idx = items[off + j];           // final dense index
                    const std::size_t pos = b0 + local[j];            // global slot
                    _slot_to_index[pos] = static_cast<index_t>(idx);
                }
            }
        }
        
        template <typename KeyType, KeyType... Keys>
        constexpr std::size_t fks_impl<KeyType, Keys...>::size() noexcept {
            return sizeof...(Keys); 
        }
        
        template <typename KeyType, KeyType... Keys>
        constexpr std::size_t fks_impl<KeyType, Keys...>::not_found() noexcept {
            return size(); 
        }
        
        template <typename KeyType, KeyType... Keys>
        constexpr std::size_t fks_impl<KeyType, Keys...>::buckets() noexcept{
            return ceil_pow2<std::size_t>(size() ? size() : 1);
        }
        
        template <typename KeyType, KeyType... Keys>
        constexpr std::size_t fks_impl<KeyType, Keys...>::slots() noexcept {
            constexpr std::array<KeyType, size()> keys{ { Keys... } };
            constexpr auto counts = compute_bucket_counts<KeyType, size(), buckets()>(keys);
            constexpr auto rbits = compute_rbits<buckets()>(counts);
            return total_slots_from_rbits<buckets()>(rbits);
        }
        
        template <typename KeyType, KeyType... Keys>
        constexpr std::size_t
        fks_impl<KeyType, Keys...>::local_pos(std::size_t b, KeyType key) const noexcept {
            const std::size_t r = _local_bits[b];
            const std::size_t a = _local_multiplier[b];
            const std::size_t mixed = mix_native<std::size_t>(static_cast<std::size_t>(key));
            return top_bits<std::size_t>(mixed * a, r);
        }
        
        template <typename KeyType, KeyType... Keys>
        constexpr std::size_t
        fks_impl<KeyType, Keys...>::operator()(KeyType key) const noexcept {
            const std::size_t mixed  = mix_native<std::size_t>(static_cast<std::size_t>(key));
            const std::size_t b = mixed & (buckets() - 1);
            const std::size_t base = _base_offset[b];
            const std::size_t pos = base + local_pos(b, key);
            const index_t v = _slot_to_index[pos];
            if (v == static_cast<index_t>(size())) return size();   // empty slot
            const std::size_t i = static_cast<std::size_t>(v);
            return (_keys_by_index[i] == key) ? i : size();         // membership guard
        }
        
    } // namespace details;
    
    template <typename KeyType>
    template <KeyType... Keys>
    constexpr const details::fks_impl<KeyType, Keys...>& fks<KeyType>::instance() noexcept {
        return details::fks_impl_singleton<KeyType, Keys...>;
    }
} // namespace etools::hashing

#endif // ETOOLS_HASHING_FKS_TPP_