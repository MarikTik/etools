// SPDX-License-Identifier: MIT
/**
* @file slot.tpp
*
* @brief implementation of slot.hpp methods.
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2025-07-29
*
* @copyright
* MIT License
* Copyright (c) 2025 Mark Tikhonov
* See the accompanying LICENSE file for details.
* @par Changelog
* - 2025-08-10
*      - added use of `std::launder` for compile optimization
*        safety guarantee on different type memory access.
*      - Added separate `std::launder` alias in case it is not defined on the platform. 
*
*/

#ifndef ETOOLS_MEMORY_SLOT_TPP_
#define ETOOLS_MEMORY_SLOT_TPP_
#include "slot.hpp"
#include <cassert>
#include <new>

// Make sure std::launder is defined.
#if __cplusplus < 201703L || !defined(__cpp_lib_launder)
namespace std {
    template<class T> constexpr T* launder(T* p) noexcept { return p; }
}
#endif

namespace etools::memory {

    template <typename T>
    inline slot<T> &slot<T>::instance() noexcept
    {
        static slot inst;
        return inst;
    }

    template <typename T>
    template <typename... Args>
    inline T *slot<T>::construct(Args &&...args) noexcept(std::is_nothrow_constructible_v<T, Args &&...>)
    {
        static_assert(std::is_constructible_v<T, Args&&...>, "T must be constructible with the forwarded arguments.");
        assert(not _constructed && "Slot already constructed, cannot construct again.");
        return emplace(std::forward<Args>(args)...);
    }

    template <typename T>
    template <typename... Args>
    inline T *slot<T>::emplace(Args &&...args) noexcept(std::is_nothrow_constructible_v<T, Args &&...>){
        static_assert(std::is_constructible_v<T, Args&&...>, "T must be constructible with the forwarded arguments.");
        if (_constructed) destroy();
        _constructed = true;
        return new (&_mem) T(std::forward<Args>(args)...);
    }

    template <typename T>
    inline void slot<T>::destroy() noexcept(std::is_nothrow_destructible_v<T>) {
        if (not _constructed) return; // No object to destroy
        auto *p = std::launder(reinterpret_cast<T*>(&_mem));
        p->~T();
        _constructed = false;
    }

    template <typename T>
    inline T *slot<T>::get() noexcept  {
        if (not _constructed) return nullptr;
        return std::launder(reinterpret_cast<T*>(&_mem));
    }
    
    template <typename T>
    inline const T *slot<T>::get() const noexcept {
        if (not _constructed) return nullptr;
        return std::launder(reinterpret_cast<const T*>(&_mem));
    }
} // namespace etools::memory
#endif // ETOOLS_MEMORY_SLOT_TPP_