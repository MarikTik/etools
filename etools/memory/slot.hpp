// SPDX-License-Identifier: MIT
/**
* @file slot.hpp
*
* @brief Value-type utility for type-safe in-place object construction in raw storage.
*
* @ingroup etools_memory etools::memory
*
* This header defines the `slot<T>` class template, a utility designed for embedded or
* resource-constrained environments where dynamic memory allocation is unavailable or undesirable.
*
* A `slot<T>` owns an aligned, correctly-sized byte buffer and manages the lifetime of at
* most one `T` constructed in-place within it (via placement new). It is a **value type**:
* each `slot<T>` object owns its own storage, exactly like `std::optional<T>` — two `slot<T>`
* objects are independent. Construct as many as you need, wherever you need them (stack,
* static, as a member of a larger aggregate). There is no global/singleton storage.
*
* The `slot<T>` provides manual lifecycle control (`emplace`, `reset`) plus an
* `std::optional`-shaped access surface (`has_value`, `operator bool`, `operator*`,
* `operator->`). The contained object, if any, is destroyed automatically by the
* `slot` destructor (RAII).
*
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
*      - Changed type of `_mem` to direct aligned buffer with using std::byte array instead following
*        future deprecation of `std::aligned_storage`.
* - 2026-05-30
*      - Reworked from a global singleton into a value type. Each `slot<T>` now owns its
*        own (non-static) storage; the `instance()` accessor was removed. Added an RAII
*        destructor, conditional move semantics (movable iff `T` is move-constructible,
*        with honest type traits via the standard base-class technique), and an
*        `std::optional`-shaped access surface.
* - 2026-06-23
*      - API trim toward the `std::optional` model. Removed `construct()` (it was
*        `emplace()` plus a debug-only assert — a tripwire callers can't see). Removed
*        the `get()` accessors (nullable raw-pointer access is a smart-pointer idiom that
*        invites unchecked deref; `operator bool` + `operator*` already cover safe access).
*        Renamed `destroy()` to `reset()` to match `std::optional`. `emplace()` now returns
*        `T&` instead of `T*`, matching `std::optional::emplace`.
*/

#ifndef ETOOLS_MEMORY_SLOT_HPP_
#define ETOOLS_MEMORY_SLOT_HPP_
#include "../meta/traits.hpp"   // etools::meta::always_false_v
#include <cassert>
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

static_assert(__cpp_lib_launder >= 201606L,
    "etools::memory::slot requires <new>'s std::launder (C++17, "
    "__cpp_lib_launder >= 201606). A no-op shim would silently miscompile "
    "under the optimizer, so we refuse to build instead.");

namespace etools::memory {

    namespace details {
        /**
        * @brief Storage, lifecycle, and (custom) move logic for `slot<T>`.
        *
        * Holds the aligned buffer and engaged flag, and implements all lifecycle operations.
        * The move special members are **user-provided** here because a `slot` must *relocate*
        * the contained object (move-construct a new `T` at the destination), not byte-copy the
        * buffer — a defaulted move would memcpy and break any `T` with internal pointers.
        *
        * Conditional deletion of those move members (when `T` is not move-constructible) is
        * layered on top by `slot_move_ctrl`; see that type. This split is the standard-library
        * technique (`std::optional`'s `_Move_ctor_base`), required because `enable_if` cannot
        * disable a non-template special member.
        *
        * @tparam T The contained type.
        */
        template <typename T>
        struct slot_base {
            static_assert(std::is_nothrow_destructible_v<T>,
                "slot<T> requires T to be nothrow-destructible; a throwing destructor "
                "could leave the slot in a half-state during emplace().");

            alignas(T) std::byte _mem[sizeof(T)];
            bool _constructed = false;

            slot_base() noexcept = default;

            ~slot_base() noexcept { reset(); }

            slot_base(const slot_base&) = delete;
            slot_base& operator=(const slot_base&) = delete;

            slot_base(slot_base&& other) noexcept(std::is_nothrow_move_constructible_v<T>) {
                if (other._constructed) {
                    ::new (static_cast<void*>(&_mem)) T(std::move(*other.ptr()));
                    _constructed = true;
                    other.reset();
                }
            }

