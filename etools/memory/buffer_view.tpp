// SPDX-License-Identifier: MIT
/**
* @file buffer_view.tpp
*
* @brief Implements `etools::memory::buffer_view` methods.
*
* This file provides template-based implementation details of the `buffer_view` class,
* including deserialization routines.
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
* @date 2025-07-20
*
* @copyright
* MIT License
* Copyright (c) 2025 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_MEMORY_BUFFER_VIEW_TPP_
#define ETOOLS_MEMORY_BUFFER_VIEW_TPP_
#include "buffer_view.hpp"
#include <eser/flat/deserializer.hpp>
namespace etools::memory {
    inline buffer_view::buffer_view(const std::byte *data, std::size_t size) noexcept
        : _data{data}, _size{size}
    {
    }

    inline const std::byte* buffer_view::data() const noexcept {
        return _data;
    }

    inline std::size_t buffer_view::size() const noexcept {
        return _size;
    }

    template<typename... Ts>
    inline std::optional<std::tuple<Ts...>> buffer_view::unpack() const {
        return eser::flat::deserialize(data(), size()).template to<std::tuple<Ts...>>();
    }

} // namespace etools::memory

#endif // ETOOLS_MEMORY_BUFFER_VIEW_TPP_