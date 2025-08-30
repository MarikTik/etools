// SPDX-License-Identifier: MIT
/**
* @file utility.hpp
*
* @ingroup etools_meta etools::meta
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
#ifndef ETOOLS_META_UTILITY_HPP_
#define ETOOLS_META_UTILITY_HPP_
#include <array>

namespace etools::meta{
    
    /**
    * @brief Compute the maximum value in a non-empty parameter pack of integral
    *        values or enum values (scoped or unscoped).
    *
    * Accepts `T` being an integral type or an `enum`/`enum class`. If `T` is an
    * enum, its `std::underlying_type_t<T>` is used for the comparison and fold.
    *
    * @tparam T      Integral or enum type.
    * @tparam First  First (required) value in the pack.
    * @tparam Rest   Remaining values.
    * @return The maximum value among {First, Rest...}, returned as `T`.
    *
    * @note Purely compile-time; no storage is materialized.
    * @note If `T` is an enum, the result is `static_cast<T>(max_underlying)`.
    *       This allows values that may not correspond to a named enumerator,
    *       but are representable by the underlying type.
    */
    template <typename T, T First, T... Rest>
    constexpr T tpack_max() noexcept;
    
    /**
    * @brief Constexpr distinctness check using an open-addressed hash set.
    *
    * Inserts each T into a compile-time linear-probing table (capacity is
    * a power of two, typically ≥ 2·N). Keys are mixed to spread them across
    * the table; if a probe encounters the same T already present, a duplicate
    * is detected. Runs in ~O(N) time and avoids the O(N²) blow-up of naive
    * pairwise comparison during constant evaluation.
    *
    * @tparam T Unsigned integral T type.
    * @tparam N   Number of keys in the array.
    * @param[in] keys  Array of keys to test for pairwise distinctness.
    * @return true if all keys are distinct; false if any duplicate is found.
    *
    * @note Uses your existing mixing utilities (e.g., `mix_native`) and a
    *       power-of-two capacity so probes are short even under constexpr.
    * @warning Requires an unsigned T type for well-defined bit operations.
    */
    template <class T, std::size_t N>
    constexpr bool all_distinct_probe(const std::array<T, N>& keys) noexcept;
    
    
    /**
    * @brief Constexpr distinctness check via bitmap membership (≤ 16-bit keys).
    *
    * Allocates one bit per possible T value and sets the bit for each
    * observed T. If a bit is already set, the T is a duplicate. This is
    * O(N) with very small constants and is ideal for `uint8_t`/`uint16_t`
    * keys during compile-time evaluation.
    *
    * @tparam T Unsigned integral T type with at most 16 value bits.
    * @tparam N   Number of keys in the array.
    * @param[in] keys  Array of keys to test for pairwise distinctness.
    * @return true if all keys are distinct; false on any duplicate.
    *
    * @note Memory footprint during constant evaluation is
    *       `(1 << digits(T)) / 8` bytes (e.g., 32 B for 8-bit, 8 KB for 16-bit).
    * @warning Constrained to types where `std::numeric_limits<T>::digits <= 16`.
    *          For wider keys, prefer `all_distinct_probe`.
    */
    template <class T, std::size_t N>
    constexpr bool all_distinct_bitmap(const std::array<T, N>& keys) noexcept;
    
    
    /**
    * @brief Constexpr distinctness check that chooses the best strategy by T width.
    *
    * Uses a bitmap-based check for small T widths (≤ 16 value bits) and
    * falls back to an open-addressed probing set for wider keys. This provides
    * O(N) behavior with good compile-time performance across common T sizes.
    *
    * @tparam T Unsigned integral T type.
    * @tparam N   Number of keys in the array.
    * @param[in] keys  Array of keys to test for pairwise distinctness.
    * @return true if all keys are distinct; false otherwise.
    *
    * @note Selection is made with `if constexpr` on
    *       `std::numeric_limits<T>::digits`, ensuring no runtime overhead.
    * @warning Requires an unsigned T type. For extremely small N, a simple
    *          O(N²) pairwise check is also acceptable but less scalable in constexpr.
    */
    template <class T, std::size_t N>
    constexpr bool all_distinct_fast(const std::array<T, N>& keys) noexcept;
    
} // namespace etools::meta
#include "utility.tpp"
#endif // ETOOLS_META_UTILITY_HPP_

