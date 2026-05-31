// SPDX-License-Identifier: MIT
/**
* @file envelope_view.tpp
*
* @brief Implements `etools::memory::envelope_view` methods.
*
* This file provides template-based implementation details of the `envelope_view` class,
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
#ifndef ETOOLS_MEMORY_ENVELOPE_VIEW_TPP_
#define ETOOLS_MEMORY_ENVELOPE_VIEW_TPP_
#include "envelope_view.hpp"
#include <eser/binary/deserializer.hpp>
namespace etools::memory {
    inline envelope_view::envelope_view(const std::byte *data, std::size_t size) noexcept
        : _data{data}, _size{size}
    {
    }

    inline const std::byte* envelope_view::data() const noexcept {
        return _data;
    }

    inline std::size_t envelope_view::size() const noexcept {
        return _size;
    }

    template<typename... Ts>
    inline std::tuple<Ts...> envelope_view::unpack() const {
        return eser::binary::deserialize(_data, _size).template to<Ts...>();
    }

} // namespace etools::memory

#endif // ETOOLS_MEMORY_ENVELOPE_VIEW_TPP_