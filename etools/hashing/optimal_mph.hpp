// SPDX-License-Identifier: MIT
/**
* @file optimal_mph.hpp
*
* @brief Minimal Perfect Hash (MPH) selector: choose the most memory-efficient
*        backend (LLUT or FKS) at compile time and return its singleton.
*
* @details
* This header provides a facade that selects an implementation of a
* **Minimal Perfect Hash Function (MPHF)** for a fixed key set.
* An MPHF maps N distinct keys to the range `[0..N-1]` with no collisions.
*
* Two backends are supported:
*  - **LLUT** (direct address): a dense array indexed by key value; trivial MPHF,
*    excellent when the key span is compact.
*  - **FKS** (two-level perfect hashing): near-linear space in N with a small constant,
*    excellent for sparse key sets or large key spans.
*
* The selector compares compile-time memory models for key pack and chooses
* the better one. It then returns a **`constexpr const&`** to the chosen backend’s
* canonical singleton. The exact concrete type is intentionally unspecified.
*
* **Selection heuristic (integer math, all at compile time)**
*
* Let:
*  - `N = sizeof...(Keys)` — number of keys;
*  - `K = max_key + 1` — span of the value domain;
*  - `index_t = meta::smallest_uint_t<N>` — per-entry index type;
*  - `α ~ 3` — conservative total-slots factor for FKS.
*
* We approximate:
*  - `LLUT_mem ~ K * sizeof(index_t)`
*  - `FKS_mem ~ N * ( α*sizeof(index_t) + 2*sizeof(size_t) + 1 + sizeof(KeyType) )`
*
* If `LLUT_mem > FKS_mem`, we select **FKS**; otherwise we select **LLUT**.
*
* **How to use**
*  - Include this header.
*  - Call `optimal_mph<Key>::instance<Keys...>()` to get a `constexpr const&`
*    to the canonical MPHF object. It supports `operator()(key)`, `size()`,
*    and `not_found()`. (Do not rely on backend-specific members.)
*
* **Example**
* @code
* #include <cstdint>
* #include "etools/hashing/optimal_mph.hpp"
*
* using Opt = etools::hashing::optimal_mph<std::uint16_t>;
*
* // Dense-ish set: likely LLUT
* constexpr const auto& A = Opt::instance<2,5,7,8,9>();
* static_assert(A.size() == 5);
* static_assert(A(7) == 2);
* static_assert(A(999) == A.not_found());
*
* // Sparse set: likely FKS
* constexpr const auto& B = Opt::instance<1, 10000, 60000>();
* static_assert(B(60000) == 2);
* @endcode
*
* **Notes**
*  - Keys must be **distinct**; this is checked at compile time unless you
*    define `ETOOLS_SKIP_CONSTEXPR_DISTINCT_CHECK`.
*  - `instance<...>()` always returns a reference to a **singleton** with
*    static storage duration (ODR-merged across translation units).
*  - If you *want* a specific backend, call `llut<Key>::instance<...>()`
*    or `fks<Key>::instance<...>()` directly; the point of this facade is
*    to choose for you.
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2025-08-21
*
* @copyright
* MIT License
* Copyright (c) 2025 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_HASHING_OPTIMAL_MPH_HPP_
#define ETOOLS_HASHING_OPTIMAL_MPH_HPP_
#include <cstdint>

namespace etools::hashing{

    /**
    * @brief Choose an MPH backend (LLUT or FKS) at compile time and return its singleton.
    *
    * @tparam KeyType Unsigned integral key type.
    * @tparam AlphaScaled Integer approximation of FKS’s “α” (total-slots factor).
    *         Use 3 (default) for a conservative choice; 2 if your buckets are usually even.
    *
    * @note The decision uses a pure-constexpr memory model:
    *       - LLUT memory ~ K * sizeof(index_t)          (K = max_key+1)
    *       - FKS memory ~ N * (α*index_t + 2*size_t + 1 + sizeof(KeyType))
    *       If LLUT’s cost exceeds FKS’s, we choose FKS; otherwise LLUT.
    *
    * @warning `instance<...>()` returns a `constexpr const&` to the chosen backend’s
    *          canonical singleton. The *type* of the returned object depends on the
    *          key pack and may be either `llut_impl<...>` or `fks_impl<...>`.
    */
    template <typename KeyType, std::size_t AlphaScaled = 3>
    struct optimal_mph {
    public:
        /**
        * @brief Obtain the canonical MPH instance for the given key pack.
        *
        * @tparam Keys Distinct keys; pack order defines dense indices `[0..N-1]`.
        * @return `constexpr const auto&` to the chosen backend’s singleton.
        *
        * ```cpp
        * using Opt = etools::hashing::optimal_mph<std::uint16_t>;
        * constexpr const auto& H = Opt::instance<1,5,2,10,7>();  // LLUT or FKS
        * static_assert(H(10) == 3);
        * ```
        */
        template <KeyType... Keys>
        [[nodiscard]] static constexpr const auto& instance() noexcept;

        /**
        * @brief Deleted default constructor — this facade is not meant to be instantiated.
        */
        optimal_mph() = delete;

        /**
        * @brief Deleted copy constructor.
        */
        optimal_mph(const optimal_mph&) = delete;

        /**
        * @brief Deleted copy assignment.
        */
        optimal_mph& operator=(const optimal_mph&) = delete;

        /**
        * @brief Deleted move constructor.
        */
        optimal_mph(optimal_mph&&) = delete;

        /**
        * @brief Deleted move assignment.
        */
        optimal_mph& operator=(optimal_mph&&) = delete;
    };
    
} // namespace etools::hashing

#include "optimal_mph.tpp"
#endif // ETOOLS_HASHING_OPTIMAL_MPH_HPP_