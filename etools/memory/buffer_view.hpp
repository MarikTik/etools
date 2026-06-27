// SPDX-License-Identifier: MIT
/**
* @file buffer_view.hpp
*
* @brief Declares the `buffer_view` class used for non-owning, read-only access to buffer contents.
*
* @ingroup etools_memory etools::memory
*
* The `buffer_view` class provides a lightweight, read-only interface to serialized data.
* It does not manage or own memory, making it suitable for inspecting the contents of a buffer
* without copying or taking ownership.
*
* This class complements `etools::memory::buffer` by supporting a view-like access pattern
* similar to `std::string_view` or `std::span`, while maintaining unpacking functionality.
*
* @note This class is trivially copyable and moveable.
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
* @date 2025-07-20
*
* @copyright
* MIT License
* Copyright (c) 2025 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_MEMORY_BUFFER_VIEW_HPP_
#define ETOOLS_MEMORY_BUFFER_VIEW_HPP_
#include <cstddef>
#include <tuple>
#include <optional>

namespace etools::memory {

    /**
    * @class buffer_view
    * @brief Non-owning, read-only view over a serialized byte buffer.
    *
    * The `buffer_view` class provides non-owning access to a contiguous memory buffer
    * containing serialized values. It allows unpacking typed data using the same API as
    * `buffer`, but without memory management overhead.
    *
    * This class is useful when deserializing incoming data that is managed externally
    * (e.g., received over the network or passed by reference).
    *
    * @invariant The pair `(_data, _size)` describes a contiguous read-only range
    *            owned externally. The view never modifies, allocates, or deallocates.
    * @invariant Trivially copyable, trivially movable, trivially destructible.
    *
    * @warning The lifetime of the pointed-to memory must exceed that of the buffer_view.
    *          The class does not perform bounds checking or memory safety.
    */
    class buffer_view{
    public:
        /**
        * @brief Constructs a buffer_view from a raw data pointer and the length of the viewed range.
        *
        * @param data A pointer to the external memory block (must remain valid during use).
        * @param size The length of the viewed range in bytes.
        *
        * @pre `data` either points to at least `size` valid bytes, or is `nullptr` with `size == 0`.
        * @post `this->data() == data` and `this->size() == size`.
        *
        * @note `noexcept` — the constructor stores the pointer and size only.
        * @warning No null-check or bounds-check is performed. Passing a non-null
        *          `data` with `size` larger than the buffer is undefined behaviour
        *          on any subsequent `unpack()`.
        */
        inline buffer_view(const std::byte* data, std::size_t size) noexcept;

        /**
        * @brief Unpacks the buffer contents into a tuple of typed values.
        *
        * Deserializes the buffer into strongly typed values via
        * `eser::flat::deserialize(...).to<std::tuple<Ts...>>()`.
        *
        * @tparam Ts... The types to deserialize and extract from the view.
        * @return `std::nullopt` if the buffer holds fewer than the required bytes
        *         (`sizeof(Ts) + ...`); otherwise an engaged optional holding the tuple.
        *
        * @note Strictly all-or-nothing: a viewed range too short for `Ts...` yields
        *       `nullopt` and reads nothing — there is no partial/zero fill.
        * @note Does not modify the viewed range.
        */
        template<typename ...Ts>
        [[nodiscard]] inline std::optional<std::tuple<Ts...>> unpack() const;

        /**
        * @brief Returns a pointer to the underlying byte data.
        *
        * Provides read-only access to the viewed memory buffer.
        *
        * @return A pointer to the beginning of the memory block (may be `nullptr`
        *         if the view was constructed with a null pointer).
        *
        * @post Returns the same pointer that was passed at construction.
        */
        [[nodiscard]] inline const std::byte* data() const noexcept;

        /**
        * @brief Returns the length of the viewed memory range in bytes.
        *
        * The view does not distinguish "used" from "total" bytes — it is a
        * fixed read-only window over an externally-owned buffer, so the only
        * meaningful length is the size passed at construction. Matches the
        * `std::string_view::size()` / `std::span::size()` convention.
        *
        * @return Number of bytes in the viewed range.
        *
        * @post Returns the same value that was passed at construction.
        */
        [[nodiscard]] inline std::size_t size() const noexcept;

        /// Default copy constructor (trivial shallow copy).
        buffer_view(const buffer_view&) = default;
        /// Default copy assignment operator (trivial shallow copy).
        buffer_view& operator=(const buffer_view&) = default;
        /// Move constructor (trivial shallow move).
        buffer_view(buffer_view&&) = default;
        /// Move assignment operator (trivial shallow move).
        buffer_view& operator=(buffer_view&&) = default;
        
    private:
        /// Pointer to the viewed byte range. Not owned.
        const std::byte* _data;

        /// Length of the viewed range in bytes.
        std::size_t _size;
    };
}
#include "buffer_view.tpp"
#endif // ETOOLS_MEMORY_BUFFER_VIEW_HPP_