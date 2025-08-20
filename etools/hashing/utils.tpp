// SPDX-License-Identifier: MIT
/**
* @file utils.tpp
*
* @brief Definition of utils.hpp methods.
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2025-08-17
*
* @copyright
* MIT License
* Copyright (c) 2025 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_HASHING_UTILS_TPP_
#define ETOOLS_HASHING_UTILS_TPP_
#include "utils.hpp"
#include "../future_std/utility.hpp"
#include "../meta/traits.hpp"
#include <limits>
#include <type_traits>

namespace etools::hashing{
    constexpr std::uint64_t mix_u64(std::uint64_t x) noexcept{
        x ^= x >> 30;
        x *= 0xBF58476D1CE4E5B9ULL;
        x ^= x >> 27;
        x *= 0x94D049BB133111EBULL;
        x ^= x >> 31;
        return x;
    }
    constexpr std::uint32_t mix_u32(std::uint32_t x) noexcept{
        x ^= x >> 16;
        x *= 0x85EBCA6Bu;
        x ^= x >> 13;
        x *= 0xC2B2AE35u;
        x ^= x >> 16;
        return x;
    }
    constexpr std::uint16_t mix_u16(std::uint16_t x) noexcept{
        x ^= static_cast<std::uint16_t>(x >> 7);
        x = static_cast<std::uint16_t>(x * static_cast<std::uint16_t>(0x9E37u));
        x ^= static_cast<std::uint16_t>(x >> 11);
        x = static_cast<std::uint16_t>(x * static_cast<std::uint16_t>(0x85EBu));
        x ^= static_cast<std::uint16_t>(x >> 7);
        return x;
    }
    constexpr std::uint8_t mix_u8(std::uint8_t x) noexcept{
        x ^= static_cast<std::uint8_t>(x >> 4);
        x = static_cast<std::uint8_t>(x * static_cast<std::uint8_t>(0x9Bu));
        x ^= static_cast<std::uint8_t>(x >> 3);
        x = static_cast<std::uint8_t>(x * static_cast<std::uint8_t>(0xC3u));
        x ^= static_cast<std::uint8_t>(x >> 5);
        return x;
    }
    
    template <class T, class Key>
    constexpr T mix_width(Key key) noexcept {
        static_assert(std::is_unsigned<T>::value, "T must be an unsigned integer");
        static_assert(std::is_unsigned<Key>::value, "Key must be unsigned");
        if constexpr (sizeof(T) == 8) 
        return static_cast<T>(mix_u64(static_cast<std::uint64_t>(key)));
        else if constexpr (sizeof(T) == 4) 
        return static_cast<T>(mix_u32(static_cast<std::uint32_t>(key)));
        else if constexpr (sizeof(T) == 2) 
        return static_cast<T>(mix_u16(static_cast<std::uint16_t>(key)));
        else if constexpr (sizeof(T) == 1)
        return static_cast<T>(mix_u8(static_cast<std::uint8_t >(key)));
        else static_assert(meta::always_false_v<T>, "Unsupported mix width");
    }
    
    template <class Key>
    constexpr std::size_t mix_native(Key key) noexcept
    {
        return mix_width<std::size_t, Key>(key);
    }
    
    template <typename T>
    constexpr T ceil_pow2(T x) noexcept{
        static_assert(std::is_unsigned_v<T>, "T must be unsigned");
        
        // Define behavior for x == 0
        if (x <= 1) return T{1};
        --x;
        x |= (x >> 1);
        x |= (x >> 2);
        x |= (x >> 4);
        if constexpr (std::numeric_limits<T>::digits > 8)  x |= (x >> 8);
        if constexpr (std::numeric_limits<T>::digits > 16) x |= (x >> 16);
        if constexpr (std::numeric_limits<T>::digits > 32) x |= (x >> 32);
        return x + 1;
    }
    
    template <class T>
    constexpr T ceil_pow2_saturate(T x) noexcept{
        static_assert(std::is_unsigned<T>::value, "T must be unsigned");
        
        if (x <= 1) return T{1};
        constexpr int W = std::numeric_limits<T>::digits;
        constexpr T max_pow2 = T{1} << (W - 1);
        if (x > max_pow2) return max_pow2;
        return ceil_pow2(x);
    }
    
    template <typename R, typename T>
    constexpr R bit_width(T x) noexcept
    {   
        static_assert(std::is_unsigned<T>::value, "T must be unsigned");
        static_assert(std::is_unsigned<R>::value, "R must be unsigned");
        // R must hold values up to value-bit count of T.
        static_assert(std::numeric_limits<R>::max() >= std::numeric_limits<T>::digits,
        "R too small to hold bit width of T");
        
        if (x == 0) return R{0};
        
        // Shift on a known unsigned width to avoid signed promotions.
        using U = std::conditional_t<(std::numeric_limits<T>::digits <= 32), std::uint32_t, std::uint64_t>;
        U v = static_cast<U>(x);
        
        R r = R{0};
        if constexpr (std::numeric_limits<U>::digits >= 64) {
            if (v >> 32) { v >>= 32; r += R{32}; }
        }
        if constexpr (std::numeric_limits<U>::digits >= 32) {
            if (v >> 16) { v >>= 16; r += R{16}; }
        }
        if (v >> 8)  { v >>= 8;  r += R{8}; }
        if (v >> 4)  { v >>= 4;  r += R{4}; }
        if (v >> 2)  { v >>= 2;  r += R{2}; }
        if (v >> 1)  {           r += R{1}; }
        return static_cast<R>(r + R{1}); // since x>0
    }

    template <typename R, typename T>
    constexpr R ceil_log2(T x) noexcept{
        static_assert(std::is_unsigned_v<R>, "Return type must be unsigned");
        static_assert(std::is_unsigned_v<T>, "T must be unsigned");
        if (x <= 1) return R{0};
        return bit_width<R>(static_cast<T>(x - 1));
    }

    template <typename Key>
    constexpr std::size_t bucket_of(Key k, std::size_t bucket_count) noexcept{
        // Use Word-width mixing (32 or 64 typically); mask because bucket_count is a power of two.
        auto mixed = mix_native<Key>(k);
        return static_cast<std::size_t>(mixed) & (bucket_count - 1);
    }
    
    template <typename T>
    constexpr std::size_t top_bits(T x, std::uint8_t r) noexcept {
        if (r == 0) return 0;
        constexpr unsigned W = std::numeric_limits<T>::digits;
        return static_cast<std::size_t>(x >> (W - r));
    }
}
#endif // ETOOLS_HASHING_UTILS_TPP_

