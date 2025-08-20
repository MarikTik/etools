// SPDX-License-Identifier: MIT
/**
* @file utils.hpp
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
#ifndef ETOOLS_HASHING_UTILS_HPP_
#define ETOOLS_HASHING_UTILS_HPP_
#include <cstddef>
#include <cstdint>
#include <array>

namespace etools::hashing{
    /**
    * @brief 64-bit integer avalanche mixer (SplitMix64 finalizer).
    *
    * @details
    * Applies the SplitMix64 “finalizer” sequence (xor-shifts and multiplications
    * by large odd constants) to produce well-scrambled output bits from a
    * 64-bit input. The function is tiny, branch-free, and suitable for
    * hash pre-mixing, bucket selection with power-of-two masks, and multiply–shift
    * schemes (e.g., second-level FKS hashing).
    *
    * @param[in] x  Input value to be mixed.
    * @return Mixed 64-bit value with strong bit diffusion.
    *
    * @note This is **not** a cryptographic hash. It is intended for fast,
    *       non-adversarial hashing and indexing.
    */
    [[nodiscard]] constexpr std::uint64_t mix_u64(std::uint64_t x) noexcept;
    
    /**
    * @brief 32-bit integer avalanche mixer (MurmurHash3 fmix32).
    *
    * @details
    * Uses the MurmurHash3 fmix32 finalization (xor-shifts and odd multipliers)
    * to quickly diffuse entropy across all 32 bits. Appropriate for hash
    * pre-mixing, power-of-two bucket masks, and multiply–shift hashing on
    * 32-bit targets.
    *
    * @param[in] x  Input value to be mixed.
    * @return Mixed 32-bit value with good avalanche properties.
    *
    * @note Not cryptographic; intended for fast indexing and hashing tasks.
    */
    [[nodiscard]] constexpr std::uint32_t mix_u32(std::uint32_t x) noexcept;
    
    /**
    * @brief 16-bit integer mixer (compact xorshift–multiply cascade).
    *
    * @details
    * Provides lightweight diffusion for 16-bit inputs using a short sequence
    * of xor-shifts and odd multipliers. Useful on very small targets when you
    * explicitly operate in 16-bit width. For general use, prefer widening to
    * 32 bits and calling `mix_u32`.
    *
    * @param[in] x  16-bit input value.
    * @return Mixed 16-bit value.
    *
    * @warning Lower statistical strength than 32/64-bit finalizers; not suitable
    *          for adversarial inputs or cryptographic purposes.
    */
    [[nodiscard]] constexpr std::uint16_t mix_u16(std::uint16_t x) noexcept;
    
    
    /**
    * @brief 8-bit integer mixer (compact xorshift–multiply cascade).
    *
    * Performs a tiny sequence of xor-shifts and odd multiplications to scramble
    * an 8-bit value. Intended for very small word sizes. For most scenarios,
    * prefer widening to 32 bits and using `mix_u32` to obtain more mixed bits.
    *
    * @param[in] x  8-bit input value.
    * @return Mixed 8-bit value.
    *
    * @warning Minimal statistical strength; not for cryptographic or
    *          adversarial settings.
    */
    [[nodiscard]] constexpr std::uint8_t mix_u8(std::uint8_t x) noexcept;
    
    /**
    * @brief Mix a key using a mixer chosen by the **target** integer width.
    *
    * This function selects the appropriate per-width mixer at **compile time**
    * via `if constexpr` based on T. Use it to “go wider” than the key’s
    * native width (e.g., mix a `T8_t` key with a 32-bit or 64-bit mixer)
    * when you need more mixed bits for downstream indexing.
    * 
    * @tparam T    Unsigned target integer type determining the mixing width.
    *              Must be one of: `T8_t`, `T16_t`, `T32_t`, `T64_t`.
    * @tparam Key  Unsigned input key type; will be safely cast to the width of`T`.
    *
    * @param[in] key  Input key to mix.
    * @return Mixed value of type `T`, produced by the corresponding per-width mixer.
    *
    * @note Not cryptographic. Requires unsigned integral types for well-defined
    *       wrap-around semantics.
    * complexity O(1).
    * 
    * @ref mix_u8,
    * @ref mix_u16,
    * @ref mix_u32,
    * @ref mix_u64.
    */
    template <typename T, typename Key>
    [[nodiscard]] constexpr T mix_width(Key key) noexcept;
    
    /**
    * @brief Mix a key using the mixer that matches the **native machine word size**.
    *
    * Provides a portable way to select the mixing width based on
    * `sizeof(std::size_t)` at **compile time**. This is convenient when you
    * want hashing/indexing logic to follow the target’s native word size.
    * 
    * @tparam Key  Unsigned input key type; will be cast to `std::size_t` width.
    *
    * @param[in] key  Input key to mix.
    * @return Mixed value as `std::size_t`. On 64-bit platforms this is the result
    *         of`mix_u64`; on 32-bit platforms, `mix_u32`; similarly for
    *         16-/8-bit if applicable.
    *
    * @note Not cryptographic. For deterministic cross-platform behavior,
    *       prefer @ref mix_width with an explicit target width.
    * complexity O(1).
    */
    template <typename Key>
    [[nodiscard]] constexpr std::size_t mix_native(Key key) noexcept;
    
    /**
    * @brief 16-bit integer mixer (compact xorshift–multiply cascade).
    *
    * Provides lightweight diffusion for 16-bit inputs using a short sequence
    * of xor-shifts and odd multipliers. Useful on very small targets when you
    * explicitly operate in 16-bit width. For general use, prefer widening to
    * 32 bits and calling `mix_u32`.
    *
    * @param[in] x  16-bit input value.
    * @return Mixed 16-bit value.
    *
    * @warning Lower statistical strength than 32/64-bit finalizers; not suitable
    *          for adversarial inputs or cryptographic purposes.
    */
    [[nodiscard]] constexpr std::uint16_t mix_u16(std::uint16_t x) noexcept;
    
    /**
    * @brief Smallest power of two greater than or equal to x.
    *
    * Returns the least power-of-two value that is not smaller than x.
    * Uses the standard bit-smearing trick (decrement, propagate highest bit, add one).
    * For x == 0 this returns 1. This version does not clamp on overflow.
    *
    * @tparam T Unsigned integer type.
    * @param x Input value.
    * @return The next power of two (e.g., 1,2,4,8,...) or 0 on overflow wrap.
    *
    * @warning If x is larger than the largest power of two representable in T,
    *          the result may wrap to 0. Use `ceil_pow2_saturate` to clamp instead.
    */
    template <typename T>
    [[nodiscard]] constexpr T ceil_pow2(T x) noexcept;
    
    /**
    * @brief Smallest power of two greater than or equal to x, clamped.
    *
    * Same as `ceil_pow2` but clamps the result to the maximum power of two
    * representable in T instead of wrapping. For x == 0 this returns 1.
    *
    * @tparam T Unsigned integer type.
    * @param x Input value.
    * @return The next power of two, clamped to the maximum power of two in T.
    */
    template <class T>
    [[nodiscard]] constexpr T ceil_pow2_saturate(T x) noexcept;
    
    /**
    * @brief Number of bits needed to represent x (floor(log2(x)) + 1).
    *
    * Returns 0 for x == 0. For x > 0, returns the position of the most significant
    * set bit plus one. Useful for sizing and for computing ceil_log2.
    *
    * @tparam R Unsigned return type (e.g., `std::size_t` to use in shifts, or `uint8_t` for compact storage).
    * @tparam T Unsigned input type.
    * @param x Input value.
    * @return Bit width of x (0 if x == 0).
    *
    * @warning Requires unsigned types to keep right shifts well-defined.
    */
    template <typename R, typename T>
    [[nodiscard]] constexpr R bit_width(T x) noexcept;
    
    /**
    * @brief Ceiling base-2 logarithm of x.
    *
    * Computes the smallest integer r such that 2^r >= x. Returns 0 for x <= 1.
    * Implemented via `bit_width(x - 1)` for x >= 1.
    *
    * @tparam R Unsigned return type (often `std::size_t` for arithmetic).
    * @tparam T Unsigned input type.
    * @param x Input value.
    * @return ceil(log2(x)), with 0 for x <= 1.
    *
    * @warning Requires unsigned types to keep right shifts well-defined.
    */
    template <typename R, typename T>
    [[nodiscard]] constexpr R ceil_log2(T x) noexcept;
    
    /**
    * @brief Map a key to a bucket index using native-width mixing.
    *
    * Mixes the key with `mix_native` and selects a bucket by masking with
    * `bucket_count - 1`. This assumes `bucket_count` is a power of two and
    * greater than zero.
    *
    * @tparam Key Unsigned key type (e.g., uint8_t..uint64_t).
    * @param[in] k Key to hash.
    * @param[in] bucket_count Number of buckets (must be a power of two, > 0).
    * @return Bucket index in the range [0, bucket_count).
    *
    * @warning `bucket_count` must be a power of two; otherwise the mask will
    *          introduce bias and the result may be out of intended spec.
    */
    template <typename Key>
    [[nodiscard]] constexpr std::size_t bucket_of(Key k, std::size_t bucket_count) noexcept;
    
    /**
    * @brief Extract the top (most-significant) r bits of x as a size_t.
    *
    * Returns the r most-significant bits of x, right-aligned. If r == 0,
    * returns 0. The template type T determines the bit width used for shifting.
    *
    * @tparam T Unsigned integer type of x.
    * @param[in] x Value to sample bits from.
    * @param[in] r Number of top bits to extract (must satisfy 0 <= r <= width(T)).
    * @return The top r bits of x, right-aligned, as size_t.
    *
    * @warning r must not exceed the bit width of T. Passing r > width(T) causes
    *          an invalid shift. If r == width(T), the full value of x is returned.
    */
    template <typename T>
    [[nodiscard]] constexpr std::size_t top_bits(T x, std::uint8_t r) noexcept;

    
} // namespace etools::hashing
#include "utils.tpp"
#endif // ETOOLS_HASHING_UTILS_HPP_