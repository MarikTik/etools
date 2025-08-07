// SPDX-License-Identifier: MIT
/**
* @file envelope.hpp
*
* @brief Defines the `envelope` class for managing task parameters and results in the etask framework.
*
* @ingroup etools_memory etools::memory
*
* The `envelope` class encapsulates a contiguous block of memory used for transmitting
* serialized data between tasks and the communication layer. It provides both ownership
* semantics for dynamically allocated data and convenient methods for packing and unpacking
* structured parameters or results.
*
* Unlike traditional byte buffers, `envelope` is designed to allow flexible memory sources.
* You can allocate memory dynamically (heap), from a memory pool, or reuse a static buffer.
* This is possible due to the support for custom deleters via a template parameter.
*
* ### Use Cases
* - Packing structured task output and returning it via a result envelope
* - Passing serialized input data to tasks in a uniform way
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

namespace etools::memory{
    /**
    * @class envelope
    * 
    * @brief Owns and manages a block of memory used for transmitting serialized task parameters and results.
    *
    * The `envelope` class acts as a flexible data container for the etask communication protocol.
    * It provides:
    *
    * - ownership of a dynamically allocated memory block via unique_ptr
    * - methods for packing structured data into the block
    * - methods for unpacking data from the block into typed objects
    *
    * @tparam Deleter The function to delete the memory owned by `data` pointer when `envelope` is destroyed. 
    *         Defalted to `std::default_delete<std::byte[]>>`.
    * 
    * @note The envelope is a move-only type and cannot be copied.
    */
    template<typename Deleter = std::default_delete<std::byte[]>>
    class envelope{
    public:
        /**
        * @brief Constructs an envelope by taking ownership of the given memory block.
        *
        * This constructor is typically used when the entire buffer is reserved for future writes (packing).
        *
        * @param data A unique_ptr to the allocated byte array containing the envelope contents.
        * @param capacity The total capacity of the memory block in bytes.
        *
        * @note The `Deleter` template parameter determines how the memory will be released.
        *       This enables support for non-standard memory sources like stack-allocated memory,
        *       memory pools, or aligned arenas.
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
        * @note The `size` must be less than or equal to `capacity`.
        * @warning Passing an incorrect `size` will result in `Assertion Error` in debug mode,
        * in release however it would be replaced by capacity.
        */
        inline envelope(std::unique_ptr<std::byte[], Deleter> data, std::size_t capacity, std::size_t size);

        /**
        * @brief Move constructor.
        *
        * Transfers ownership of the memory block from another envelope.
        *
        * @param other The envelope to move from.
        */
        envelope(envelope&& other) noexcept;
        
        /**
        * @brief Move assignment operator.
        *
        * Transfers ownership of the memory block from another envelope.
        *
        * @param other The envelope to move from.
        * @return Reference to this object.
        */
        envelope& operator=(envelope&& other) noexcept;
        
        /**
        * @brief Unpacks the envelope contents into a tuple of typed values.
        *
        * Uses a deserialization mechanism to extract typed objects from
        * the underlying byte array.
        *
        * @tparam Ts... The types to deserialize and extract from the envelope.
        * @return A tuple containing the deserialized values.
        *
        * @warning If `sizeof...(Ts) > size()`, the method will fallback to underlying 
        * `deserializer` error handling which might involve assertion errors.
        */
        template<typename... Ts>
        inline std::tuple<Ts...> unpack() const;
        
        /**
        * @brief Packs one or more typed values into the envelope's memory block.
        *
        * This method serializes the given values into a dynamically allocated
        * memory block owned by the envelope.
        *
        * @tparam Ts... The types of the values to serialize.
        * @param args The values to serialize into the envelope.
        *
        * @note Any existing data in the envelope is discarded and replaced
        *       with the new packed contents.
        */
        template<typename... Ts>
        inline void pack(Ts&&... args);
        
        /**
        * @brief Returns a pointer to the internal byte data.
        *
        * Provides read-only access to the internal memory block.
        *
        * @return A const pointer to the byte data held by the envelope, or nullptr if empty.
        */
        inline const std::byte* data() const noexcept;
        
        /**
        * @brief Returns the size of the used envelope's memory.
        *
        * @return The number of used bytes stored in the envelope.
        */
        inline std::size_t size() const noexcept;

        /**
        * @brief Returns the total capacity of the envelope's memory
        * 
        * @return The number of bytes that are available.
        */
        inline std::size_t capacity() const noexcept;

        /// Delete copy constructor because envelope is a unique-ownership type.
        envelope(const envelope&) = delete;
        
        /// Delete copy assignment operator because envelope is a unique-ownership type.
        envelope& operator=(const envelope&) = delete;
    private:
        std::unique_ptr<std::byte[], Deleter> _data{};
        std::size_t _size{0};    
        std::size_t _capacity{0};
    };

} // namespace etools::memory

#include "envelope.tpp"
#endif // ETOOLS_MEMORY_ENVELOPE_HPP_