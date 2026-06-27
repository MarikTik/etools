// SPDX-License-Identifier: MIT
/**
* @file buffer.tpp
*
* @brief Definition of buffer.hpp methods.
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2025-07-03
*
* @copyright
* MIT License
* Copyright (c) 2025 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_MEMORY_BUFFER_TPP_
#define ETOOLS_MEMORY_BUFFER_TPP_
#include "buffer.hpp"
#include <eser/flat/serializer.hpp>
#include <eser/flat/deserializer.hpp>
#include <cassert>

namespace etools::memory{
    template<typename Deleter>
    buffer<Deleter>::buffer(std::unique_ptr<std::byte[], Deleter> data, std::size_t capacity)
        :
        _data{std::move(data)},
        _size{0},
        _capacity{capacity}
    {
    }

    template<typename Deleter>
    buffer<Deleter>::buffer(std::unique_ptr<std::byte[], Deleter> data, std::size_t capacity, std::size_t size)
        :
        _data{std::move(data)},
        _size{size},
        _capacity{capacity}
    {
        assert(size <= capacity && "buffer: size cannot exceed capacity");
    }

    template<typename Deleter>
    buffer<Deleter>::buffer(buffer &&other) noexcept
        : _data{std::move(other._data)}, _size{other._size}, _capacity{other._capacity}
    {
        other._size = other._capacity = 0;
    }

    template<typename Deleter>
    buffer<Deleter> &buffer<Deleter>::operator=(buffer &&other) noexcept
    {
        if (this != &other) {
            _data = std::move(other._data);
            _size = other._size;
            _capacity = other._capacity;
            other._size = other._capacity = 0;
        }
        return *this;
    }

    template<typename Deleter>
    inline const std::byte* buffer<Deleter>::data() const noexcept {
        return _data.get();
    }

    template<typename Deleter>
    inline std::size_t buffer<Deleter>::size() const noexcept {
        return _size;
    }

    template<typename Deleter>
    inline std::size_t buffer<Deleter>::capacity() const noexcept {
        return _capacity;
    }

    template<typename Deleter>
    template<typename... Ts>
    inline std::optional<std::tuple<Ts...>> buffer<Deleter>::unpack() const {
        return eser::flat::deserialize(data(), size()).template to<std::tuple<Ts...>>();
    }

    template<typename Deleter>
    template<typename... Ts>
    inline std::size_t buffer<Deleter>::pack(Ts&&... args) {
        assert(_data && "buffer::pack(): called on a moved-from or null-data buffer");
        _size = eser::flat::serialize(std::forward<Ts>(args)...).to(_data.get(), capacity());
        return _size;
    }

} // namespace etools::memory

#endif // ETOOLS_MEMORY_BUFFER_TPP_