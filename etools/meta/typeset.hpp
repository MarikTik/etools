/**
* @file typeset.hpp
*
* @brief Provides a utility class for managing flags associated with types.
*
* @ingroup etools_meta etools::meta
*
* This file defines the `etools::meta::typeset` class, which allows associating
* boolean flags with a set of types. It enables O(1) runtime access
* to the corresponding flag.
*
* @note The class uses std::bitset to store the flags, so you can consider it lightweight.
* @warning The class may not perform well for large sets of types due to 
* internal usage of recursive template instantiation. Refactor with std::index_sequence
* if there is a need to decrease compilation times.
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
#ifndef ETOOLS_META_TYPESET_HPP_
#define ETOOLS_META_TYPESET_HPP_
#include <bitset>
#include <type_traits>
#include <utility>  // std::integral_constant
#include "traits.hpp" // is_unique trait

namespace etools::meta {
    
    /**
    * @class typeset
    * @brief Manages boolean flags associated with a set of types.
    *
    * This class provides a mechanism to efficiently associate boolean flags with a
    * predefined set of types. It uses template metaprogramming to map each type to
    * a unique bit within an internal `std::bitset`. This allows for fast, O(1) runtime
    * access to the flag associated with any of the handled types.
    *
    * @tparam Types... The set of types to associate with flags. All types in this
    * parameter pack must be distinct.
    */
    template <typename... Types>
    class typeset {
    static_assert(is_distinct_v<Types...>, "All types in the parameter pack must be distinct.");
    public:    
        /**
        * @brief Checks if the flag associated with a given type is set.
        *
        * This method determines if the flag associated with the specified type `T`
        * is currently set. It uses compile-time metaprogramming to efficiently
        * retrieve the index of the type within the handled set and then performs
        * a constant-time lookup in the internal `std::bitset`.
        *
        * @tparam T The type to check the flag for.
        * @return `true` if the flag for type `T` is set, `false` otherwise.
        */
        template <typename T>
        constexpr bool test() const;
     
        /**
        * @brief Sets the flag associated with a given type.
        *
        * This method sets the flag associated with the specified type `T`. It uses
        * compile-time metaprogramming to efficiently retrieve the index of the type
        * within the handled set and then sets the corresponding bit in the internal
        * `std::bitset`.
        *
        * @tparam T The type to set the flag for.
        */
        template <typename T>
        constexpr void set();
     
        /**
        * @brief Resets (clears) the flag associated with a given type.
        *
        * This method resets (clears) the flag associated with the specified type `T`.
        * It uses compile-time metaprogramming to efficiently retrieve the index of
        * the type within the handled set and then resets the corresponding bit in
        * the internal `std::bitset`.
        *
        * @tparam T The type to reset the flag for.
        */
        template <typename T>
        constexpr void reset();
     
    private:
        std::bitset<sizeof...(Types)> _bits; ///< Bitset storing the flags.
     
        /**
        * @struct type_to_index
        * @brief Helper struct for mapping types to bit indices.
        *
        * This struct uses template metaprogramming to recursively determine the
        * index of a given type `T` within the `Types...` parameter pack. The
        * index is calculated at compile time.
        *
        * @tparam T The type to find the index of.
        * @tparam Ts... The remaining types in the parameter pack.
        */
        template <typename T, typename... Ts>
        struct type_to_index;
     
        /**
        * @brief Partial specialization of `type_to_index` for the general case.
        *
        * This partial specialization handles the recursive step in determining the
        * index of a type `T`. It checks if `T` is the same as the first type `U`.
        * If they are the same, the index is 0. Otherwise, it's 1 plus the index
        * of `T` in the remaining types `Us...`.
        *
        * @tparam T The type to find the index of.
        * @tparam U The first type in the `Ts...` parameter pack.
        * @tparam Us... The remaining types in the `Ts...` parameter pack.
        */
        template <typename T, typename U, typename... Us>
        struct type_to_index<T, U, Us...> : std::integral_constant<std::size_t,
            std::is_same_v<T, U> ? 0 : 1 + type_to_index<T, Us...>::value> {};
     
        /**
        * @brief Partial specialization of `type_to_index` for the base case.
        *
        * This partial specialization handles the base case of the recursion, when
        * there is only one type `T` left. In this case, the index of `T` is 0.
        *
        * @tparam T The type to find the index of.
        */
        template <typename T>
        struct type_to_index<T> : std::integral_constant<std::size_t, 0> {};
    };
} // namespace etools::meta

#include "typeset.tpp"
#endif // ETOOLS_META_TYPESET_HPP_