// SPDX-License-Identifier: MIT
/**
* @file llut.tpp
*
* @brief Definition of llut.hpp methods.
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
#ifndef ETOOLS_HASHING_LLUT_TPP_
#define ETOOLS_HASHING_LLUT_TPP_
#include "llut.hpp"
namespace etools::hashing{
    namespace details{
        template <typename KeyType, KeyType... Keys>
        constexpr std::size_t llut_impl<KeyType, Keys...>::keys() noexcept{
            return sizeof...(Keys);
        } 

        template <typename KeyType, KeyType... Keys>
        constexpr std::size_t llut_impl<KeyType, Keys...>::not_found() noexcept{
            return keys();
        }

        template <typename KeyType, KeyType... Keys>
        constexpr std::size_t llut_impl<KeyType, Keys...>::size() noexcept{
            return meta::tpack_max<KeyType, Keys...>() + 1;
        }

        template <typename KeyType, KeyType... Keys>
        constexpr std::size_t llut_impl<KeyType, Keys...>::operator()(KeyType key) const noexcept{
            const std::size_t k = static_cast<std::size_t>(key);
            if (k >= size()) return not_found();
            const index_t v = _table[k];
            return (v == static_cast<index_t>(keys())) ? not_found() : static_cast<std::size_t>(v);
        }

        template <typename KeyType, KeyType... Keys>
        constexpr llut_impl<KeyType, Keys...>::llut_impl() noexcept 
            : _table{make_table()}
        {    
        }

        template <typename KeyType, KeyType... Keys>
        constexpr auto llut_impl<KeyType, Keys...>::make_table() noexcept -> std::array<index_t, size()>{
            #ifndef ETOOLS_SKIP_CONSTEXPR_DISTINCT_CHECK
                constexpr std::array<KeyType, keys()> klist{{ Keys... }};
                static_assert(meta::all_distinct_fast(klist), "Keys must be distinct");
            #endif
            std::array<index_t, size()> table{};
            for (std::size_t i = 0; i < size(); i++) table[i] = static_cast<index_t>(not_found());
            std::size_t idx = 0;
            (void)std::initializer_list<int>{
                (table[static_cast<std::size_t>(Keys)] = static_cast<index_t>(idx++), 0)...
            };
            return table;
        }

    }

    template<typename KeyType>
    template<KeyType... Keys>
    constexpr const details::llut_impl<KeyType, Keys...>& llut<KeyType>::instance() noexcept
    {
        return details::llut_impl_singleton<KeyType, Keys...>;
    }
}
#endif // ETOOLS_HASHING_LLUT_TPP_