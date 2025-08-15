// SPDX-License-Identifier: MIT
/**
* @file slot.hpp
* 
* @brief Singleton utility for type-safe in-place object construction in static memory.
*
* @ingroup etools_memory etools::memory
*
* This header defines the `slot<T>` class template, a utility designed for embedded or
* resource-constrained environments where dynamic memory allocation is unavailable or undesirable.
* 
* The `slot<T>` provides a static, singleton-based mechanism for constructing, destroying, and 
* accessing a single object of type `T` using placement new within a pre-allocated memory buffer.
* 
* It ensures that only one object of type `T` is alive at any time and uses a lightweight 
* `_constructed` flag to track the object’s lifecycle. This utility avoids heap allocations entirely
* and is suitable for deterministic memory management scenarios (e.g. bare-metal, RTOS).

* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2025-07-29
*
* @copyright
* MIT License
* Copyright (c) 2025 Mark Tikhonov
* See the accompanying LICENSE file for details.
*
* @par Changelog
* - 2025-08-10
*      - Changed type of `_mem` to direct aligned buffer with using std::byte array instaed following 
*        future deprecation of `std::aligned_storage`.
*/

#ifndef ETOOLS_MEMORY_SLOT_HPP_
#define ETOOLS_MEMORY_SLOT_HPP_
#include <type_traits>
#include <cstddef>

namespace etools::memory {
    /**
    * @class slot
    * @brief Provides static memory storage and lifecycle management for a single object of type T.
    *
    * This stateless singleton class provides static memory and placement new semantics for constructing
    * and managing the lifetime of one object of type `T`.
    *
    * @tparam T The type of object to store in this slot.
    *
    * ### Key Characteristics:
    * - Only one instance of `T` can exist in the slot at a time.
    * - Memory is allocated statically and aligned for type `T`.
    * - Lifecycle is manually controlled via `construct()`, `emplace()`, and `destroy()`.
    *
    * ### Usage Example:
    *
    * ```cpp
    * struct Foo { int x; };
    * auto& s = slot<Foo>::instance();
    * s.construct(42);        // Construct Foo in-place with x = 42
    * Foo* ptr = s.get();     // Access constructed object
    * s.destroy();            // Destroy the object
    * ```
    *
    *
    * @warning ```
    * - `construct()` will assert in debug mode if called twice without destruction.
    * - Calling `get()` before construction returns `nullptr`.
    * - This class is NOT thread-safe. It is intended for single-core embedded platforms or
    *   cooperative multitasking environments where explicit synchronization is not needed.
    * ```
    */
    template<typename T>
    class slot {
        public:
        /**
        * @brief Returns the singleton instance of `slot<T>`.
        *
        * @return Reference to the static singleton instance.
        */
        static slot &instance() noexcept;
        
        /**
        * @brief Constructs an object of type `T` in-place with the given arguments.
        *
        * @warning This method will assert in debug builds if the object has already been constructed.
        * Use `emplace()` if you want to overwrite an existing object safely.
        *
        * @tparam Args Argument types for T's constructor.
        * 
        * @param args Arguments forwarded to T's constructor.
        * 
        * @return Pointer to the newly constructed object.
        *
        * @note Placement new is used. Memory is not zero-initialized before construction.
        */
        template<typename... Args>
        inline T* construct(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args&&...>) ;
        
        /**
        * @brief Constructs or replaces the object in-place with the given arguments.
        *
        * If an object is already constructed, it will be destroyed before constructing a new one.
        *
        * @tparam Args Argument types for T's constructor.
        * @param args Arguments forwarded to T's constructor.
        * @return Pointer to the newly constructed object.
        */
        template<typename... Args>
        inline T* emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args&&...>);
        
        /**
        * @brief Destroys the currently constructed object, if any.
        *
        * This method calls the destructor of T and resets the constructed flag.
        * 
        * @note If the object is not constructed, this method does nothing.
        */
        inline void destroy() noexcept(std::is_nothrow_destructible_v<T>);
        
        /**
        * @brief Returns a pointer to the object if constructed.
        *
        * @return Pointer to T if constructed, otherwise `nullptr`.
        */
        inline T* get() noexcept;
        
        /**
        * @brief Returns a const pointer to the object if constructed.
        *
        * @return Pointer to const T if constructed, otherwise `nullptr`.
        */
        inline const T* get() const noexcept;
        
        /// @brief Deleted copy constructor.
        slot(const slot&) = delete; 
        
        /// @brief Deleted copy assignment operator.
        slot& operator=(const slot&) = delete; 
        
        /// @brief Deleted move constructor.
        slot(slot&&) = delete; 
        
        /// @brief Deleted move assignment operator.
        slot& operator=(slot&&) = delete;
        
        private:
        /**
        * @brief Private default constructor to enforce singleton usage.
        */
        slot() = default;
        
        /**
        * @brief Statically allocated aligned buffer to hold the object of type `T`.
        *
        * This memory is never deallocated and lives for the duration of the program.
        * Placement new is used to construct the object into this buffer.
        */
        alignas(T) static inline std::byte _mem[sizeof(T)];
        
        /**
        * @brief Internal flag indicating whether the object is currently constructed.
        *
        * This allows the slot to track the validity of `get()` and prevent undefined behavior.
        *
        * @note This is NOT atomic. Not suitable for concurrent access.
        */
        bool _constructed = false;
    };
} // namespace etools::memory

#include "slot.tpp"
#endif // ETOOLS_TOOLS_SLOT_HPP_