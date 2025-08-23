// SPDX-License-Identifier: MIT
/**
* @file traits.hpp
*
* @brief Provides custom type traits for template metaprogramming.
*
* @ingroup etools_meta etools::meta
*
* This file defines additional type traits outside of the C++ standard library.
* These traits are designed for use in modern template metaprogramming,
* following the same structure and naming conventions as standard library traits.
* 
* @note These traits reside in the `etools::meta` namespace to avoid collision
* with standard traits and other libraries.
* 
* @warning This file is intended to use on platforms supporting C++17 standard or later.
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2025-07-03
*
* @copyright
* MIT License
* Copyright (c) 2025 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/

#ifndef ETOOLS_META_TRAITS_HPP_
#define ETOOLS_META_TRAITS_HPP_
#include <type_traits> // For std::true_type, std::bool_constant, std::is_same_v
#include <limits> // For std::numeric_limits
#include <cstdint> // For std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t
#include <cstddef> // For size_t
#include <utility> // For a bunch of stuff

 
#ifndef __has_builtin
    #define __has_builtin(x) 0
#endif

namespace etools::meta {
    
    /**
    * @struct is_distinct
    * @brief Checks whether a pack of types is composed of distinct types.
    *
    * This type trait determines at compile-time whether all types provided in the parameter pack
    * are unique (i.e., no duplicates). The check is performed recursively using fold expressions
    * and `std::is_same_v`.
    *
    * The result is accessible via the `::value` member or via the `is_distinct_v` alias.
    *
    * @tparam Ts The parameter pack of types to check for uniqueness.
    *
    * @note This trait is useful when implementing compile-time type sets or constraints where
    * repeated types would cause ambiguity or incorrect behavior.
    *
    * @see is_distinct_v
    */
    template <typename...>
    struct is_distinct : std::true_type {};
    
    /**
    * @brief Recursive specialization of `is_distinct` to check for duplicate types.
    *
    * Evaluates whether the current type `T` is not the same as any of the remaining types,
    * and then recursively checks the rest.
    *
    * @tparam T The first type to compare.
    * @tparam Rest The remaining types in the parameter pack.
    */
    template <typename T, typename... Rest>
    struct is_distinct<T, Rest...> : std::bool_constant<
    (!std::is_same_v<T, Rest> && ...) && is_distinct<Rest...>::value
    > {};
    
    /**
    * @var is_distinct_v
    * @brief Convenience variable template for `is_distinct<Ts...>::value`.
    *
    * Evaluates to `true` if all types in the pack `Ts...` are distinct, `false` otherwise.
    *
    * @tparam Ts The types to check for uniqueness.
    *
    * @see is_distinct
    */
    template <typename... Ts>
    inline constexpr bool is_distinct_v = is_distinct<Ts...>::value;
    
    
    /**
    * @brief Helper function to retrieve the underlying value of an enum.
    * 
    * This function template takes an enum value and returns its underlying type.
    * 
    * @tparam T an `enum class` type.
    * 
    * @param v the enum value to convert.
    * 
    */
    template<typename T>
    constexpr std::underlying_type_t<T> underlying_v(T v){
        return static_cast<std::underlying_type_t<T>>(v);
    }
    
    /**
    * @var always_false_v
    * @brief Template-dependent compile-time false value for triggering conditional static_assert.
    *
    * This utility is used in `if constexpr` or other SFINAE-based contexts to intentionally
    * trigger a `static_assert` only when a specific template branch is instantiated.
    * 
    * Unlike `false` or `std::false_type::value`, this variable is *dependent* on the template
    * parameter `T`, ensuring that the compiler will only evaluate it when that branch is chosen.
    * 
    * This is particularly useful in generic functions or traits where a catch-all `else` branch
    * should cause a compile-time error only if it is actually reached.
    *
    * @tparam T A template type used to make the expression type-dependent.
    *
    * @code
    * template <typename T>
    * void process(const T&) {
    *     if constexpr (std::is_integral_v<T>) {
    *         // Handle integers
    *     } else {
    *         static_assert(always_false_v<T>, "Unsupported type in process()");
    *     }
    * }
    * @endcode
    *
    * @note This is a common metaprogramming idiom adopted by many modern C++ codebases.
    */
    template <typename T>
    constexpr bool always_false_v = false;
    
    
    /**
    * @brief Provides a member typedef `type` that names `T`.
    *
    * This struct performs the identity transformation on a type `T`, effectively
    * returning the same type. It's particularly useful in template metaprogramming
    * to prevent type deduction in certain contexts.
    *
    * @tparam T The type to be encapsulated.
    */
    template <class T>
    struct type_identity {
        using type = T; /**< The encapsulated type. */
    };
    
    /**
    * @brief Helper alias template for `type_identity`.
    *
    * Provides a convenient way to access the encapsulated type without explicitly
    * specifying `::type`.
    *
    * @tparam T The type to be encapsulated.
    */
    template <class T>
    using type_identity_t = typename type_identity<T>::type;
    
    
    // /**
    // * @struct template_parameter_of
    // *
    // * @brief Extracts the inner type of one or more template instantiations,
    // *        verifying that all inner types are the same.
    // *
    // * This struct extracts the type parameter of any template class that takes
    // * a single type argument (e.g. `task<int>`). When passed multiple template
    // * instances, it performs a static check to ensure all inner types are identical.
    // * Compilation fails if any types differ.
    // *
    // * @tparam Ts... A variadic pack of template instantiations to examine.
    // *
    // * @note Only templates of the form `Template<T>` are supported.
    // * @note Use `template_parameter_of_t<Ts...>` for a convenient alias.
    // */
    // template<typename... Ts>
    // struct template_parameter_of;
    
    // /// @brief Specialization for multiple template instantiations.
    // /// @tparam Arg the inner type of the first template instantiation.
    // /// @tparam ...Rest the remaining template instantiations to check.
    // template<
    // template<typename> class Outer,
    // typename Arg,
    // typename... Rest
    // >
    // struct template_parameter_of<Outer<Arg>, Rest...>
    // {
    //     // static check that all other Ts have same inner type Arg
    //     static_assert(
    //         (std::is_same_v<typename template_parameter_of<Rest>::type, Arg> && ...),
    //         "template_parameter_of error: not all inner types are the same."
    //     );
    
    //     using type = Arg;
    // };
    
    // ///@brief Specialization for a single template instantiation.
    // ///@tparam Outer the template class.
    // ///@tparam Arg the inner type of the template instantiation.
    // template<
    // template<typename> class Outer,
    // typename Arg
    // >
    // struct template_parameter_of<Outer<Arg>>
    // {
    //     using type = Arg;
    // };
    
    // /// @brief Convenience alias for `template_parameter_of<Ts...>::type`.
    // /// @tparam Ts... A variadic pack of template instantiations.
    // /// 
    // /// This alias allows easy access to the extracted type without needing to
    // /// explicitly refer to `template_parameter_of<Ts...>::type`.
    // /// 
    // /// @note If the pack contains multiple template instantiations, it will
    // ///       perform a static check to ensure all inner types are the same.
    // template<typename... Ts>
    // using template_parameter_of_t = typename template_parameter_of<Ts...>::type;
    
    
    
    /**
    * @struct member
    *
    * @brief Extracts and validates the common type of a static member across multiple types.
    *
    * The `member` metafunction recursively:
    *
    * - invokes a user-supplied extractor metafunction on each type
    * - extracts the declared type of the static member
    * - verifies that all extracted types are identical
    *
    * Compilation fails if any types differ.
    *
    * @tparam Extractor
    *         A template metafunction of the form:
    *         @code
    *         template<typename T>
    *         struct Extractor {
    *             static constexpr auto value = T::member_name;
    *         };
    *         @endcode
    *   
    *
    * @tparam First
    *         The first type from which to extract the member value.
    *
    * @tparam Rest
    *         Zero or more additional types to check for type consistency.
    *
    * @note Only extracts the type of the static constant member, not its value.
    *
    * @note For convenient use, prefer the alias template `member_t`.
    */
    template<template<typename> class Extractor, typename First, typename... Rest>
    struct member
    {
        /**
        * @brief The common type extracted from the static member of all types.
        *
        * This alias resolves to the deduced type from:
        * @code
        * decltype(Extractor<T>::value)
        * @endcode
        * for each type `T` in the parameter pack.
        *
        * A static assertion guarantees that all types yield the same result.
        */
        using type = typename member<Extractor, First>::type;
        
        static_assert(
            (std::is_same_v<type, std::remove_cv_t<decltype(Extractor<Rest>::value)>> && ...),
            "member error: not all Extractor<Ts> yield the same type."
        );
    };
    
    /**
    * @struct member (single-type specialization)
    *
    * @brief Specialization of `member` for a single type.
    *
    * Provides the extracted member type from a single type via the supplied extractor.
    *
    * @tparam Extractor
    *         The extractor metafunction used to retrieve the static member.
    *
    * @tparam T
    *         The type whose static member is extracted.
    */
    template<template<typename> class Extractor, typename T>
    struct member<Extractor, T>
    {
        /// @brief The extracted type of the static member for the given type.
        using type = std::remove_cv_t<decltype(Extractor<T>::value)>;
    };
    
    /**
    * @brief Alias template for easier use of `member`.
    *
    * Simplifies extraction of the common member type across multiple types:
    *
    * Example usage:
    * ```
    * template<typename T>
    * struct Extractor {
    *     static constexpr auto value = T::member_name;
    * };
    * 
    * using member_type = member_t<Extractor, A, B, C>;
    * ```
    *
    * @tparam Extractor
    *         The extractor metafunction used to retrieve the static member.
    *
    * @tparam Ts...
    *         The types whose static members should be extracted and checked.
    *
    * @see member
    */
    template<template<typename> class Extractor, typename... Ts>
    using member_t = typename member<Extractor, Ts...>::type;
    
    
    #if ETOOLS_HAS_TYPE_PACK_ELEMENT
        /**
        * @brief Fast pack indexing using compiler builtin when available.
        *
        * @tparam I  Zero-based index into the parameter pack.
        * @tparam Ts The parameter pack of types.
        *
        * @note Ill-formed if `I >= sizeof...(Ts)` (as with standard aliases).
        */
        template <std::size_t I, class... Ts>
        using pack_at_t = __type_pack_element<I, Ts...>;
    #else
        namespace details {
            /**
            * @brief MI node carrying an index and a type.
            *
            * @tparam I Index tag.
            * @tparam T Type stored at index I.
            */
            template<std::size_t I, class T>
            struct indexed { using type = T; };

            /**
            * @brief Builds a single base class that inherits from `indexed<I, Ts>`...
            *        for all `I` and `Ts...` in the pack.
            */
            template<class Seq, class... Ts>
            struct indexer_impl;

            template<std::size_t... Is, class... Ts>
            struct indexer_impl<std::index_sequence<Is...>, Ts...> : indexed<Is, Ts>... {};

            /**
            * @brief Overload helper that selects the `indexed<I, T>` base.
            */
            template<std::size_t I, class T>
            indexed<I, T> pick(const indexed<I, T>&);

            /**
            * @brief Amortized O(N) “view” over a parameter pack, providing O(1) lookups.
            *
            * @tparam Ts Types in the pack.
            */
            template<class... Ts>
            struct type_pack_view {
                using indexer = indexer_impl<std::make_index_sequence<sizeof...(Ts)>, Ts...>;
                template<std::size_t I>
                using at = typename decltype(pick<I>(std::declval<indexer>()))::type;
            };

            /**
            * @brief Fast pack indexing using the MI fallback (portable).
            *
            * @tparam I  Zero-based index into the parameter pack.
            * @tparam Ts The parameter pack of types.
            *
            * @note Ill-formed if `I >= sizeof...(Ts)` (as with standard aliases).
            */
            template <std::size_t I, class... Ts>
            using pack_at_t = typename type_pack_view<Ts...>::template at<I>;
        } // namespace details
        #endif // ETOOLS_HAS_TYPE_PACK_ELEMENT

        namespace details{
            /**
            * @brief Implementation detail for `nth`: resolves to the N-th type.
            *
            * This thin wrapper delegates to a fast compiler builtin (`__type_pack_element`)
            * when available, otherwise to a portable multiple-inheritance fallback.
            *
            * @tparam N  Zero-based index to retrieve.
            * @tparam Ts Parameter pack of types.
            */
            template<std::size_t N, typename... Ts>
            struct nth_impl {
                using type = pack_at_t<N, Ts...>;
            };
        } // namespace details
        
        /**
        * @struct nth
        * @brief Retrieves the N-th type from a parameter pack at compile time.
        *
        * Provides `::type` equal to the type at index `N` in `Ts...`.
        *
        * @tparam N  Zero-based index.
        * @tparam Ts Parameter pack of types.
        *
        * @note Performs a friendly bounds check with `static_assert`. Under the hood,
        *       this uses a compiler builtin when present, otherwise a MI fallback.
        *
        * @see nth_t
        * ```cpp
        * using my_type = etools::meta::nth<1, int, double, float>::type; // double
        * ```
        */
        template<std::size_t N, typename... Ts>
        struct nth {
            static_assert(N < sizeof...(Ts), "Index out of bounds");
            using type = typename details::nth_impl<N, Ts...>::type;
        };
        
        /**
        * @brief Convenience alias for `nth<N, Ts...>::type`.
        *
        * @tparam N  Zero-based index.
        * @tparam Ts Parameter pack of types.
        *
        * @code
        * using my_type = etools::meta::nth_t<1, int, double, float>; // double
        * @endcode
        */
        template<std::size_t N, typename... Ts>
        using nth_t = typename nth<N, Ts...>::type;
        
        /**
        * @brief Type trait that resolves to the smallest unsigned integer type
        *        capable of holding a given constant value.
        *
        * This trait selects the smallest type among std::uint8_t, std::uint16_t,
        * std::uint32_t, and std::uint64_t that can represent the given value `V`.
        *
        * @tparam V The constant unsigned value to evaluate.
        *
        * @note This trait fails to compile if V exceeds the range of std::uint64_t.
        *
        * @example
        * ```
        * smallest_uint_t<100>       // resolves to std::uint8_t
        * smallest_uint_t<70000>     // resolves to std::uint32_t
        * ```
        */
        template <std::uintmax_t V>
        using smallest_uint_t =
        std::conditional_t<(V <= std::numeric_limits<std::uint8_t>::max()),  std::uint8_t,
        std::conditional_t<(V <= std::numeric_limits<std::uint16_t>::max()), std::uint16_t,
        std::conditional_t<(V <= std::numeric_limits<std::uint32_t>::max()), std::uint32_t,
        std::conditional_t<(V <= std::numeric_limits<std::uint64_t>::max()), std::uint64_t,
        void
        >
        >
        >
        >;
        
        /**
        * @brief Adds const qualifier to a type if the condition is true.
        * 
        * @tparam T The type to potentially add const to.
        * @tparam Condition If true, adds const to T.
        *
        * @note If the type already has a `const` qualifier, it will remain unchanged regardless of the condition.
        * 
        * Example:
        * ```cpp
        * using const_int = add_const_if_t<int, true>;  // const int
        * using non_const_int = add_const_if_t<int, false>; // int
        * ```
        */
        template<typename T, bool Condition>
        struct add_const_if{
            using type = T;
        };
        
        /**
        * @brief Specialization of `add_const_if` for when the condition is true.
        * Adds const to the type.
        */
        template<typename T>
        struct add_const_if<T, true>{
            using type = const T;
        };
        
        /**
        * @brief Alias template for `add_const_if`.
        * 
        * Provides a convenient way to use `add_const_if` without needing to specify `::type`.
        * @tparam T The type to potentially add const to.
        * @tparam Condition If true, adds const to T.
        * 
        * @note If the type already has a `const` qualifier, it will remain unchanged regardless of the condition.
        * 
        * Example:
        * ```cpp
        * using const_int = add_const_if_t<int, true>;  // const int
        * using non_const_int = add_const_if_t<int, false>; // int
        * ```
        */
        template<typename T, bool Condition>
        using add_const_if_t = typename add_const_if<T, Condition>::type;
        
    } // namespace etools::meta
    
    #endif // ETOOLS_META_TRAITS_HPP_
    