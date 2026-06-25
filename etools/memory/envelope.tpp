// SPDX-License-Identifier: MIT
/**
* @file envelope.tpp
*
* @brief Definition of envelope.hpp methods.
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
#ifndef ETOOLS_MEMORY_ENVELOPE_TPP_
#define ETOOLS_MEMORY_ENVELOPE_TPP_
#include "envelope.hpp"
#include <eser/flat/serializer.hpp>
#include <eser/flat/deserializer.hpp>
#include <cassert>

namespace etools::memory{
    template<typename Deleter>
    envelope<Deleter>::envelope(std::unique_ptr<std::byte[], Deleter> data, std::size_t capacity)
        :
        _data{std::move(data)},
        _size{0},
        _capacity{capacity}
    {
    }

    template<typename Deleter>
    envelope<Deleter>::envelope(std::unique_ptr<std::byte[], Deleter> data, std::size_t capacity, std::size_t size)
        :
        _data{std::move(data)},
        _size{size},
        _capacity{capacity}
    {
        assert(size <= capacity && "envelope: size cannot exceed capacity");
    }

    template<typename Deleter>
    envelope<Deleter>::envelope(envelope &&other) noexcept
        : _data{std::move(other._data)}, _size{other._size}, _capacity{other._capacity}
    {
        other._size = other._capacity = 0;
    }

    template<typename Deleter>
    envelope<Deleter> &envelope<Deleter>::operator=(envelope &&other) noexcept
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
    inline const std::byte* envelope<Deleter>::data() const noexcept {
        return _data.get();
    }

    template<typename Deleter>
    inline std::size_t envelope<Deleter>::size() const noexcept {
        return _size;
    }

    template<typename Deleter>
    inline std::size_t envelope<Deleter>::capacity() const noexcept {
        return _capacity;
    }

    template<typename Deleter>
    template<typename... Ts>
    inline std::optional<std::tuple<Ts...>> envelope<Deleter>::unpack() const {
        return eser::flat::deserialize(data(), size()).template to<std::tuple<Ts...>>();
    }

    template<typename Deleter>
    template<typename... Ts>
    inline void envelope<Deleter>::pack(Ts&&... args) {
        assert(_data && "envelope::pack(): called on a moved-from or null-data envelope");
        _size = eser::flat::serialize(std::forward<Ts>(args)...).to(_data.get(), capacity());
    }

} // namespace etools::memory

#endif // ETOOLS_MEMORY_ENVELOPE_TPP_