            slot_base& operator=(slot_base&& other) noexcept(std::is_nothrow_move_constructible_v<T>) {
                if (this == &other) return *this;
                reset();
                if (other._constructed) {
                    ::new (static_cast<void*>(&_mem)) T(std::move(*other.ptr()));
                    _constructed = true;
                    other.reset();
                }
                return *this;
            }

            /// @brief Laundered typed pointer into the buffer (no engaged check).
            T* ptr() noexcept { return std::launder(reinterpret_cast<T*>(&_mem)); }
            const T* ptr() const noexcept { return std::launder(reinterpret_cast<const T*>(&_mem)); }

            /// @brief Destroy the contained object if engaged. Idempotent.
            void reset() noexcept {
                if (_constructed) {
                    ptr()->~T();
                    _constructed = false;
                }
            }
        };

        /**
        * @brief Conditional move-enablement layer for `slot<T>`.
        *
        * When `T` is move-constructible the primary template inherits `slot_base`'s
        * user-provided move members unchanged. When `T` is not move-constructible the
        * specialization deletes the move special members, so `std::is_move_constructible_v`
        * reports the truth for `slot<T>` and for any aggregate (e.g. `static_factory`) that
        * contains one.
        *
        * @tparam T        The contained type.
        * @tparam Movable  Defaulted; do not specify explicitly.
        */
        template <typename T, bool Movable = std::is_move_constructible_v<T>>
        struct slot_move_ctrl : slot_base<T> {
            using slot_base<T>::slot_base;
        };

        template <typename T>
        struct slot_move_ctrl<T, false> : slot_base<T> {
            using slot_base<T>::slot_base;
            slot_move_ctrl() = default;
            slot_move_ctrl(const slot_move_ctrl&) = delete;
            slot_move_ctrl& operator=(const slot_move_ctrl&) = delete;
            slot_move_ctrl(slot_move_ctrl&&) = delete;
            slot_move_ctrl& operator=(slot_move_ctrl&&) = delete;
        };
    } // namespace details

    /**
    * @class slot
    * @brief Value type providing aligned storage and manual lifetime control for one `T`.
    *
    * A `slot<T>` owns an `alignas(T)`-aligned buffer of `sizeof(T)` bytes and tracks whether
    * a `T` is currently constructed within it. The object is constructed and destroyed
    * explicitly; the slot destructor cleans up any remaining object (RAII).
    *
    * @tparam T The type of object to store in this slot.
    *
    * ### Key Characteristics:
    * 
    * - At most one `T` is alive in the slot at any time.
    * - Storage is in-place (no heap allocation); the `T` lives inside the slot's bytes.
    * - Value semantics: each `slot<T>` owns independent storage. No singleton.
    * - Copyable: never (a live `T` in raw storage is not generally copyable here).
    * - Movable: iff `T` is move-constructible (relocates the contained object). The type
    *   traits report this honestly (`std::is_move_constructible_v<slot<T>>`).
    *
    * ### Usage Example:
    *
    * ```cpp
    * struct Foo { int x; };
    * slot<Foo> s;            // owns its own storage
    * s.emplace(42);          // construct Foo in-place with x = 42
    * if (s) { Foo& f = *s; } // access via operator bool / operator*
    * s.reset();              // destroy the object (also done by ~slot)
    * ```
    *
    * @invariant `_mem` is aligned for `T` and large enough to hold a `T`.
    * @invariant `_constructed == true` iff `_mem` currently contains a live `T`.
    *
    * @warning
    * - `reset()` is idempotent — calling it on an empty slot is a no-op.
    * - `operator*` / `operator->` have a precondition that the slot is engaged; they
    *   assert in debug builds. Check `has_value()` (or `operator bool`) first when the
    *   state is unknown.
    * 
    * - This class is NOT thread-safe.
    *
    * @note Move semantics diverge from `std::optional`: a moved-from `slot` is left **empty**
    *       (disengaged), matching the manual-lifetime model, whereas `std::optional` leaves
    *       the source engaged-but-moved-from.
    * @note `T` must be nothrow-destructible; this is enforced by `static_assert`.
    */
    template<typename T>
    class slot : private details::slot_move_ctrl<T> {
        using base = details::slot_move_ctrl<T>;
    public:
        /// @brief The contained value type.
        using value_type = T;

