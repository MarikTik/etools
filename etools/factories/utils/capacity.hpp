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
} // namespace etools::factories::utils
#endif //ETOOLS_FACTORIES_UTILS_CAPACITY_HPP_
