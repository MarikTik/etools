// SPDX-License-Identifier: MIT
/**
* @file typelist.tpp
*
* @brief implementation of typelist methods.
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2026-08-26
*
* @copyright
* MIT License
* Copyright (c) 2026 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_META_TYPELIST_TPP_
#define ETOOLS_META_TYPELIST_TPP_
#include "typelist.hpp"

namespace etools::meta {

    template<typename... Ts>
    constexpr std::size_t typelist<Ts...>::size() noexcept {
        return sizeof...(Ts);
    }

    template<typename... Ts>
    constexpr bool typelist<Ts...>::is_empty() noexcept {
        return sizeof...(Ts) == 0;
    }

    template<typename... Ts>
    template<typename T>
    constexpr bool typelist<Ts...>::contains() noexcept {
        return (std::is_same_v<T, Ts> || ...);
    }

    template<typename... Ts>
    template<template<typename> class Predicate>
    constexpr bool typelist<Ts...>::any_of() noexcept {
        return (static_cast<bool>(Predicate<Ts>::value) || ...);
    }

    template<typename... Ts>
    template<template<typename> class Predicate>
    constexpr bool typelist<Ts...>::all_of() noexcept {
        return (static_cast<bool>(Predicate<Ts>::value) && ...);
    }

    template<typename... Ts>
    template<typename Other>
    constexpr bool typelist<Ts...>::disjoint() noexcept {
        return (!details::tl_contains<Other, Ts>::value && ...);
    }

} // namespace etools::meta

#endif // ETOOLS_META_TYPELIST_TPP_
