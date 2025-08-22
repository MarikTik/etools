// SPDX-License-Identifier: MIT
/**
* @file llut.hpp
*
* @brief Light look-up table (LLUT): compile-time direct-index table mapping a fixed
*        set of unsigned keys to dense indices, with O(1) queries and a sentinel.
*
* @ingroup etools_hashing etools::hashing
*
* @details
* This header provides a tiny, header-only facility for translating a small, fixed set
* of integral/enum-like keys into dense indices `[0..N-1]` without hashing or branching.
*
* #### Components
* - `etools::hashing::details::llut_impl<Key, Keys...>` — the immutable, constexpr-constructible
*   implementation type. It owns a direct-indexed `std::array` of length `max(Keys) + 1`.
*   Present keys map to their dense indices (defined by pack order), and holes store a
*   sentinel equal to `N = sizeof...(Keys)`.
* - `etools::hashing::llut<Key>` — a thin facade that returns a reference to a canonical
*   `inline constexpr` singleton `llut_impl<Key, Keys...>` via `generate<Keys...>()`.
*
* #### Design notes
* - All structure/lookup properties are exposed as `static constexpr` members:
*   - `keys()`         -> number of keys (also the sentinel value).
*   - `size()`         -> table length (aka `max(Keys) + 1`).
*   - `not_found()`    -> sentinel equal to `keys()`.
* - The singleton is defined as a **namespace-scope `inline constexpr` variable template**,
*   so it has static storage duration and is usable in constant evaluation. Calls to
*   `llut<Key>::generate<Keys...>()` return a `constexpr const&` to this object.
*
* Example:
* ```cpp
* #include <cstdint>
* #include "etools/hashing/llut.hpp"
*
* using llut = etools::hashing::llut<std::uint8_t>;
*
* // Obtain the canonical table for the key-set {2,5,7}.
* constexpr const auto& table = llut::generate<2,5,7>();
*
* static_assert(table::keys() == 3);                 // 3 keys -> indices [0..2], sentinel == 3
* static_assert(table::size()  == 8);                // max key = 7 => table length = 8
* static_assert(table(2) == 0 && table(5) == 1 && table(7) == 2);
* static_assert(table(0) == table.not_found());         // absent key -> sentinel
* static_assert(table(9) == table.not_found());         // out-of-range -> sentinel
* ```
*
* @note Name origin: **llut** = *light look-up table*. “Simple look-up table” was... vetoed :)
*
* dependencies
* This file expects:
* - `meta::tpack_max<Key, Keys...>()` returning the maximum of `Keys...` at compile time.
* - `meta::smallest_uint_t<N>` — smallest unsigned type that can represent `[0..N]`.
* - Optionally, `meta::all_distinct(std::array<Key, N>)` for pairwise distinctness checking.
*
* Define `ETOOLS_SKIP_CONSTEXPR_DISTINCT_CHECK` to skip the distinctness assertion.
*/
#ifndef ETOOLS_HASHING_LLUT_HPP_
#define ETOOLS_HASHING_LLUT_HPP_

#include <array>
#include <cstddef>
#include <initializer_list>
#include <type_traits>

#include "../meta/utility.hpp" // meta::all_distinct(...)
#include "../meta/traits.hpp"  // smallest_uint_t<...>, meta::tpack_max<...>

namespace etools::hashing {
    // facade (forward)
    template<typename KeyType>
    class llut;

    /**
    * @namespace etools::hashing::details
    * @brief Internal implementation details for LLUT; not part of the public API surface.
    */
    namespace details{
       

        /**
        * @brief Forward declaration of implementation.
        */
        template <typename Key, Key... Keys>
        class llut_impl;
            
        /**
        * @brief Forward declaration of factory.
        */
        template <typename Key, Key... Keys>
        constexpr llut_impl<Key, Keys...> make_llut_impl() noexcept;

        /**
        * @class llut_impl
        *
        * @tparam KeyType Unsigned integral key type (e.g., `uint8_t`, `uint16_t`, `uint32_t`).
        * @tparam Keys Parameter pack of **distinct** key values. Pack order defines indices.
        *
        * @brief Immutable, constexpr-constructible direct table mapping `Keys...` to indices.
        *
        * The table length is `size() == max(Keys) + 1`. Each present key maps to its index
        * in `[0..keys()-1]`, where `keys() == sizeof...(Keys)`. Holes are filled with the sentinel
        * `not_found()` == `keys()`.
        *
        * Invariants:
        * 
        * - `KeyType` is an unsigned integral type (`std::is_unsigned_v<Key>`).
        * 
        * - `keys() > 0`.
        * 
        * - Keys are pairwise distinct (unless the check is explicitly disabled).
        *
        * @note Lookup: `O(1)`.
        * 
        * @warning When `max(Keys...)` is significantly larger then `sizeof...(Keys)` it 
        * is heavily advised to prefer `fks` generator. 
        */
        template<typename KeyType, KeyType... Keys>
        class llut_impl{

