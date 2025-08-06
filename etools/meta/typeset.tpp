/**
* @file typeset.tpp
*
* @brief implementation of typeset.tpp methods.
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2025-08-06
*
* @copyright
* MIT License
* Copyright (c) 2025 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_META_TYPESET_TPP_
#define ETOOLS_META_TYEPSET_TPP_
#include "typeset.hpp"

namespace etools::meta{
    template <typename... Types>
    template <typename T>
    constexpr bool typeset<Types...>::test() const noexcept{
        static_assert((std::is_same_v<T, Types> || ...), "The type T is not part of the typeset.");
        constexpr std::size_t index = type_to_index<T, Types...>::value;
        return _bits.test(index);
    }

    template <typename... Types>
    template <typename T>
    constexpr void typeset<Types...>::set() noexcept
    {
        static_assert((std::is_same_v<T, Types> || ...), "The type T is not part of the typeset.");
        constexpr std::size_t index = type_to_index<T, Types...>::value;
        _bits.set(index);
    }

    template<typename... Types>
    template<typename T>
    constexpr void typeset<Types...>::reset() noexcept{
        static_assert((std::is_same_v<T, Types> || ...), "The type T is not part of the typeset.");
        constexpr std::size_t index = type_to_index<T, Types...>::value;
        _bits.reset(index);
    }
} // namespace etools::meta

#endif //ETOOLS_META_TYEPSET_TPP_