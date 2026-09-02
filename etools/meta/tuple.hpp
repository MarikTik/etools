// SPDX-License-Identifier: MIT
/**
* @file tuple.hpp
*
* @brief A heterogeneous container whose template instantiation depth does not
*        grow with the number of elements.
*
* @ingroup etools_meta etools::meta
*
* @details
* `std::tuple` is specified as a recursive chain: `_Tuple_impl<I, Head, Tail...>`
* derives from `_Tuple_impl<I+1, Tail...>`, so an N-element tuple is an N-deep
* inheritance hierarchy. Depth costs the compiler more than it looks: each level
* is a distinct class template instantiation, `get<I>` walks I of them, and the
* whole chain is re-examined for every operation over the tuple.
*
* This is a **flat** layout instead. One `leaf<I, T>` per element, all of them
* *direct* bases of a single class, indexed by `std::index_sequence`. Depth is
* O(1) whatever N is; the compiler instantiates N independent leaves rather than
* one N-deep chain, and `get<I>` resolves by overload against the unique
* `leaf<I, T>` base - a single deduction step, not an I-step walk.
*
* ## Why this exists
*
* `dispatch_factory` holds one `std::array<std::optional<T>, N>` per registered
* type. With a task-per-type registry that pack is the size of the whole schema,
* and the recursive layout dominated compile time. Measured with GCC 15 on
* `std::tuple<std::array<std::optional<T_i>, 1>...>`:
*
*     elements | std::tuple        | meta::tuple       | speedup
*          260 |   1.94 s,  383 MB |   0.60 s,  205 MB |    3.2x
*          520 |   9.72 s,  877 MB |   1.34 s,  374 MB |    7.3x
*         1040 | does not compile  |   2.77 s,  714 MB |      -
*         2080 | does not compile  |   5.83 s, 1395 MB |      -
*
* Time scales as roughly N^2.3 for `std::tuple` and N^1.1 here. Past ~1000
* elements `std::tuple` exceeds GCC's default `-ftemplate-depth` of 900 and
* fails outright, which put a hard ceiling on schema size that had nothing to do
* with the target device.
*
* In the real factory the effect is smaller but still decisive - a 400-task
* etask project went from 153.7 s / 1,488 MB to 57.2 s / 1,005 MB in the
* translation unit that instantiates the manager. Emitted firmware was unchanged
* (256,465 vs 256,497 bytes on ESP32), as it must be: this is a compile-time
* representation, not a runtime one.
*
* ## What it is not
*
* Not a `std::tuple` replacement. There is no comparison, no `tuple_cat`, no
* structured-binding support, and no `std::tuple_size`/`tuple_element`
* specialisation. It offers indexed access and a whole-container `for_each`,
* which is all `dispatch_factory` needs. Reach for `std::tuple` unless the pack
* is large enough for the depth to matter.
*
* @note Elements are stored in declaration order but the standard grants no
*       layout guarantee across distinct base classes, so do not depend on the
*       address order of elements.
* @note Two identical types are kept distinct by the index in `leaf<I, T>`, so
*       `tuple<int, int>` is well-formed.
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2025-09-01
*
* @copyright
* MIT License
* Copyright (c) 2025 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_META_TUPLE_HPP_
#define ETOOLS_META_TUPLE_HPP_

#include <cstddef>
#include <type_traits>
#include <utility>

namespace etools::meta {

    namespace detail {

        /// @brief One element, tagged by its index so two equal types stay distinct bases.
        ///
        /// @note `T value{}` would be wrong here: a default member initialiser is
        ///       part of the class definition, so it is required to be valid as
        ///       soon as `leaf` is instantiated - which makes merely *naming*
        ///       `tuple<T>` an error for any `T` that is not default-constructible,
        ///       even where the tuple is never default-constructed. Leaving the
        ///       member uninitialised keeps that requirement where the standard
        ///       puts it, on the constructor, and `tuple`'s own `= default`
        ///       constructor is then implicitly deleted for such a `T` rather
        ///       than ill-formed.
        template<std::size_t I, typename T>
        struct leaf {
            T value;
        };

        /// @brief All leaves as direct bases: depth 1, not depth N.
        template<typename Seq, typename... Ts>
        struct flat_impl;

        template<std::size_t... Is, typename... Ts>
        struct flat_impl<std::index_sequence<Is...>, Ts...> : leaf<Is, Ts>... {};

    } // namespace detail

    template<typename... Ts>
    struct tuple
        : detail::flat_impl<std::index_sequence_for<Ts...>, Ts...>
    {
        static constexpr std::size_t size = sizeof...(Ts);
    };

    /// @brief Indexed access. Deduces T from the unique `leaf<I, T>` base.
    template<std::size_t I, typename T>
    constexpr T& get(detail::leaf<I, T>& l) noexcept { return l.value; }

    template<std::size_t I, typename T>
    constexpr const T& get(const detail::leaf<I, T>& l) noexcept { return l.value; }

    template<typename Fn, typename... Ts, std::size_t... Is>
    constexpr void for_each_impl(tuple<Ts...>& t, Fn&& fn, std::index_sequence<Is...>)
    {
        (fn(get<Is>(t)), ...);
    }

    /// @brief Applies `fn` to every element. Replaces `std::apply` for folds
    ///        over the whole tuple; one pack expansion, no recursion.
    template<typename Fn, typename... Ts>
    constexpr void for_each(tuple<Ts...>& t, Fn&& fn)
    {
        for_each_impl(t, std::forward<Fn>(fn), std::index_sequence_for<Ts...>{});
    }

} // namespace etools::meta

#endif // ETOOLS_META_TUPLE_HPP_
