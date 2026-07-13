// SPDX-License-Identifier: MIT
/**
* @file capacity.hpp
*
* @ingroup etools_factories etools::factories::utils
*
* @brief Type-registration tag pairing a derived type with its inline slot count.
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2026-06-29
*
* @copyright
* MIT License
* Copyright (c) 2025 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_FACTORIES_UTILS_CAPACITY_HPP_
#define ETOOLS_FACTORIES_UTILS_CAPACITY_HPP_
#include <cstddef>
namespace etools::factories::utils {
    /**
    * @brief Registration tag that pairs a derived type with how many concurrent
    *        instances the `dispatch_factory` should reserve storage for.
    *
    * Pass one `capacity<T, N>` per registered type as a template argument to
    * `dispatch_factory`.  For the common single-instance case, `N` defaults to 1
    * and bare (unwrapped) types are accepted at the public facade and are
    * normalized to `capacity<T, 1>` automatically.
    *
    * @tparam T Derived type to register.  Must derive from the factory's `Base`
    *           and satisfy `dispatch_factory`'s static constraints
    *           (`nothrow_destructible`, non-abstract).
    * @tparam N Number of concurrent instances to reserve.  Must be > 0.
    *
    * @note `capacity<T, N>::type`  is `T`.
    * @note `capacity<T, N>::count` is `N`.
    */
    template<typename T, std::size_t N = 1>
    struct capacity {
        using type = T;
        static constexpr std::size_t count = N;
    };

    /**
    * @brief Normalises a registration argument to `capacity<T, N>`.
    *
    * A bare type `T` becomes `capacity<T, 1>`; an already-wrapped `capacity<T, N>`
    * passes through unchanged.  This is the canonical primitive for any utility that
    * accepts a mix of bare derived types and explicit `capacity<>` tags.
    *
    * @tparam T Either a bare type or a `capacity<T, N>` specialisation.
    */
    template<typename T>
    struct as_capacity { using type = capacity<T, 1>; };

    /**
    * @brief Partial specialisation for an already-wrapped `capacity<T, N>`.
    *
    * Passes the tag through unchanged; no double-wrapping occurs.
    *
    * @tparam T The derived type carried by the tag.
    * @tparam N The slot count carried by the tag.
    */
    template<typename T, std::size_t N>
    struct as_capacity<capacity<T, N>> { using type = capacity<T, N>; };

    /**
    * @brief Convenience alias for `as_capacity<T>::type`.
    *
    * @tparam T Either a bare type or a `capacity<T, N>` specialisation.
    */
    template<typename T>
    using as_capacity_t = typename as_capacity<T>::type;

} // namespace etools::factories::utils
#endif //ETOOLS_FACTORIES_UTILS_CAPACITY_HPP_
