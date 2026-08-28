// SPDX-License-Identifier: MIT
/**
* @file static_vector.hpp
*
* @brief Fixed-capacity contiguous container with inline storage and stable element addresses.
*
* @ingroup etools_memory etools::memory
*
* This header defines `static_vector<T, Capacity>`, a sequence container that holds up to
* `Capacity` objects of type `T` in storage embedded **inside the container object itself**.
* There is no allocator, no heap, and - the property the rest of this file is really about -
* **no reallocation, ever**.
*
* ### Why this exists alongside `std::vector`
*
* `std::vector` with a `reserve()` is *almost* this, and on a hosted system it is the better
* choice. Two things it cannot offer:
*
* - **No allocation at all.** `reserve()` still calls the allocator once. On a target with no
*   heap, or where a single `malloc` in a control loop is unacceptable, once is too many.
* - **Address stability as a type-level guarantee.** A `vector` that outgrows its reserve
*   reallocates and invalidates every pointer, reference, and iterator into it. `reserve()`
*   is a *hint* the compiler cannot check and a later `push_back` can silently exceed. Here
*   the bound is a template parameter: exceeding it is not undefined, it is a `nullptr` from
*   @ref try_emplace_back. Code that holds a `T&` across a mutation is correct by construction
*   rather than by discipline.
*
* That second point is the reason to reach for this type even where a heap is available. A
* container whose elements never move lets callers hold references across insertions - which
* is exactly what a scheduler or task manager iterating its own records while they mutate
* needs, and exactly what `std::vector` cannot promise.
*
* ### Relationship to `slot`
*
* @ref slot "slot<T>" describes itself as a blueprint for a future pool cell, and notes that
* its per-object `_constructed` flag is the part a real pool would drop, because a pool tracks
* occupancy externally. `static_vector` is that pool for the contiguous-prefix case: occupancy
* is the single `_size` counter, so elements are built directly in one aligned byte array with
* no per-element discriminant. A `static_vector<T, N>` is therefore strictly smaller than an
* array of `N` slots, and its elements are contiguous - which `slot<T>[N]` is not, since each
* slot carries its own flag and padding.
*
* ### Usage Example
*
* ```cpp
* etools::memory::static_vector<record, 8> live;
*
* if (record* r = live.try_emplace_back(uid, channel)) {
*     // r stays valid for as long as this element lives, across any
*     // number of further insertions - no reallocation can occur.
* }
* else {
*     // Capacity reached. A rejection, not a reallocation.
* }
*
* for (std::size_t i = live.size(); i-- > 0; )
*     if (live[i].done)
*         live.swap_erase(i);   // O(1); order is not preserved
* ```
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2026-08-27
*
* @copyright
* MIT License
* Copyright (c) 2026 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_MEMORY_STATIC_VECTOR_HPP_
#define ETOOLS_MEMORY_STATIC_VECTOR_HPP_
#include "../meta/traits.hpp"   // etools::meta::always_false_v
#include <cassert>
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

static_assert(__cpp_lib_launder >= 201606L,
    "etools::memory::static_vector requires <new>'s std::launder (C++17, "
    "__cpp_lib_launder >= 201606). A no-op shim would silently miscompile "
    "under the optimizer, so we refuse to build instead.");

namespace etools::memory {

    /**
    * @class static_vector
    * @brief Contiguous container of at most `Capacity` objects, stored inline, never relocated.
    *
    * @tparam T        The element type. Must be nothrow-destructible.
    * @tparam Capacity The maximum number of elements. Must be greater than zero.
    *
    * ### Key Characteristics
    *
    * - Storage is a single `alignas(T)` byte array inside the container; no heap, no allocator.
    * - Elements are contiguous, so `T*` is a valid random-access iterator and the whole
    *   `<algorithm>` surface applies.
    * - **Element addresses are stable.** A reference, pointer, or iterator to an element stays
    *   valid until that element is erased or the container is destroyed. Insertion never
    *   invalidates anything; see @ref swap_erase for the one operation that does.
    * - Copyable: never. Movable: never. The container is pinned, matching the address-stability
    *   guarantee - a move would relocate every element and defeat the point of the type.
    * - Not thread-safe.
    *
    * @invariant `size() <= Capacity`.
    * @invariant The first `size()` slots of the storage hold live `T` objects; the rest are raw.
    *
    * @note `T` must be nothrow-destructible; this is enforced by `static_assert`. Destruction
    *       runs from @ref clear, @ref pop_back, @ref swap_erase, and the destructor, none of
    *       which can meaningfully recover from a throwing `~T()`.
    */
    template<typename T, std::size_t Capacity>
    class static_vector {
        static_assert(Capacity > 0,
            "etools::memory::static_vector<T, Capacity> requires Capacity > 0. "
            "A container that can never hold an element is certainly a mistake.");

        static_assert(std::is_nothrow_destructible_v<T>,
            "etools::memory::static_vector<T, Capacity> requires T to be nothrow-destructible; "
            "a throwing destructor could leave the container in a half-erased state.");

    public:
        /// @brief The contained value type.
        using value_type = T;
        /// @brief Unsigned type used for sizes and indices.
        using size_type = std::size_t;
        /// @brief Mutable iterator - a raw pointer, since storage is contiguous.
        using iterator = T*;
        /// @brief Immutable iterator - a raw pointer, since storage is contiguous.
        using const_iterator = const T*;
        /// @brief Mutable element reference.
        using reference = T&;
        /// @brief Immutable element reference.
        using const_reference = const T&;

        /**
        * @brief Constructs an empty container.
        *
        * No element is constructed; the storage is left raw.
        *
        * @post `size() == 0`.
        */
        static_vector() noexcept = default;

        /**
        * @brief Destroys every contained element, in reverse order of construction.
        *
        * @post All elements have been destroyed exactly once.
        */
        ~static_vector() noexcept;

        /// @brief Deleted copy constructor - the container is pinned; its elements never relocate.
        static_vector(const static_vector&) = delete;
        /// @brief Deleted copy assignment - see the deleted copy constructor.
        static_vector& operator=(const static_vector&) = delete;
        /// @brief Deleted move constructor - moving would relocate elements, defeating address stability.
        static_vector(static_vector&&) = delete;
        /// @brief Deleted move assignment - see the deleted move constructor.
        static_vector& operator=(static_vector&&) = delete;

        /**
        * @brief Constructs an element at the end, if there is room.
        *
        * The reporting form of "append": a full container is an expected outcome that the
        * caller is made to handle, not an error and not undefined behaviour. This is the
        * counterpart to `std::vector::emplace_back`, which would grow instead - growth being
        * precisely what this container exists to prevent.
        *
        * @tparam Args Argument types for `T`'s constructor.
        * @param args Arguments forwarded to `T`'s constructor.
        *
        * @return Pointer to the newly constructed element, or `nullptr` if the container is
        *         already at capacity. The returned pointer remains valid until that element
        *         is erased.
        *
        * @post On success, `size()` has increased by one. On failure, the container is
        *       entirely unchanged.
        *
        * @note If `T`'s constructor throws, `size()` is not incremented and the container is
        *       left exactly as it was.
        * @note The function is `noexcept` iff `T`'s selected constructor is `noexcept`.
        */
        template<typename... Args>
        [[nodiscard]] T* try_emplace_back(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args&&...>);

        /**
        * @brief Destroys the last element.
        *
        * @pre `not empty()`.
        * @post `size()` has decreased by one.
        *
        * @warning Asserts in debug builds if the container is empty; calling it on an empty
        *          container is undefined behaviour in release builds.
        */
        void pop_back() noexcept;

        /**
        * @brief Erases the element at `index` by moving the last element into its place.
        *
        * Constant time, at the cost of order: the element that was last takes the erased
        * element's index. Use it when the container is an unordered set of records - a
        * scheduler's live-task list, a free list - and prefer `std::remove_if` plus a size
        * trim when relative order carries meaning.
        *
        * To erase several elements in one pass, walk **from the back**: erasing index `i`
        * only ever disturbs index `i` itself and the final slot, so indices below `i` keep
        * their meaning while indices above it do not.
        *
        * @param index Position of the element to erase.
        *
        * @pre `index < size()`.
        * @post `size()` has decreased by one.
        *
        * @warning This is the one operation that invalidates references: the erased element
        *          is destroyed, and the previously-last element is now reachable at `index`
        *          rather than at its old position. Every *other* element keeps its address.
        * @warning Asserts in debug builds if `index` is out of range.
        */
        void swap_erase(size_type index) noexcept;

        /**
        * @brief Destroys every element, leaving the container empty.
        *
        * @post `size() == 0`.
        * @note Idempotent: clearing an empty container is a no-op.
        */
        void clear() noexcept;

        /**
        * @brief Returns the number of live elements.
        * @return The current element count, never greater than `Capacity`.
        */
        [[nodiscard]] size_type size() const noexcept;

        /**
        * @brief Returns the fixed maximum number of elements.
        * @return `Capacity`, as a constant expression.
        */
        [[nodiscard]] static constexpr size_type capacity() noexcept { return Capacity; }

        /**
        * @brief Returns whether the container holds no elements.
        * @return `true` iff `size() == 0`.
        */
        [[nodiscard]] bool empty() const noexcept;

        /**
        * @brief Returns whether the container cannot accept another element.
        * @return `true` iff `size() == Capacity`.
        */
        [[nodiscard]] bool full() const noexcept;

        /**
        * @brief Accesses the element at `index`.
        *
        * @param index Position of the element.
        * @return Reference to that element.
        *
        * @pre `index < size()`.
        * @warning Asserts in debug builds if `index` is out of range; unchecked in release.
        */
        [[nodiscard]] reference operator[](size_type index) noexcept;

        /// @brief Const overload of `operator[]`. @copydetails operator[](size_type)
        [[nodiscard]] const_reference operator[](size_type index) const noexcept;

        /**
        * @brief Returns an iterator to the first element.
        * @return Pointer to the first element, equal to @ref end when empty.
        */
        [[nodiscard]] iterator begin() noexcept;

        /// @brief Const overload of `begin`. @copydetails begin()
        [[nodiscard]] const_iterator begin() const noexcept;

        /**
        * @brief Returns an iterator one past the last element.
        * @return Pointer one past the last element.
        */
        [[nodiscard]] iterator end() noexcept;

        /// @brief Const overload of `end`. @copydetails end()
        [[nodiscard]] const_iterator end() const noexcept;

        /// @brief Const iterator to the first element. @copydetails begin()
        [[nodiscard]] const_iterator cbegin() const noexcept;

        /// @brief Const iterator one past the last element. @copydetails end()
        [[nodiscard]] const_iterator cend() const noexcept;

        /**
        * @brief Accesses the first element.
        * @return Reference to the first element.
        * @pre `not empty()`.
        * @warning Asserts in debug builds if the container is empty.
        */
        [[nodiscard]] reference front() noexcept;

        /// @brief Const overload of `front`. @copydetails front()
        [[nodiscard]] const_reference front() const noexcept;

        /**
        * @brief Accesses the last element.
        * @return Reference to the last element.
        * @pre `not empty()`.
        * @warning Asserts in debug builds if the container is empty.
        */
        [[nodiscard]] reference back() noexcept;

        /// @brief Const overload of `back`. @copydetails back()
        [[nodiscard]] const_reference back() const noexcept;

    private:
        /// @brief Laundered typed pointer to slot `index`, live or not.
        [[nodiscard]] T* slot_at(size_type index) noexcept;

        /// @brief Const overload of @ref slot_at.
        [[nodiscard]] const T* slot_at(size_type index) const noexcept;

        /// @brief Raw storage for `Capacity` objects; the first `_size` slots are live.
        alignas(T) std::byte _storage[Capacity * sizeof(T)];

        /// @brief Number of live elements, and so the external occupancy `slot<T>` would not need.
        size_type _size = 0;
    };

    /**
    * @name Reference-type poison pills
    * @brief Explicitly disable `static_vector<U&, N>` and `static_vector<U&&, N>` with a clear diagnostic.
    * @{
    */

    /// @cond etools_internal
    template<typename U, std::size_t Capacity>
    class static_vector<U&, Capacity> {
        static_assert(meta::always_false_v<U>,
            "etools::memory::static_vector<T&, N> is disabled. "
            "Store `std::reference_wrapper<T>` to hold references, or "
            "`static_vector<std::remove_reference_t<T>, N>` to own objects.");
    };

    template<typename U, std::size_t Capacity>
    class static_vector<U&&, Capacity> {
        static_assert(meta::always_false_v<U>,
            "etools::memory::static_vector<T&&, N> is disabled. "
            "Store `std::reference_wrapper<T>` to hold references, or "
            "`static_vector<std::remove_reference_t<T>, N>` to own objects.");
    };
    /// @endcond
    /** @} */

} // namespace etools::memory

#include "static_vector.tpp"
#endif // ETOOLS_MEMORY_STATIC_VECTOR_HPP_
