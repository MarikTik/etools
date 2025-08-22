// SPDX-License-Identifier: MIT
/**
* @file optimal_mph.tpp
*
* @brief Definition of optimal_mph.hpp methods.
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
#ifndef ETOOLS_HASHING_OPTIMAL_MPH_TPP_
#define ETOOLS_HASHING_OPTIMAL_MPH_TPP_
#include "optimal_mph.hpp"
#include "fks.hpp"
#include "llut.hpp"

namespace etools::hashing{
    template <typename KeyType, std::size_t AlphaScaled>
    template <KeyType... Keys>
    constexpr const auto& optimal_mph<KeyType, AlphaScaled>::instance() noexcept{
        // N and K (pack size and span)
        constexpr std::size_t N = sizeof...(Keys);
        static_assert(N > 0, "At least one key is required");
        
        constexpr std::size_t K = static_cast<std::size_t>(meta::tpack_max<KeyType, Keys...>()) + 1u;
        
        // Index storage chosen the same way your backends do
        using index_t = meta::smallest_uint_t<N>;
        constexpr std::size_t s_index = sizeof(index_t);
        constexpr std::size_t s_key = sizeof(KeyType);
        constexpr std::size_t s_sz = sizeof(std::size_t);
        
        // Compare memory models (integer math; no FP)
        // LLUT ≈ K*s_index
        // FKS  ≈ N*(AlphaScaled*s_index + 2*s_sz + 1 + s_key)
        constexpr bool use_fks = (K * s_index) > (N * (AlphaScaled * s_index + 2 * s_sz + 1 + s_key));
        
        if constexpr (use_fks) 
        return etools::hashing::fks<KeyType>::template instance<Keys...>();
        else 
        return etools::hashing::llut<KeyType>::template instance<Keys...>();
    }
    
} // namespace etools::hashing

#endif // ETOOLS_HASHING_OPTIMAL_MPH_TPP_