            /**
            * @typedef index_t
            *  
            * @brief Index storage type (smallest unsigned that can represent `[0..keys()]`).
            */
            using index_t = meta::smallest_uint_t<sizeof...(Keys)>;
        public:
            
            /**
            * @brief Number of keys in the set (also the sentinel value).
            *
            * @return Number of keys `N = sizeof...(Keys)`.
            */
            [[nodiscard]] static constexpr std::size_t keys() noexcept;
            
            /**
            * @brief Sentinel value returned for “not found”.
            *
            * @return `keys()`.
            */
            [[nodiscard]] static constexpr std::size_t not_found() noexcept;

            /**
            * @brief Size of the direct-indexed table.
            *
            * The table must cover every present key indexable by value, hence it spans
            * `[0..max(Keys)]`, so the size is `max(Keys) + 1`.
            *
            * @return Table size as `std::size_t`.
            */
            [[nodiscard]] static constexpr std::size_t size() noexcept;

            /**
            * @brief Constant-time query: return index for `key`, or `not_found()` if absent/out of range.
            *
            * @param[in] key The key to look up.
            * @return Undex in `[0..keys()-1]` or `not_found()` if the key is not present or out of range.
            */
            [[nodiscard]] constexpr std::size_t operator()(KeyType key) const noexcept;

            /** @brief Deleted copy constructor. */
            llut_impl(const llut_impl&) = delete;
            /** @brief Deleted copy assignment.  */
            llut_impl& operator=(const llut_impl&) = delete;
            /** @brief Deleted move constructor. */
            llut_impl(llut_impl&&) = delete;
            /** @brief Deleted move assignment.  */
            llut_impl& operator=(llut_impl&&) = delete;

        private:
            
            /**
            * @brief Private default constructor; builds the table at compile time.
            */
            constexpr llut_impl() noexcept;
            
            /**
            * @brief Construct the backing array in pure `constexpr` fashion.
            *
            * - Initializes all slots to `not_found()`.
            * 
            * - Assigns indices in the order of `Keys...` using LTR guaranteed evaluation.
            *
            * @return Fully populated `std::array<index_t, size()>`.
            */
            static constexpr std::array<index_t, size()> make_table() noexcept;

            /**
            * @brief Friend factory allowed to invoke the private constructor.
            */
            template <typename K, K... Ks>
            friend constexpr llut_impl<K, Ks...> make_llut_impl() noexcept;

            /**
            * @brief Backing direct-index table (`[0..size()-1]`), filled with dense indices or sentinel.
            */
            std::array<index_t, size()> _table;

            static_assert(std::is_unsigned_v<KeyType>, "KeyType must be unsigned integral type");
            static_assert(keys() > 0, "Number of keys must exceed 0");
        };

        /**
        * @brief Factory that constructs `llut_impl` by value in a `constexpr` way.
        *
        * @return A value-initialized `llut_impl<Key, Keys...>`.
        */
        template <typename Key, Key... Keys>
        [[nodiscard]] constexpr llut_impl<Key, Keys...> make_llut_impl() noexcept;
        
        /**
        * @brief Canonical singleton: one `inline constexpr` instance per `<Key, Keys...>` in the program.
        *
        * This variable template has static storage duration and is ODR-merged across TUs.
        * It is usable in constant evaluation. The facade `llut<Key>::generate<Keys...>()`
        * returns a `constexpr const&` to this object.
        */
        template <typename Key, Key... Keys>
        inline constexpr auto llut_impl_singleton = make_llut_impl<Key, Keys...>();
        
    } // namespace details

    /**
    * @class llut
    *
    * @brief Thin facade that returns the canonical `llut_impl<KeyType, Keys...>` instance by const reference
    * via its `generate` method.
    * 
    * @tparam KeyType Unsigned integral key type..
    */
    template<typename KeyType>
    struct llut{
        /**
        * @brief Obtain the canonical table for a fixed key set.
        *
        * @tparam Keys The fixed key set (distinct values). Pack order defines dense indices.
        * @return `constexpr const details::llut_impl<KeyType, Keys...>&` reference to the singleton.
        */
        template<KeyType... Keys>
        [[nodiscard]] static constexpr const details::llut_impl<KeyType, Keys...>& instance() noexcept;

        /**
        * @brief Deleted default constructor — this facade is not meant to be instantiated.
        */
        llut() = delete;

        /**
        * @brief Deleted copy constructor.
        */
        llut(const llut&) = delete;

        /**
        * @brief Deleted copy assignment.
        */
        llut& operator=(const llut&) = delete;
        
        /**
        * @brief Deleted move constructor.
        */
        llut(llut&&) = delete;

        /**
        * @brief Deleted move assignment.
        */
        llut& operator=(llut&&) = delete;
    };

} // namespace etools::hashing

#include "llut.tpp"
#endif // ETOOLS_HASHING_LLUT_HPP_
