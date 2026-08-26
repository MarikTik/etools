// SPDX-License-Identifier: MIT
/**
* @file typelist.hpp
*
* @brief Defines a compile-time container for a parameter pack of types, and the
* operations that query and combine such containers.
*
* @ingroup etools_meta etools::meta
*
* This header introduces the `etools::meta::typelist` struct - a utility
* that encapsulates a sequence of types at compile time. It is designed for use
* in template metaprogramming, compile-time dispatch, and type-based computation.
*
* Unlike `std::tuple`, this structure does not instantiate objects or store any
* data - it only carries type information and can be used for operations such as
* indexing, filtering, and expansion.
*
* Operations are exposed as members of `typelist` itself. Every one is `static`,
* so they are reached through the type and need no object. The struct stays empty
* and trivially constructible: `static` members add no storage, and member
* templates that are never named are never instantiated.
*
* ## Available operations
*
* | Operation | Answers |
* |-----------|---------|
* | `List::size()`                  | how many types are in the list |
* | `List::is_empty()`              | is the list empty |
* | `List::contains<T>()`           | does `T` appear in the list |
* | `List::any_of<Predicate>()`     | does at least one type satisfy `Predicate` |
* | `List::all_of<Predicate>()`     | do all types satisfy `Predicate` |
* | `List::disjoint<Other>()`       | do the two lists share no type |
* | `List::at<N>`                   | the type at index `N` |
* | `List::apply<Target>`           | `Target<Ts...>` - instantiate a template with the list |
* | `List::concat<Others...>`       | the lists joined end to end |
*
* The last three are type aliases rather than functions, because a C++ function
* cannot return a type.
*
* @note This utility is conceptually similar to `type_list` in other metaprogramming libraries.
*
* ### Example usage:
* @code
* #include "typelist.hpp"
* #include <tuple>
* #include <type_traits>
* using namespace etools::meta;
*
* // Define a typelist of some fundamental types
* using my_types = typelist<int, double, char>;
*
* static_assert(my_types::size() == 3, "Should contain 3 types");
* static_assert(not my_types::is_empty());
* static_assert(my_types::contains<double>());
*
* // Feed the list to any variadic template
* static_assert(std::is_same_v<my_types::apply<std::tuple>,
*                              std::tuple<int, double, char>>);
*
* // Ask a question of every element
* static_assert(my_types::all_of<std::is_arithmetic>());
* static_assert(my_types::any_of<std::is_floating_point>());
*
* // Index into the list
* static_assert(std::is_same_v<my_types::at<1>, double>);
* @endcode
*
* @warning Inside a template, where the list is a dependent type, C++ requires
* the `template` (and for aliases, `typename`) disambiguator - see the `typelist`
* docs for the exact form.
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2025-07-03
*
* @par Changelog
* - 2026-08-26: Added the operation set `size`, `is_empty`, `contains`, `at`,
*   `apply`, `any_of`, `all_of`, `disjoint` and `concat` as members of `typelist`.
*   The struct remains empty and trivially constructible.
*
* @copyright
* MIT License
* Copyright (c) 2025 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_META_TYPELIST_HPP_
#define ETOOLS_META_TYPELIST_HPP_
#include <cstddef>      // std::size_t
#include <type_traits>  // std::is_same_v, std::bool_constant
#include "traits.hpp"   // nth_t, backing the at<N> alias


namespace etools::meta {

    /// @brief Forward declaration; the definition follows the helpers it relies on.
    template<typename... Ts>
    struct typelist;

    namespace details {

        // -----------------------------------------------------------------
        // tl_contains: membership test
        // -----------------------------------------------------------------

        /**
        * @brief Determines whether `T` occurs in a `typelist`.
        *
        * A helper rather than an inlined expression because `disjoint` must ask
        * this question of a *second* list: reaching `Other`'s pack requires
        * partial specialization, which a member alias template cannot do.
        *
        * @tparam List A `typelist<...>`.
        * @tparam T    The type to look for.
        */
        template<typename List, typename T>
        struct tl_contains;

        template<typename... Ts, typename T>
        struct tl_contains<typelist<Ts...>, T>
            : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};


        // -----------------------------------------------------------------
        // tl_concat: join any number of typelists
        // -----------------------------------------------------------------

        /**
        * @brief Joins zero or more typelists into one.
        *
        * Folds left to right, two lists at a time, so the recursion depth is
        * linear in the *number of lists* rather than in their total length.
        *
        * @tparam Lists The typelists to join.
        */
        template<typename... Lists>
        struct tl_concat;

        /// @brief Base case: no lists at all joins to the empty list.
        template<>
        struct tl_concat<> { using type = typelist<>; };

        /// @brief Base case: a single list is already the answer.
        template<typename... Ts>
        struct tl_concat<typelist<Ts...>> { using type = typelist<Ts...>; };

        /// @brief Recursive case: merge the first two lists, then continue.
        template<typename... Ls, typename... Rs, typename... Rest>
        struct tl_concat<typelist<Ls...>, typelist<Rs...>, Rest...>
            : tl_concat<typelist<Ls..., Rs...>, Rest...> {};

    } // namespace details


    /**
    * @struct typelist
    * @brief A container for a variadic list of types, used for compile-time type manipulation.
    *
    * The `typelist` struct stores types as a parameter pack without instantiating them.
    * It serves as a compile-time-only construct, useful for performing metaprogramming tasks
    * such as static dispatch, filtering, or trait extraction.
    *
    * Operations are exposed as `static constexpr` member functions (for the ones
    * yielding a value) and member type aliases (for the ones yielding a type).
    * Every member is `static`, so no object is required:
    *
    * ```cpp
    * using lst = etools::meta::typelist<int, bool, float>;
    * static_assert(lst::contains<bool>());
    * static_assert(lst::size() == 3);
    * using second = lst::at<1>;   // bool
    * ```
    *
    * The type remains empty and trivially constructible - `static` members add no
    * storage, and member templates that are never named are never instantiated.
    *
    * @warning Inside a template, where the list itself is a dependent type, C++
    * requires the `template` (and for aliases, `typename`) disambiguator:
    * ```cpp
    * template<typename List>
    * struct consumer {
    *     static constexpr bool has_int = List::template contains<int>();
    *     using manager = typename List::template apply<my_manager>;
    * };
    * ```
    * Omitting `template` there parses `<` as less-than and fails to compile.
    *
    * @tparam Ts The types to store in the typelist.
    */
    template<typename... Ts>
    struct typelist {

        /**
        * @brief The number of types held by this list.
        *
        * ```cpp
        * static_assert(typelist<int, char, bool>::size() == 3);
        * static_assert(typelist<>::size() == 0);
        * ```
        *
        * @return The element count.
        *
        * @see is_empty
        */
        [[nodiscard]] static constexpr std::size_t size() noexcept;

        /**
        * @brief Whether this list holds no types.
        *
        * ```cpp
        * static_assert(typelist<>::is_empty());
        * static_assert(not typelist<int>::is_empty());
        * ```
        *
        * @return `true` iff the list is empty.
        *
        * @see size
        */
        [[nodiscard]] static constexpr bool is_empty() noexcept;

        /**
        * @brief Whether `T` occurs at least once in this list.
        *
        * Membership is decided by `std::is_same_v`, so cv-qualifiers and
        * references distinguish types: `typelist<int>::contains<const int>()`
        * is `false`.
        *
        * ```cpp
        * static_assert(typelist<int, char>::contains<char>());
        * static_assert(not typelist<int, char>::contains<bool>());
        * static_assert(not typelist<>::contains<int>());   // empty contains nothing
        * ```
        *
        * @tparam T The type to look for.
        * @return `true` iff `T` is in the list.
        */
        template<typename T>
        [[nodiscard]] static constexpr bool contains() noexcept;

        /**
        * @brief Whether at least one type in this list satisfies `Predicate`.
        *
        * ```cpp
        * using nums = typelist<int, double, char>;
        * static_assert(nums::any_of<std::is_floating_point>());
        * static_assert(not typelist<>::any_of<std::is_floating_point>());
        * ```
        *
        * @tparam Predicate Unary metafunction exposing a `::value` convertible to `bool`.
        * @return `true` iff any element satisfies `Predicate`. Empty list yields `false`.
        *
        * @warning `Predicate` is instantiated for every element, so it must be
        * well-formed for all of them - it cannot be used as a SFINAE filter.
        *
        * @see all_of
        */
        template<template<typename> class Predicate>
        [[nodiscard]] static constexpr bool any_of() noexcept;

        /**
        * @brief Whether every type in this list satisfies `Predicate`.
        *
        * ```cpp
        * using nums = typelist<int, double, char>;
        * static_assert(nums::all_of<std::is_arithmetic>());
        * static_assert(not nums::all_of<std::is_floating_point>());
        * static_assert(typelist<>::all_of<std::is_floating_point>());  // vacuously true
        * ```
        *
        * @tparam Predicate Unary metafunction exposing a `::value` convertible to `bool`.
        * @return `true` iff all elements satisfy `Predicate`. Empty list yields `true`.
        *
        * @warning `Predicate` is instantiated for every element, so it must be
        * well-formed for all of them - it cannot be used as a SFINAE filter.
        *
        * @see any_of
        */
        template<template<typename> class Predicate>
        [[nodiscard]] static constexpr bool all_of() noexcept;

        /**
        * @brief Whether this list shares no type with `Other`.
        *
        * Type identity only. Two distinct types that merely agree on some
        * property (an id constant, a tag member) are *not* considered shared -
        * express that with `any_of` and a predicate of your own.
        *
        * ```cpp
        * static_assert(typelist<int, char>::disjoint<typelist<bool, float>>());
        * static_assert(not typelist<int, char>::disjoint<typelist<char>>());
        * static_assert(typelist<>::disjoint<typelist<int>>());   // empty is disjoint with all
        * ```
        *
        * @tparam Other A `typelist<...>` to compare against.
        * @return `true` iff no type occurs in both lists.
        *
        * @note Repeats within either list do not affect the result.
        *
        * @see contains
        */
        template<typename Other>
        [[nodiscard]] static constexpr bool disjoint() noexcept;

        /**
        * @brief The type stored at index `N` of this list.
        *
        * A type alias rather than a function: a C++ function cannot return a type.
        *
        * ```cpp
        * using nums = typelist<int, double, char>;
        * static_assert(std::is_same_v<nums::at<0>, int>);
        * static_assert(std::is_same_v<nums::at<2>, char>);
        * ```
        *
        * @tparam N Zero-based index. Must be less than `size()`.
        *
        * @pre `N < size()`; an out-of-range index fails a `static_assert`
        * inside `nth`, not this alias.
        *
        * @see nth_t, size
        */
        template<std::size_t N>
        using at = nth_t<N, Ts...>;

        /**
        * @brief `Target` instantiated with this list's types, in order.
        *
        * Replaces writing a bespoke metafunction whose only job is to unpack a
        * list into one specific template. A type alias rather than a function,
        * for the same reason as `at`.
        *
        * ```cpp
        * template<typename...> struct manager {};
        *
        * using tasks = typelist<int, char>;
        * static_assert(std::is_same_v<tasks::apply<manager>, manager<int, char>>);
        * static_assert(std::is_same_v<typelist<>::apply<manager>, manager<>>);
        * ```
        *
        * @tparam Target The variadic template to instantiate.
        *
        * @note `Target` must accept exactly the list's types; a template with a
        * non-type or template-template parameter cannot be used here.
        */
        template<template<typename...> class Target>
        using apply = Target<Ts...>;

        /**
        * @brief This list joined end to end with `Others`, preserving order.
        *
        * Duplicates are kept: concatenation is not a set union. Feed the result
        * to `unique_typelist_t` if distinctness is required.
        *
        * ```cpp
        * using joined = typelist<int>::concat<typelist<char, bool>>;
        * static_assert(std::is_same_v<joined, typelist<int, char, bool>>);
        * static_assert(std::is_same_v<typelist<int>::concat<>, typelist<int>>);
        * ```
        *
        * @tparam Others Zero or more typelists to append, in order.
        *
        * @see unique_typelist_t
        */
        template<typename... Others>
        using concat = typename details::tl_concat<typelist<Ts...>, Others...>::type;
    };

} // namespace etools::meta

#include "typelist.tpp"
#endif // ETOOLS_META_TYPELIST_HPP_
