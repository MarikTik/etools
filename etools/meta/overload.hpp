// SPDX-License-Identifier: MIT
/**
* @file overload.hpp
*
* @brief Aggregates multiple callables into a single overload set.
*
* @ingroup etools_meta etools::meta
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2026-07-14
*
* @copyright
* MIT License
* Copyright (c) 2025 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_META_OVERLOAD_HPP_
#define ETOOLS_META_OVERLOAD_HPP_

namespace etools::meta {

    /**
    * @struct overload
    *
    * @brief Aggregates multiple callables into a single overload set.
    *
    * The standard C++17 "overloaded visitor" idiom: inherits `operator()` from
    * every type in `Fs...`, exposing all of them through a single callable type.
    * The primary use case is `std::visit` - one lambda per variant alternative
    * instead of a monolithic visitor that must dispatch via `std::holds_alternative`
    * or a chain of `if constexpr` branches.
    *
    * @tparam Fs... Callable types (typically lambdas) whose `operator()` should
    *               be merged into the overload set. Each `Fs` must have a distinct,
    *               non-ambiguous `operator()`.
    *
    * @note The companion deduction guide makes explicit template arguments
    *       unnecessary: `overload{f1, f2, ...}` deduces `<F1, F2, ...>` automatically.
    *
    * @note Every `Fs` must be copy- or move-constructible for aggregate initialisation
    *       to work; stateful lambdas captured by value satisfy this trivially.
    *
    * @warning Ambiguity is a compile-time error: if two callables in `Fs...` accept
    *          the same argument types, the overload set is ill-formed and the compiler
    *          will report the conflict at the call site.
    *
    * Example:
    * ```cpp
    * #include "etools/meta/overload.hpp"
    *
    * std::variant<int, float, std::string> v = 3.14f;
    *
    * std::visit(etools::meta::overload{
    *     [](int i)                { handle int    },
    *     [](float f)              { handle float   },
    *     [](const std::string& s) { handle string },
    * }, v);
    * ```
    */
    template<typename... Fs>
    struct overload : Fs... {
        using Fs::operator()...;
    };

    /**
    * @brief Deduction guide for `overload`.
    *
    * Allows `overload{f1, f2, ...}` without spelling out the template arguments.
    *
    * @tparam Fs... Callable types deduced from the constructor arguments.
    */
    template<typename... Fs>
    overload(Fs...) -> overload<Fs...>;

} // namespace etools::meta

#endif // ETOOLS_META_OVERLOAD_HPP_
