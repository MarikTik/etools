// SPDX-License-Identifier: MIT
/**
* @file buffer.hpp
*
* @brief Defines the `buffer` class for owning and serializing a fixed-size byte buffer.
*
* @ingroup etools_memory etools::memory
*
* The `buffer` class encapsulates a contiguous block of memory used for transmitting
* serialized data between subsystems. It provides both ownership semantics for the
* underlying buffer and convenient methods for packing and unpacking structured values.
*
* Unlike traditional byte buffers, `buffer` is designed to allow flexible memory sources.
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
* etools::memory::buffer env(std::move(buffer), 1024);
* env.pack(42, 'A');
*
* // Reusing external memory without deallocation
* std::byte static_buffer[256];
* auto no_delete = [](std::byte*) {}; // noop deleter
* etools::memory::buffer<decltype(no_delete)>
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
#ifndef ETOOLS_MEMORY_BUFFER_HPP_
#define ETOOLS_MEMORY_BUFFER_HPP_
#include <memory>
#include <cstddef>
#include <optional>
#include <tuple>

namespace etools::memory{
    /**
    * @class buffer
    *
    * @brief Owns and manages a block of memory used for transmitting serialized data.
    *
    * The `buffer` class acts as a flexible data container. It provides:
    *
    * - ownership of a memory block via `unique_ptr` with a customizable deleter
    * - methods for packing structured data into the block
    * - methods for unpacking data from the block into typed objects
    *
    * @tparam Deleter The function to delete the memory owned by `data` pointer when `buffer` is destroyed.
    *         Defaulted to `std::default_delete<std::byte[]>`.
    *
    * @invariant `_size <= _capacity` at all times.
    * @invariant Either `_data` is non-null (the buffer owns a buffer) or
    *            the buffer has been moved from / default-constructed and
    *            both `_size` and `_capacity` are zero.
    * @invariant The buffer owns its buffer uniquely; no two buffers share storage.
    *
    * @note The buffer is a move-only type and cannot be copied.
    */
    template<typename Deleter = std::default_delete<std::byte[]>>
    class buffer{
    public:
        /**
        * @brief Constructs an empty buffer that owns no memory.
        *
        * @post `data() == nullptr`, `size() == 0`, `capacity() == 0`.
        *
        * @note Available only when `Deleter` is default-constructible (the default
        *       `std::default_delete`, or a function-pointer deleter). A buffer backed by
        *       a capture-stateful deleter must be constructed with an explicit block.
        */
        buffer() noexcept = default;

        /**
        * @brief Constructs a buffer by taking ownership of the given memory block.
        *
        * This constructor is typically used when the entire buffer is reserved
        * for future writes (packing).
        *
        * @param data A unique_ptr to the allocated byte array containing the buffer contents.
        * @param capacity The total capacity of the memory block in bytes.
        *
        * @post `this->capacity() == capacity` and `this->size() == 0`.
        * @post `this->data()` returns the address of the buffer formerly owned by `data`.
        *
        * @note The `Deleter` template parameter determines how the memory will be released.
        *       This enables support for non-standard memory sources like stack-allocated memory,
        *       memory pools, or aligned arenas.
        * @note No assertion is made on `capacity > 0`; a zero-capacity buffer is
        *       legal but cannot be packed into.
        */
        inline buffer(std::unique_ptr<std::byte[], Deleter> data, std::size_t capacity);

        /**
        * @brief Constructs a buffer from a pre-populated memory block.
        *
        * Use this when the memory already contains a serialized payload, and you want to unpack it later.
        *
        * @param data A unique_ptr to the byte array containing the buffer contents.
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
        inline buffer(std::unique_ptr<std::byte[], Deleter> data, std::size_t capacity, std::size_t size);

        /**
        * @brief Move constructor.
        *
        * Transfers ownership of the memory block from another buffer.
        *
        * @param other The buffer to move from.
        *
        * @post `*this` owns the buffer formerly owned by `other`, with the same
        *       `size()` and `capacity()`.
        * @post `other` is left in a moved-from state: `data()` is `nullptr`,
        *       `size()` is `0`, `capacity()` is `0`.
        * @note `noexcept` - the operation only moves a `unique_ptr` and two `size_t`s.
        */
        buffer(buffer&& other) noexcept;

        /**
        * @brief Move assignment operator.
        *
        * Transfers ownership of the memory block from another buffer.
        *
        * @param other The buffer to move from.
        * @return Reference to this object.
        *
        * @post Same as the move constructor; `other` is left empty.
        * @post Self-assignment is a no-op (`*this` is unchanged).
        * @note `noexcept` - `unique_ptr` move-assignment is `noexcept`.
        */
        buffer& operator=(buffer&& other) noexcept;

        /**
        * @brief Unpacks the buffer contents into a tuple of typed values.
        *
        * Deserializes the underlying byte array into strongly typed values via
        * `eser::flat::deserialize(...).to<std::tuple<Ts...>>()`.
        *
        * @tparam Ts... The types to deserialize and extract from the buffer.
        * @return `std::nullopt` if the buffer holds fewer than the required bytes
        *         (`sizeof(Ts) + ...`); otherwise an engaged optional holding the tuple.
        *
        * @note Strictly all-or-nothing: a buffer too short for `Ts...` yields `nullopt`
        *       and reads nothing - there is no partial/zero fill.
        * @note Does not modify the buffer.
        */
        template<typename... Ts>
        [[nodiscard]] inline std::optional<std::tuple<Ts...>> unpack() const;

        /**
        * @brief Packs one or more typed values into the buffer's memory block.
        *
        * Serializes the given values into the owned block via `eser::flat::serialize`.
        *
        * @tparam Ts... The types of the values to serialize.
        * @param args The values to serialize into the buffer.
        * @return The number of bytes written, or `0` if `args...` do not fit in
        *         `capacity()` (in which case nothing is written).
        *
        * @pre The buffer owns a non-null block (i.e. has not been moved from).
        * @post `this->size()` equals the returned byte count.
        * @post Any previously-packed contents have been overwritten in-place.
        *
        * @warning Violating the precondition triggers an `assert` in debug builds.
        * @note Not `[[nodiscard]]`: the byte count is available for callers that want it,
        *       but the serializer never overflows the buffer, so ignoring it is safe.
        */
        template<typename... Ts>
        inline std::size_t pack(Ts&&... args);

        /**
        * @brief Returns a pointer to the internal byte data.
        *
        * Provides read-only access to the internal memory block.
        *
        * @return A const pointer to the byte data held by the buffer, or `nullptr`
        *         if the buffer has been moved from.
        *
        * @post The returned pointer is either `nullptr` or points to the start
        *       of the owned buffer.
        */
        [[nodiscard]] inline const std::byte* data() const noexcept;

        /**
        * @brief Returns the size of the used portion of the buffer's memory.
        *
        * @return The number of used bytes stored in the buffer.
        *
        * @post Return value is always `<= capacity()`.
        */
        [[nodiscard]] inline std::size_t size() const noexcept;

        /**
        * @brief Returns the total capacity of the buffer's memory.
        *
        * @return The number of bytes available in the underlying buffer.
        *
        * @post Return value is the capacity passed at construction, or `0` after move.
        */
        [[nodiscard]] inline std::size_t capacity() const noexcept;

        /// Deleted copy constructor - buffer is a unique-ownership type.
        buffer(const buffer&) = delete;

        /// Deleted copy assignment operator - buffer is a unique-ownership type.
        buffer& operator=(const buffer&) = delete;
    private:
        /// Owned buffer. Null when moved-from or default-constructed.
        std::unique_ptr<std::byte[], Deleter> _data{};
        /// Number of bytes currently used in the buffer. Always `<= _capacity`.
        std::size_t _size{0};
        /// Total length of the owned buffer in bytes. Zero when moved-from.
        std::size_t _capacity{0};
    };

} // namespace etools::memory

#include "buffer.tpp"
#endif // ETOOLS_MEMORY_BUFFER_HPP_