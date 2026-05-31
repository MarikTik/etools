// SPDX-License-Identifier: MIT
/**
* @file envelope_view.hpp
*
* @brief Declares the `envelope_view` class used for non-owning, read-only access to envelope contents.
*
* @ingroup etools_memory etools::memory
*
* The `envelope_view` class provides a lightweight, read-only interface to serialized data.
* It does not manage or own memory, making it suitable for inspecting the contents of an envelope
* without copying or taking ownership.
*
* This class complements `etools::memory::envelope` by supporting a view-like access pattern
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
#ifndef ETOOLS_MEMORY_ENVELOPE_VIEW_HPP_
#define ETOOLS_MEMORY_ENVELOPE_VIEW_HPP_
#include <cstddef>
#include <tuple>

namespace etools::memory {

    /**
    * @class envelope_view
    * @brief Non-owning, read-only view over a serialized byte buffer.
    *
    * The `envelope_view` class provides non-owning access to a contiguous memory buffer
    * containing serialized values. It allows unpacking typed data using the same API as
    * `envelope`, but without memory management overhead.
    *
    * This class is useful when deserializing incoming data that is managed externally
    * (e.g., received over the network or passed by reference).
    *
    * @warning The lifetime of the pointed-to memory must exceed that of the envelope_view.
    *          The class does not perform bounds checking or memory safety.
    */
    class envelope_view{
    public:
        /**
        * @brief Constructs an envelope_view from a raw data pointer and the length of the viewed range.
        *
        * @param data A pointer to the external memory block (must remain valid during use).
        * @param size The length of the viewed range in bytes.
        */
        inline envelope_view(const std::byte* data, std::size_t size) noexcept;

        /**
        * @brief Unpacks the envelope contents into a tuple of typed values.
        *
        * Deserializes the buffer into strongly typed values using the
        * `eser::binary::deserialize` API.
        *
        * @tparam Ts... The types to deserialize and extract from the view.
        * @return A tuple containing the deserialized values.
        */
        template<typename ...Ts>
        inline std::tuple<Ts...> unpack() const;

        /**
        * @brief Returns a pointer to the underlying byte data.
        *
        * Provides read-only access to the viewed memory buffer.
        *
        * @return A pointer to the beginning of the memory block.
        */
        inline const std::byte* data() const noexcept;

        /**
        * @brief Returns the length of the viewed memory range in bytes.
        *
        * The view does not distinguish "used" from "total" bytes — it is a
        * fixed read-only window over an externally-owned buffer, so the only
        * meaningful length is the size passed at construction. Matches the
        * `std::string_view::size()` / `std::span::size()` convention.
        *
        * @return Number of bytes in the viewed range.
        */
        inline std::size_t size() const noexcept;

        /// Default copy constructor (trivial shallow copy).
        envelope_view(const envelope_view&) = default;
        /// Default copy assignment operator (trivial shallow copy).
        envelope_view& operator=(const envelope_view&) = default;
        /// Move constructor (trivial shallow move).
        envelope_view(envelope_view&&) = default;
        /// Move assignment operator (trivial shallow move).
        envelope_view& operator=(envelope_view&&) = default;
        
    private:
        /// Pointer to the viewed byte range. Not owned.
        const std::byte* _data;

        /// Length of the viewed range in bytes.
        std::size_t _size;
    };
}
#include "envelope_view.tpp"
#endif // ETOOLS_MEMORY_ENVELOPE_VIEW_HPP_