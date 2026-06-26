// SPDX-License-Identifier: MIT
/**
* @file envelope.hpp
*
* @brief Defines the `envelope` class for owning and serializing a fixed-size byte buffer.
*
* @ingroup etools_memory etools::memory
*
* The `envelope` class encapsulates a contiguous block of memory used for transmitting
* serialized data between subsystems. It provides both ownership semantics for the
* underlying buffer and convenient methods for packing and unpacking structured values.
*
* Unlike traditional byte buffers, `envelope` is designed to allow flexible memory sources.
* You can allocate memory dynamically (heap), from a memory pool, or reuse a static buffer.
* This is possible due to the support for custom deleters via a template parameter.
*
* ### Use Cases
* - Packing structured output into a transmittable buffer
* - Passing serialized input across a layer boundary in a uniform way
* - Reusing memory from a static pool (via custom deleter)
*
* ### Example Usage
* ```cpp
* // Standard usage with heap allocation
* auto buffer = std::make_unique<std::byte[]>(1024);
* etools::memory::envelope env(std::move(buffer), 1024);
* env.pack(42, 'A');
*
* // Reusing external memory without deallocation
* std::byte static_buffer[256];
* auto no_delete = [](std::byte*) {}; // noop deleter
* etools::memory::envelope<decltype(no_delete)>
*     static_env(std::unique_ptr<std::byte[], decltype(no_delete)>{static_buffer, no_delete}, 256);
* ```
*
* @note This class is move-only due to unique ownership of the memory block.
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2025-08-06
*
* @copyright
* MIT License
* Copyright (c) 2025 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_MEMORY_ENVELOPE_HPP_
#define ETOOLS_MEMORY_ENVELOPE_HPP_
#include <memory>
#include <cstddef>
#include <optional>
#include <tuple>

namespace etools::memory{
    /**
    * @class envelope
    *
    * @brief Owns and manages a block of memory used for transmitting serialized data.
    *
    * The `envelope` class acts as a flexible data container. It provides:
    *
    * - ownership of a memory block via `unique_ptr` with a customizable deleter
    * - methods for packing structured data into the block
    * - methods for unpacking data from the block into typed objects
    *
    * @tparam Deleter The function to delete the memory owned by `data` pointer when `envelope` is destroyed.
    *         Defaulted to `std::default_delete<std::byte[]>`.
    *
    * @invariant `_size <= _capacity` at all times.
    * @invariant Either `_data` is non-null (the envelope owns a buffer) or
    *            the envelope has been moved from / default-constructed and
    *            both `_size` and `_capacity` are zero.
    * @invariant The envelope owns its buffer uniquely; no two envelopes share storage.
    *
    * @note The envelope is a move-only type and cannot be copied.
    */
    template<typename Deleter = std::default_delete<std::byte[]>>
    class envelope{
    public:
        /**
        * @brief Constructs an envelope by taking ownership of the given memory block.
        *
        * This constructor is typically used when the entire buffer is reserved
        * for future writes (packing).
        *
        * @param data A unique_ptr to the allocated byte array containing the envelope contents.
        * @param capacity The total capacity of the memory block in bytes.
        *
        * @post `this->capacity() == capacity` and `this->size() == 0`.
        * @post `this->data()` returns the address of the buffer formerly owned by `data`.
        *
        * @note The `Deleter` template parameter determines how the memory will be released.
        *       This enables support for non-standard memory sources like stack-allocated memory,
        *       memory pools, or aligned arenas.
        * @note No assertion is made on `capacity > 0`; a zero-capacity envelope is
        *       legal but cannot be packed into.
        */
        inline envelope(std::unique_ptr<std::byte[], Deleter> data, std::size_t capacity);

        /**
        * @brief Constructs an envelope from a pre-populated memory block.
        *
        * Use this when the memory already contains a serialized payload, and you want to unpack it later.
        *
        * @param data A unique_ptr to the byte array containing the envelope contents.
        * @param capacity The total size of the memory block in bytes.
        * @param size The number of bytes already used (e.g., serialized content).
        *
        * @pre `size <= capacity`.
        * @post `this->capacity() == capacity` and `this->size() == size`.
        *
        * @warning Violating the precondition triggers an `assert` in debug builds.
        *          In release builds the value is trusted; the caller is responsible
        *          for honoring the contract.
        */
        inline envelope(std::unique_ptr<std::byte[], Deleter> data, std::size_t capacity, std::size_t size);

        /**
        * @brief Move constructor.
        *
        * Transfers ownership of the memory block from another envelope.
        *
        * @param other The envelope to move from.
        *
        * @post `*this` owns the buffer formerly owned by `other`, with the same
        *       `size()` and `capacity()`.
        * @post `other` is left in a moved-from state: `data()` is `nullptr`,
        *       `size()` is `0`, `capacity()` is `0`.
        * @note `noexcept` — the operation only moves a `unique_ptr` and two `size_t`s.
        */
        envelope(envelope&& other) noexcept;

        /**
        * @brief Move assignment operator.
        *
        * Transfers ownership of the memory block from another envelope.
        *
        * @param other The envelope to move from.
        * @return Reference to this object.
        *
        * @post Same as the move constructor; `other` is left empty.
        * @post Self-assignment is a no-op (`*this` is unchanged).
        * @note `noexcept` — `unique_ptr` move-assignment is `noexcept`.
        */
        envelope& operator=(envelope&& other) noexcept;

        /**
        * @brief Unpacks the envelope contents into a tuple of typed values.
        *
        * Deserializes the underlying byte array into strongly typed values via
        * `eser::flat::deserialize(...).to<std::tuple<Ts...>>()`.
        *
        * @tparam Ts... The types to deserialize and extract from the envelope.
        * @return `std::nullopt` if the buffer holds fewer than the required bytes
        *         (`sizeof(Ts) + ...`); otherwise an engaged optional holding the tuple.
        *
        * @note Strictly all-or-nothing: a buffer too short for `Ts...` yields `nullopt`
        *       and reads nothing — there is no partial/zero fill.
        * @note Does not modify the envelope.
        */
        template<typename... Ts>
        [[nodiscard]] inline std::optional<std::tuple<Ts...>> unpack() const;

        /**
        * @brief Packs one or more typed values into the envelope's memory block.
        *
        * This method serializes the given values into the memory block owned by
        * the envelope.
        *
        * @tparam Ts... The types of the values to serialize.
        * @param args The values to serialize into the envelope.
        *
        * @pre The envelope owns a non-null buffer (i.e. has not been moved from).
        * @post `this->size()` reflects the number of bytes written by the
        *       underlying serializer.
        * @post Any previously-packed contents have been overwritten in-place.
        *
        * @warning Violating the precondition triggers an `assert` in debug builds.
        * @warning Total serialized size of `args...` must not exceed `capacity()`;
        *          the underlying serializer enforces this with its own diagnostic.
        */
        template<typename... Ts>
        inline void pack(Ts&&... args);

        /**
        * @brief Returns a pointer to the internal byte data.
        *
        * Provides read-only access to the internal memory block.
        *
        * @return A const pointer to the byte data held by the envelope, or `nullptr`
        *         if the envelope has been moved from.
        *
        * @post The returned pointer is either `nullptr` or points to the start
        *       of the owned buffer.
        */
        [[nodiscard]] inline const std::byte* data() const noexcept;

        /**
        * @brief Returns the size of the used portion of the envelope's memory.
        *
        * @return The number of used bytes stored in the envelope.
        *
        * @post Return value is always `<= capacity()`.
        */
        [[nodiscard]] inline std::size_t size() const noexcept;

        /**
        * @brief Returns the total capacity of the envelope's memory.
        *
        * @return The number of bytes available in the underlying buffer.
        *
        * @post Return value is the capacity passed at construction, or `0` after move.
        */
        [[nodiscard]] inline std::size_t capacity() const noexcept;

        /// Deleted copy constructor — envelope is a unique-ownership type.
        envelope(const envelope&) = delete;

        /// Deleted copy assignment operator — envelope is a unique-ownership type.
        envelope& operator=(const envelope&) = delete;
    private:
        /// Owned buffer. Null when moved-from or default-constructed.
        std::unique_ptr<std::byte[], Deleter> _data{};
        /// Number of bytes currently used in the buffer. Always `<= _capacity`.
        std::size_t _size{0};
        /// Total length of the owned buffer in bytes. Zero when moved-from.
        std::size_t _capacity{0};
    };

} // namespace etools::memory

#include "envelope.tpp"
#endif // ETOOLS_MEMORY_ENVELOPE_HPP_