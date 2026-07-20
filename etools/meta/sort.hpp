// SPDX-License-Identifier: MIT
/**
* @file sort.hpp
*
* @brief Compile-time stable sort of a parameter pack using a comparator metafunction.
*
* @ingroup etools_meta etools::meta
*
* Provides `sort_t<Cmp, Ts...>` (and the struct form `sort<Cmp, Ts...>`)
* that rearranges the types in `Ts...` into a `typelist` ordered by the binary
* comparator `Cmp`.  A `typelist<Ts...>` may be passed in place of the bare pack.
*
* ## Comparator contract
*
* `Cmp` must be a template of the form:
* ```cpp
* template<typename T, typename U>
* struct MyCmp : std::bool_constant<T before U ?> {};
* ```
* It must satisfy strict weak ordering:
* - **Irreflexivity**: `Cmp<T, T>::value == false`.
* - **Asymmetry**: if `Cmp<T, U>::value` then `!Cmp<U, T>::value`.
* - **Transitivity**: if `Cmp<T,U>` and `Cmp<U,V>` then `Cmp<T,V>`.
*
* Types for which `Cmp<T,U>::value == false` AND `Cmp<U,T>::value == false` are
* considered equivalent; their relative order in the output matches their relative
* order in the input (the sort is **stable**).
*
* ## Built-in comparators
*
* | Comparator | Ordering |
* |------------|----------|
* | `size_greater<T,U>` | Largest `sizeof` first |
* | `size_less<T,U>` | Smallest `sizeof` first |
* | `align_greater<T,U>` | Strictest `alignof` first |
* | `align_less<T,U>` | Weakest `alignof` first |
*
* ## Example
* ```cpp
* #include "etools/meta/sort.hpp"
* using namespace etools::meta;
*
* // Sort largest-first (useful for struct-layout optimisation).
* using sorted = sort_t<size_greater, char, double, int, float>;
* static_assert(std::is_same_v<sorted, typelist<double, int, float, char>>);
*
* // Custom comparator: any constexpr boolean expression over T and U.
* template<typename T, typename U>
* struct prefer_trivial : std::bool_constant<
*     std::is_trivial_v<T> && !std::is_trivial_v<U>
* > {};
*
* using sorted2 = sort_t<prefer_trivial, std::string, int, std::vector<int>, float>;
* // trivial types (int, float) will precede non-trivial ones
* ```
*
* ## Algorithm
*
* Top-down merge sort.  The list is split sequentially at the midpoint
* (`tl_take<N/2>` left, `tl_drop<N/2>` right), each half is sorted recursively,
* and the two sorted halves are merged.  The sequential split ensures that every
* element in the left half has an earlier original position than every element in
* the right half; taking the left element first when two elements are equivalent
* therefore always picks the element that appeared earlier in the input.
*
* - Instantiation count: O(N log N) - a genuine improvement over insertion sort.
* - Maximum recursion depth: O(log N) - important for compilers with template depth limits.
* - Stability: preserved (see above).
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2026-07-20
*
* @copyright
* MIT License
* Copyright (c) 2026 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_META_SORT_HPP_
#define ETOOLS_META_SORT_HPP_
#include <type_traits>
#include "typelist.hpp"

namespace etools::meta {

    namespace details {

        // -----------------------------------------------------------------
        // tl_prepend: typelist<T, Ts...> from T and typelist<Ts...>
        // -----------------------------------------------------------------

        /**
        * @brief Prepends a single type `T` to a `typelist`.
        *
        * @tparam T    Type to prepend.
        * @tparam List A `typelist<...>` to prepend to.
        */
        template<typename T, typename List>
        struct tl_prepend;

        template<typename T, typename... Ts>
        struct tl_prepend<T, typelist<Ts...>> { using type = typelist<T, Ts...>; };


        // -----------------------------------------------------------------
        // tl_take / tl_drop: sequential split at position N
        // -----------------------------------------------------------------

        /**
        * @brief Collects the first `N` types of a `typelist` into a new `typelist`.
        *
        * A 4th `void`-tagged SFINAE parameter keeps the recursive specialization from
        * matching when `N == 0`, resolving what would otherwise be an ambiguous
        * partial specialisation.
        *
        * @tparam N    Number of types to take (0 yields the accumulator as-is).
        * @tparam List The source `typelist`.
        * @tparam Acc  Accumulator (starts empty; do not supply explicitly).
        */
        template<std::size_t N, typename List, typename Acc = typelist<>, typename = void>
        struct tl_take_impl { using type = Acc; };

        template<std::size_t N, typename H, typename... Ts, typename... As>
        struct tl_take_impl<N, typelist<H, Ts...>, typelist<As...>,
                            std::enable_if_t<(N > 0)>>
            : tl_take_impl<N - 1, typelist<Ts...>, typelist<As..., H>> {};

        /**
        * @brief Drops the first `N` types of a `typelist`, returning the remainder.
        *
        * Same SFINAE guard as `tl_take_impl`: the recursive case is disabled when
        * `N == 0`, so the primary (which returns `List` unchanged) is the unambiguous
        * winner at the base case.
        *
        * @tparam N    Number of types to drop (0 returns the list unchanged).
        * @tparam List The source `typelist`.
        */
        template<std::size_t N, typename List, typename = void>
        struct tl_drop_impl { using type = List; };

        template<std::size_t N, typename H, typename... Ts>
        struct tl_drop_impl<N, typelist<H, Ts...>, std::enable_if_t<(N > 0)>>
            : tl_drop_impl<N - 1, typelist<Ts...>> {};


        // -----------------------------------------------------------------
        // merge_impl: merge two Cmp-sorted typelists into one
        // -----------------------------------------------------------------

        /**
        * @brief Merges two `Cmp`-sorted typelists into a single sorted typelist.
        *
        * At each step, the head with higher priority per `Cmp` is prepended to the
        * merged tail.  When `Cmp<L, R>::value` is false and `Cmp<R, L>::value` is
        * also false (equivalent elements), `L` is taken first.  Because the
        * sequential split guarantees every element in L had an earlier original
        * position than every element in R, this rule preserves stability.
        *
        * @tparam Cmp   Binary comparator.
        * @tparam Left  A `Cmp`-sorted `typelist`.
        * @tparam Right A `Cmp`-sorted `typelist`.
        */
        template<template<typename, typename> class Cmp, typename Left, typename Right>
        struct merge_impl;

        /// @brief Base case: left is empty, result is right (possibly also empty).
        template<template<typename, typename> class Cmp, typename... Rs>
        struct merge_impl<Cmp, typelist<>, typelist<Rs...>> {
            using type = typelist<Rs...>;
        };

        /// @brief Base case: right is empty (and left is non-empty).
        template<template<typename, typename> class Cmp, typename L, typename... Ls>
        struct merge_impl<Cmp, typelist<L, Ls...>, typelist<>> {
            using type = typelist<L, Ls...>;
        };

        /// @brief Recursive case: compare heads and prepend the winner.
        template<template<typename, typename> class Cmp,
                 typename L, typename... Ls,
                 typename R, typename... Rs>
        struct merge_impl<Cmp, typelist<L, Ls...>, typelist<R, Rs...>> {
            using type = std::conditional_t<
                Cmp<R, L>::value,   // R strictly before L -> take R
                typename tl_prepend<R, typename merge_impl<Cmp, typelist<L, Ls...>, typelist<Rs...>>::type>::type,
                typename tl_prepend<L, typename merge_impl<Cmp, typelist<Ls...>, typelist<R, Rs...>>::type>::type
            >;
        };


        // -----------------------------------------------------------------
        // sort_impl: top-down merge sort on a typelist
        // -----------------------------------------------------------------

        /**
        * @brief Top-down merge sort on a `typelist`.
        *
        * Base cases (empty or singleton) return the list unchanged.
        * Otherwise, split sequentially at the midpoint, sort each half
        * recursively, then merge.
        *
        * @tparam Cmp  Binary comparator.
        * @tparam List The `typelist` to sort.
        */
        template<template<typename, typename> class Cmp, typename List>
        struct sort_impl;

        /// @brief Base case: empty list.
        template<template<typename, typename> class Cmp>
        struct sort_impl<Cmp, typelist<>> { using type = typelist<>; };

        /// @brief Base case: singleton list.
        template<template<typename, typename> class Cmp, typename T>
        struct sort_impl<Cmp, typelist<T>> { using type = typelist<T>; };

        /// @brief Recursive case: sequential split at midpoint, sort, merge.
        template<template<typename, typename> class Cmp, typename T1, typename T2, typename... Ts>
        struct sort_impl<Cmp, typelist<T1, T2, Ts...>> {
        private:
            static constexpr std::size_t half = (2 + sizeof...(Ts)) / 2;
            using list          = typelist<T1, T2, Ts...>;
            using left_list     = typename tl_take_impl<half, list>::type;
            using right_list    = typename tl_drop_impl<half, list>::type;
            using sorted_left   = typename sort_impl<Cmp, left_list>::type;
            using sorted_right  = typename sort_impl<Cmp, right_list>::type;
        public:
            using type = typename merge_impl<Cmp, sorted_left, sorted_right>::type;
        };

    } // namespace details


    // -------------------------------------------------------------------------
    // Built-in comparators
    // -------------------------------------------------------------------------

    /**
    * @brief Comparator: `T` before `U` iff `sizeof(T) > sizeof(U)`. Largest types first.
    *
    * Useful for ordering struct members to minimise padding (place widest fields first).
    */
    template<typename T, typename U>
    struct size_greater : std::bool_constant<(sizeof(T) > sizeof(U))> {};

    /**
    * @brief Comparator: `T` before `U` iff `sizeof(T) < sizeof(U)`. Smallest types first.
    */
    template<typename T, typename U>
    struct size_less : std::bool_constant<(sizeof(T) < sizeof(U))> {};

    /**
    * @brief Comparator: `T` before `U` iff `alignof(T) > alignof(U)`. Strictest alignment first.
    *
    * Useful for ordering sections in a heterogeneous buffer by alignment requirement.
    */
    template<typename T, typename U>
    struct align_greater : std::bool_constant<(alignof(T) > alignof(U))> {};

    /**
    * @brief Comparator: `T` before `U` iff `alignof(T) < alignof(U)`. Weakest alignment first.
    */
    template<typename T, typename U>
    struct align_less : std::bool_constant<(alignof(T) < alignof(U))> {};


    // -------------------------------------------------------------------------
    // Public interface
    // -------------------------------------------------------------------------

    /**
    * @struct sort
    *
    * @brief Stable compile-time sort of a parameter pack, ordered by `Cmp`.
    *
    * `type` is a `typelist<...>` containing the same types as `Ts...` but
    * reordered so that for any two adjacent elements `A`, `B` in the result,
    * `Cmp<B, A>::value` is `false` (i.e. `B` does not strictly precede `A`).
    *
    * @tparam Cmp  Binary comparator metafunction (see file-level contract).
    * @tparam Ts   The types to sort. May also be a single `typelist<Ts...>`.
    *
    * @see sort_t
    */
    template<template<typename, typename> class Cmp, typename... Ts>
    struct sort {
        using type = typename details::sort_impl<Cmp, typelist<Ts...>>::type;
    };

    /**
    * @brief Partial specialisation accepting a `typelist<Ts...>` in place of a bare pack.
    *
    * Allows `sort_t<Cmp, typelist<A, B, C>>` as an alternative to `sort_t<Cmp, A, B, C>`.
    * The two forms produce identical results.
    *
    * @tparam Cmp  Binary comparator metafunction.
    * @tparam Ts   Types carried inside the typelist.
    */
    template<template<typename, typename> class Cmp, typename... Ts>
    struct sort<Cmp, typelist<Ts...>> {
        using type = typename details::sort_impl<Cmp, typelist<Ts...>>::type;
    };

    /**
    * @brief Convenience alias for `sort<Cmp, Ts...>::type`.
    *
    * @tparam Cmp  Binary comparator metafunction.
    * @tparam Ts   Types to sort (bare pack or a single `typelist<...>`).
    *
    * ```cpp
    * using sorted = etools::meta::sort_t<etools::meta::size_greater,
    *                                     char, double, int, float>;
    * // typelist<double, int, float, char>
    * ```
    */
    template<template<typename, typename> class Cmp, typename... Ts>
    using sort_t = typename sort<Cmp, Ts...>::type;

} // namespace etools::meta

#endif // ETOOLS_META_SORT_HPP_
