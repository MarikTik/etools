// SPDX-License-Identifier: MIT
/**
* @file utility.tpp
*
* @brief Definition of utility.hpp methods.
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
#ifndef ETOOLS_META_UTILITY_TPP_
#define ETOOLS_META_UTILITY_TPP_
#include "utility.hpp"
#include "../hashing/utils.hpp"
#include <type_traits>
#include "traits.hpp"
#include <array>
#include <limits>

namespace etools::meta{
    template <typename T, T First, T... Rest>
    constexpr T tpack_max() noexcept{
        using U = typename std::conditional_t<
            std::is_enum_v<T>,
            std::underlying_type<T>,   // note: NOT _t
            type_identity<T>
        >::type;

        static_assert(
            std::is_integral_v<U>,
            "tpack_max<T,...>: T must be integral or an enum with integral underlying type"
        );

        U m = static_cast<U>(First);
        ((m = (m < static_cast<U>(Rest) ? static_cast<U>(Rest) : m)), ...);
        return static_cast<T>(m);
    }
    
    template <class T, std::size_t N>
    constexpr bool all_distinct_probe(const std::array<T, N>& keys) noexcept {
        static_assert(std::is_unsigned_v<T>, "T must be unsigned");
        
        if (N < 2) return true;
        
        // Capacity = next power of two >= 2N (at least 1)
        constexpr std::size_t wanted = (N < (std::numeric_limits<std::size_t>::max() / 2))
        ? (N * 2) : (std::numeric_limits<std::size_t>::max() / 2);
        constexpr std::size_t cap = hashing::ceil_pow2<std::size_t>(wanted);
        
        // Separate occupancy array so we don't need a special empty-T sentinel.
        std::array<uint8_t, cap> used{}; // 0 = empty, 1 = occupied
        std::array<T, cap> slots{}; // keys
        
        for (std::size_t i = 0; i < N; ++i) {
            const T k = keys[i];
            std::size_t h = hashing::mix_native(k) & (cap - 1);
            
            // linear probe
            for (;;) {
                if (!used[h]) {
                    used[h]  = 1;
                    slots[h] = k;
                    break;
                }
                if (slots[h] == k) {
                    return false; // duplicate
                }
                h = (h + 1) & (cap - 1);
            }
        }
        return true;
    }
    
    template <class T, std::size_t N>
    constexpr bool all_distinct_bitmap(const std::array<T, N>& keys) noexcept {
        static_assert(std::is_unsigned_v<T>, "T must be unsigned");
        constexpr unsigned W = std::numeric_limits<T>::digits;
        static_assert(W <= 16, "bitmap variant only for <=16-bit keys");
        
        constexpr std::size_t bits  = std::size_t{1} << W;
        constexpr std::size_t words = (bits + 63) / 64;
        
        std::array<std::uint64_t, words> bitset{};
        for (std::size_t i = 0; i < N; ++i) {
            const std::size_t v  = static_cast<std::size_t>(keys[i]);
            const std::size_t w  = v >> 6;
            const std::uint64_t m = 1ull << (v & 63);
            if (bitset[w] & m) return false;
            bitset[w] |= m;
        }
        return true;
    }

    template <class T, std::size_t N>
    constexpr bool all_distinct_fast(const std::array<T, N>& keys) noexcept {
        if constexpr (std::numeric_limits<T>::digits <= 16) 
            return all_distinct_bitmap<T, N>(keys);
        else 
            return all_distinct_probe<T, N>(keys);
    }

} // namespace etools::meta

#endif // ETOOLS_META_UTILITY_TPP_