        using base::base;

        /**
        * @brief Constructs an empty slot.
        *
        * @post `has_value() == false`.
        */
        slot() noexcept = default;

        /**
        * @brief Constructs or replaces the object in-place with the given arguments.
        *
        * If an object is already constructed, it is destroyed before the new one is built.
        *
        * @tparam Args Argument types for T's constructor.
        * @param args Arguments forwarded to T's constructor.
        * @return Reference to the newly constructed object.
        *
        * @post The slot is engaged and holds the value built from `args...`. Any
        *       previously-held object has been destroyed exactly once.
        *
        * @note If the slot is already engaged, `emplace()` destroys the current object then
        *       rebuilds; it does not assert. This is the safe choice when the slot's state
        *       is unknown.
        * @note Strong guarantee on the *new* object: the old object is destroyed first,
        *       then if `T`'s constructor throws the slot is left empty.
        * @note The function is `noexcept` iff `T`'s selected constructor is `noexcept`.
        */
        template<typename... Args>
        inline T& emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args&&...>);

        /**
        * @brief Destroys the currently constructed object, if any.
        *
        * @post The slot is empty; `has_value()` returns `false`.
        *
        * @note Idempotent: calling `reset()` on an empty slot is a no-op and does not
        *       invoke `~T()`.
        * @note The slot destructor calls this automatically (RAII); call it explicitly only
        *       to end the contained object's lifetime before the slot's own scope ends.
        */
        inline void reset() noexcept;

        /**
        * @brief Returns whether the slot currently holds a constructed object.
        *
        * @return `true` iff a `T` is constructed in the slot.
        * @note Cheap: reads the engaged flag only.
        */
        [[nodiscard]] inline bool has_value() const noexcept;

        /**
        * @brief Boolean conversion — equivalent to `has_value()`.
        *
        * @return `true` iff the slot is engaged.
        * @note `explicit` to avoid accidental integral conversions.
        */
        [[nodiscard]] explicit inline operator bool() const noexcept;

        /**
        * @brief Dereferences the contained object.
        *
        * @return Reference to the contained `T`.
        * @pre The slot is engaged (`has_value() == true`).
        * @warning Asserts in debug builds if the slot is empty; dereferencing an empty
        *          slot is undefined behaviour in release builds.
        */
        [[nodiscard]] inline T& operator*() noexcept;

        /// @brief Const overload of `operator*`. @copydetails operator*()
        [[nodiscard]] inline const T& operator*() const noexcept;

        /**
        * @brief Member access on the contained object.
        *
        * @return Pointer to the contained `T`.
        * @pre The slot is engaged (`has_value() == true`).
        * @warning Asserts in debug builds if the slot is empty.
        */
        [[nodiscard]] inline T* operator->() noexcept;

        /// @brief Const overload of `operator->`. @copydetails operator->()
        [[nodiscard]] inline const T* operator->() const noexcept;
    };


    /**
    * @name Reference-type poison pills
    * @brief Explicitly disable `slot<U&>` and `slot<U&&>` with a clear diagnostic.
    * @{
    */

    /// @cond etools_internal
    template<typename U>
    class slot<U&> {
        static_assert(meta::always_false_v<U>,
            "etools::memory::slot<T&> is disabled. "
            "Use `slot<std::remove_reference_t<T>>` to own an object, or `std::reference_wrapper<T>` to bind.");
    };

    template<typename U>
    class slot<U&&> {
        static_assert(meta::always_false_v<U>,
            "etools::memory::slot<T&&> is disabled. "
            "Use `slot<std::remove_reference_t<T>>` to own an object, or `std::reference_wrapper<T>` to bind.");
    };
    /// @endcond
    /** @} */
} // namespace etools::memory

#include "slot.tpp"
#endif // ETOOLS_MEMORY_SLOT_HPP_
