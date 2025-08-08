// SPDX-License-Identifier: MIT
/**
* @file registry.hpp
*
* @ingroup etask_facilities etask::facilities
*
* @brief Compile‑time configured, allocation‑free factory/registry for polymorphic objects.
*
* The `etools::facilities::registry` maps **integral keys** to **statically stored** (one-per-type)
* object instances of derived classes. Objects are **lazily constructed** on first use and
* **optionally destroyed** explicitly (or during registry teardown).
*
* Behind the scenes, storage and lifetime management are delegated to
* `etools::memory::slot<T>`, so there are **no runtime heap allocations** performed by the registry
* itself—construction places the object into a static `slot<T>`.
*
* ### How it works
* - A user‑provided `Extractor<T>` metafunction exposes a unique integral key via `Extractor<T>::value`.
* - A typelist enumerates all derived types that can live in the registry.
* - The registry builds:
*   - a **routing table** of function pointers (get/construct/destroy) per derived type, and
*   - a **sorted index table** mapping keys → routing indices for `O(log N)` lookups.
* - `get(key)` and `find(key)` only expose objects that have actually been constructed.
* - `construct(key, args...)` constructs (or returns) the singleton instance for that derived type.
*
* ### Constraints
* - Each `Derived` must be `nothrow` constructible from `ConstructorArgs...` and `nothrow` destructible
*   (enforced via `static_assert`).
* - All keys must be unique across the typelist.
* - Not thread-safe, internally uses `etools::memory::slot<>` class which grants unsyncronized access to memory.
*
* ### Complexity
* - Key lookup: `O(log N)` over the index table.
* - Actual dispatch (get/construct/destroy): `O(1)` via precomputed routing.
* - Iteration: visits constructed entries only; unconstructed slots are skipped.
*
* ### Example
* @code{.cpp}
* struct Base {
*   virtual ~Base() = default;
*   virtual int run() = 0;
* };
*
* struct Foo : Base { static constexpr int id = 1; int run() override { return 42; } };
* struct Bar : Base { static constexpr int id = 2; int run() override { return 7;  } };
*
* template <typename T> struct Extract { static constexpr int value = T::id; };
*
* using Types = etools::meta::typelist<Foo, Bar>;
* using CtorArgs = etools::meta::typelist<>;
*
* using Reg = etools::facilities::registry<Base, Extract, Types, CtorArgs>;
*
* // Construct and use objects
* auto& reg = Reg::instance();
* Base* foo = reg.construct(Foo::id);  // lazily constructs Foo in etools::memory::slot<Foo>
* Base* bar = reg.get(Bar::id);        // nullptr (not constructed yet)
*
* // Safe lookup
* if (auto it = reg.find(Foo::id); it != reg.end()) {
*   int v = (*it)->run();              // 42
* }
*
* // Iterate over constructed objects (skips unconstructed slots)
* for (Base* ptr : reg) {
*   ptr->run();
* }
*
* // Optional explicit destruction
* reg.destroy(Foo::id);
* @endcode
*
* ### Use cases
* - Command registries, codecs, or backends keyed by enum/integer identifiers.
* - Plugin‑like selection without dynamic discovery nor heap allocations.
* - Embedded / low‑latency systems where **allocation‑free** and **deterministic** behavior matter.
*
* @note All storage is provided by `etools::memory::slot<T>`, enabling construction without
*       dynamic memory allocation.
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2025-07-29
*
* @copyright
* MIT License
* Copyright (c) 2025 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_FACILITIES_REGISTRY_HPP_
#define ETOOLS_FACILITIES_REGISTRY_HPP_
#include "../meta/typelist.hpp"
#include "../meta/traits.hpp"
#include <array>
#include <limits>
#include <algorithm>

namespace etools::facilities {

    /**
    * @struct registry 
    * 
    * @brief A factory-style registry that maps keys to statically stored, lazily constructed objects.
    *
    * Each derived type is associated with a unique key via the Extractor metafunction.
    * Objects are constructed on demand and stored statically via `slot<T>`, enabling polymorphic access.
    *
    * @tparam Base The polymorphic base class of all derived types.
    * @tparam Extractor A metafunction returning a unique integral key for each derived type.
    * @tparam DerivedTypesList A typelist of all derived classes inheriting from Base.
    * @tparam ConstructorArgsList A typelist of constructor argument types required for all DerivedTypes.
    */
    template<
        typename Base,
        template<typename> typename Extractor,
        typename DerivedTypesList,
        typename ConstructorArgsList
    >
    class registry;

    /**
    * @brief Specialization of registry for unpacked DerivedTypes and ConstructorArgs.
    */
    template<
        typename Base,
        template<typename> typename Extractor,
        typename... DerivedTypes,
        typename... ConstructorArgs
    >
    class registry<
        Base,
        Extractor,
        meta::typelist<DerivedTypes...>,
        meta::typelist<ConstructorArgs...>
    > 
    {

        struct route; // Forward declaration for route struct
        struct mapping; // Forward declaration for mapping struct
        template<typename B> struct registry_iterator; // Forward declaration for iterator template
        
        /**
        * @brief The number of derived types in this registry.
        * @note This is a compile-time constant deduced from the number of types in DerivedTypes.
        */
        static constexpr std::size_t capacity = sizeof...(DerivedTypes);

        /**
        * @typedef sample_t 
        * 
        * @brief The first derived type used to deduce key key type.
        */
        using sample_t = typename meta::nth<0, DerivedTypes...>::type;

        /**
        * @typedef key_t
        * 
        * @brief The type of the unique key for each derived type, deduced via Extractor metafunction.
        */
        using key_t = std::remove_cv_t<decltype(Extractor<sample_t>::value)>;

        /**
        * @typedef routing_table_t
        * 
        * @brief A type alias for the routing table, which is either a std::array or const std::array
        * depending on the number of derived types.
        * 
        * @note The intent of this conidtional type is to optimize memory usage for small numbers of derived types.
        * For most systems const arrays would be placed in RAM unless they are relatively large.
        * Larger numbers of derived types (> 256) will use const std::array with the intent to place it in ROM, 
        * saving RAM space with little to no performance impact.
        */
        using routing_table_t = meta::add_const_if_t<
            std::array<route, capacity>, 
            (capacity > std::numeric_limits<std::uint8_t>::max())
        >;

        /**
        * @typedef index_t
        * 
        * @brief The smallest unsigned integer type that can hold the index of a route in the registry.
        * This is used to redirect keys to routes by index.
        */
        using index_t = meta::smallest_uint_t<capacity>;

        /**
        * @typedef index_table_t
        * @brief A static array mapping keys to their corresponding indices in the routing table.
        * This allows for fast lookups of routes by key.
        * @note The array is sized to the capacity of the registry, ensuring it can hold all keys.
        */
        using index_table_t = std::array<mapping, capacity>;

        static_assert((std::is_nothrow_constructible_v<DerivedTypes, ConstructorArgs...> && ...), "All derived types must be nothrow constructible");
        static_assert((std::is_nothrow_destructible_v<DerivedTypes> && ...), "All derived types must be nothrow destructible");
    public: 
        /**
        * @typedef iterator
        * 
        * @brief Type alias for iterators over the registry.
        * 
        * his alias allows users to iterate over the registry's constructed objects
        * and modify them. It provides a read-write view.
        */
        using iterator = registry_iterator<Base*>;

        /**
        * @typedef const_iterator
        * 
        * @brief Type alias for constant iterators over the registry.
        * 
        * This alias allows users to iterate over the registry without modifying the objects,
        * providing a read-only view of the constructed objects.
        */
        using const_iterator = registry_iterator<const Base*>;

        /**
        * @brief Retrieves the singleton instance of the registry.
        * 
        * This method ensures that only one instance of the registry exists throughout the application,
        * following the singleton design pattern.
        * 
        * @return A reference to the singleton instance of the registry. 
        */
        static registry &instance() noexcept;

        /**
        * @brief Retrieves a pointer to the object associated with the given key, if constructed.
        * 
        * @param key The key associated with a derived type.
        * 
        * @return `Base` pointer if the object is constructed, or nullptr.
        */
        Base* get(key_t key) noexcept;

        /**
        * @brief Constructs the object associated with the given key.
        *
        * If already constructed, returns the existing instance.
        * 
        * @param key The key identifying a derived type.
        * @param args Arguments forwarded to the constructor.
        * @return Base pointer to the constructed object, or nullptr if the key is unknown.
        */
        Base* construct(key_t key, ConstructorArgs&&... args) noexcept;

        /**
        * @brief Destroys the object associated with the given key, if constructed.
        * 
        * @param key The key identifying the object to destroy.
        */
        void destroy(key_t key) noexcept;

        /**
        * @brief Destructor; destroys all constructed objects in reverse registry order.
        */
        ~registry() noexcept;

        /**
        * @brief Returns a mutable iterator to the first constructed object.
        *
        * @return An iterator pointing to the first constructed object.
        */
        inline iterator begin() noexcept;

        /**
        * @brief Returns a mutable iterator to the element following the last constructed object.
        *
        * @return An iterator pointing to the end of the registry.
        */
        inline iterator end() noexcept;

        /**
        * @brief Returns a constant iterator to the first constructed object.
        *
        * @return A const_iterator pointing to the first constructed object.
        */
        inline const_iterator begin() const noexcept;

        /**
        * @brief Returns a constant iterator to the element following the last constructed object.
        *
        * @return A const_iterator pointing to the end of the registry.
        */
        inline const_iterator end() const noexcept;

        /**
        * @brief Returns a constant iterator to the first constructed object.
        *
        * @return A const_iterator pointing to the first constructed object.
        */
        inline const_iterator cbegin() const noexcept;

        /**
        * @brief Returns a constant iterator to the element following the last constructed object.
        *
        * @return A const_iterator pointing to the end of the registry.
        */
        inline const_iterator cend() const noexcept;

        /**
        * @brief Searches for a constructed object with the given key.
        *
        * This is the constant version, which returns a read-only iterator.
        *
        * @param key The unique key associated with the derived type.
        * @return A const_iterator to the found object, or `cend()` if not found.
        */
        inline const_iterator find(key_t key) const noexcept;

        /**
        * @brief Searches for a constructed object with the given key.
        *
        * This is the mutable version, which returns an iterator for read/write access.
        *
        * @param key The unique key associated with the derived type.
        * @return An iterator to the found object, or `end()` if not found.
        */
        inline iterator find(key_t key) noexcept;

        /// Delete copy constructor to prevent singleton misuse.
        registry(const registry&) = delete;

        /// Delete copy assignment operator to prevent singleton misuse.
        registry& operator=(const registry&) = delete;

        /// Delete move constructor to prevent singleton misuse.
        registry(registry&&) = delete;

        /// Delete move assignment operator to prevent singleton misuse.
        registry& operator=(registry&&) = delete;
    private:

        /**
        * @struct route
        * 
        * @brief A structure defining the routing table entry to control derived type access.
        * 
        * Contains function pointers for getting, constructing, and destoying the object.
        */
        struct route{
            Base* (*getter)() noexcept;  ///< Function returning a pointer to the instance.
            Base* (*constructor)(ConstructorArgs&&...) noexcept;  ///< Constructor function.
            void (*destructor)() noexcept;  ///< Destructor function.
        };

        /** 
        * @struct mapping
        * 
        * @brief A structure mapping a key to its index in the routing table.
        * 
        * This is used to quickly find the route for a given key
        * 
        * @note metaly the mappings are sorted by keys, so key search is O(log(n)) which is decent
        * for use cases of small to medium-sized registries, which is exactly what is intended, supposing
        * there will be no more than 2^16 entries in the `_routing_table` there is no memory impact in storing two 
        * table arrays (`_routing_table`, `_index_table`) vs one single table with the key being part of `route`.
        */
        struct mapping {
            key_t key;
            index_t index;
        };

        /**
        * @brief Constructs the registry and sorts meta mapping table by key.
        */
        registry() noexcept;

        /**
        * @brief Constructs a routing table with function pointers for each derived type.
        * 
        * This function initializes the routing table with getter, constructor, and destructor pointers
        * for each derived type.
        * 
        * @return A routing table with function pointers for each derived type.
        */
        constexpr routing_table_t make_routing_table() const;

        /**
        * @brief Constructs an index table mapping keys to their indices in the routing table.
        * 
        * This function creates an array that maps each key to its corresponding index in the routing table.
        * 
        * @return An index table mapping keys to their indices in the routing table.
        */
        constexpr index_table_t make_index_table() const;

        /**
        * 
        * @brief Helper function to create the index table using a compile-time index sequence.
        * This function generates the index table by iterating over the indices of the derived types.
        * 
        * @tparam Is The indices of the derived types.
        * @return An index table mapping keys to their indices in the routing table.
        */
        template <std::size_t... Is>
        constexpr index_table_t make_index_table_impl(std::index_sequence<Is...>) const;

        /**
        * @brief Iterator class for iterating over the registry.
        * 
        * This iterator allows for iterating over the constructed objects in the registry.
        * It skips over unconstructed slots, providing a clean interface for users.
        * 
        * @tparam PtrType The type of pointer to return from the iterator.
        */
        template <typename PtrType>
        class registry_iterator {
            using route_iter_t = typename routing_table_t::const_iterator; // route is const in both cases
            route_iter_t _it, _end;
            friend class registry<Base, Extractor, meta::typelist<DerivedTypes...>, meta::typelist<ConstructorArgs...>>;

        public:
            using value_type = PtrType;
            using reference = PtrType;
            using pointer = PtrType;
            using iterator_category = std::input_iterator_tag;
            using difference_type = std::ptrdiff_t;

            /**
            * @brief Construct an iterator over the registry routing table.
            *
            * Initializes the iterator to the first constructed slot (skipping nulls).
            *
            * @param it   Begin iterator into the routing table.
            * @param end  End iterator into the routing table.
            */
            inline registry_iterator(route_iter_t it, route_iter_t end);
            
            /**
            * @brief Dereference to the pointer-like value of the current entry.
            * @return Pointer-like value (cast to @c PtrType).
            *
            * @pre The iterator is not at end and the current entry is constructed.
            */
            inline reference operator*() const;

            /**
            * @brief Member access to the pointer-like value of the current entry.
            * @return Pointer-like value (cast to @c PtrType).
            *
            * @pre The iterator is not at end and the current entry is constructed.
            */
            inline pointer operator->() const;

            /**
            * @brief Pre-increment to the next constructed entry.
            * @return Reference to @c *this after increment.
            */
            inline registry_iterator& operator++();

            /**
            * @brief Post-increment to the next constructed entry.
            * @return Copy of the iterator prior to increment.
            */
            inline registry_iterator operator++(int);

            /**
            * @brief Equality comparison for registry iterators.
            * @return @c true if both iterators point to the same routing-table position.
            */
            friend bool operator==(const registry_iterator& a, const registry_iterator& b){
                return a._it == b._it; // writing this as a separate definition is Sysyphus labor.
            }

            /**
            * @brief Inequality comparison for registry iterators.
            * @return @c true if the iterators differ.
            */
            friend bool operator!=(const registry_iterator& a, const registry_iterator& b) {
                return !(a == b);
            }
            
        private:
            /**
            * @brief Advance `_it` until it points to a constructed entry or reaches `_end`.
            *
            * Skips over entries whose `getter()` returns `nullptr`.
            */
            void advance_if_null();

            /**
            * @brief Internal helper to get the current pointer-like value.
            * @return Pointer-like value (cast to @c PtrType).
            *
            * @note Used by the iterator’s implementation; not part of the public API.
            */
            reference getter() const;

            /**
            * @brief Internal helper to call the underlying entry destructor.
            *
            * @note Used by the iterator’s implementation; not part of the public API.
            */
            void destructor() const;
        };

        /**
        * @brief The routing table containing function pointers for each derived type.
        * 
        * This table is used to route calls to the appropriate getter, constructor, and destructor
        * for each derived type based on its key.
        */
        routing_table_t _routing_table;

        /**
        * @brief The index table mapping keys to their corresponding indices in the routing table.
        * 
        * This table allows for fast lookups of routes by key, enabling efficient access to the
        * getter, constructor, and destructor functions for each derived type.
        */
        index_table_t _index_table;

    };

   

}

#include "registry.tpp"
#endif //ETOOLS_FACILITIES_REGISTRY_HPP_