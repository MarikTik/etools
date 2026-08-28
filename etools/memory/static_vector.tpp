// SPDX-License-Identifier: MIT
/**
* @file static_vector.tpp
*
* @brief Implementation of static_vector.hpp's api.
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2026-08-27
*
* @copyright
* MIT License
* Copyright (c) 2026 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_MEMORY_STATIC_VECTOR_TPP_
#define ETOOLS_MEMORY_STATIC_VECTOR_TPP_
#include "static_vector.hpp"

namespace etools::memory {

    template<typename T, std::size_t Capacity>
    static_vector<T, Capacity>::~static_vector() noexcept
    {
        clear();
    }

    template<typename T, std::size_t Capacity>
    template<typename... Args>
    T* static_vector<T, Capacity>::try_emplace_back(Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args&&...>)
    {
        if (_size == Capacity)
            return nullptr;

        // _size is incremented only after the constructor returns, so a throwing
        // T leaves the container exactly as it was.
        T* const cell = ::new (static_cast<void*>(_storage + _size * sizeof(T)))
            T(std::forward<Args>(args)...);
        ++_size;
        return cell;
    }

    template<typename T, std::size_t Capacity>
    void static_vector<T, Capacity>::pop_back() noexcept
    {
        assert(_size > 0 and "pop_back() on an empty static_vector");
        --_size;
        slot_at(_size)->~T();
    }

    template<typename T, std::size_t Capacity>
    void static_vector<T, Capacity>::swap_erase(size_type index) noexcept
    {
        assert(index < _size and "swap_erase() index out of range");

        const size_type last = _size - 1;

        // Erasing the last element is just a pop: there is nothing to move into
        // its place, and move-assigning an object onto itself is not required to
        // leave it in any particular state.
        if (index != last)
            *slot_at(index) = std::move(*slot_at(last));

        --_size;
        slot_at(last)->~T();
    }

    template<typename T, std::size_t Capacity>
    void static_vector<T, Capacity>::clear() noexcept
    {
        // Reverse order, mirroring the destruction order of an array.
        while (_size > 0) {
            --_size;
            slot_at(_size)->~T();
        }
    }

    template<typename T, std::size_t Capacity>
    typename static_vector<T, Capacity>::size_type
    static_vector<T, Capacity>::size() const noexcept
    {
        return _size;
    }

    template<typename T, std::size_t Capacity>
    bool static_vector<T, Capacity>::empty() const noexcept
    {
        return _size == 0;
    }

    template<typename T, std::size_t Capacity>
    bool static_vector<T, Capacity>::full() const noexcept
    {
        return _size == Capacity;
    }

    template<typename T, std::size_t Capacity>
    typename static_vector<T, Capacity>::reference
    static_vector<T, Capacity>::operator[](size_type index) noexcept
    {
        assert(index < _size and "static_vector index out of range");
        return *slot_at(index);
    }

    template<typename T, std::size_t Capacity>
    typename static_vector<T, Capacity>::const_reference
    static_vector<T, Capacity>::operator[](size_type index) const noexcept
    {
        assert(index < _size and "static_vector index out of range");
        return *slot_at(index);
    }

    template<typename T, std::size_t Capacity>
    typename static_vector<T, Capacity>::iterator
    static_vector<T, Capacity>::begin() noexcept
    {
        return slot_at(0);
    }

    template<typename T, std::size_t Capacity>
    typename static_vector<T, Capacity>::const_iterator
    static_vector<T, Capacity>::begin() const noexcept
    {
        return slot_at(0);
    }

    template<typename T, std::size_t Capacity>
    typename static_vector<T, Capacity>::iterator
    static_vector<T, Capacity>::end() noexcept
    {
        return slot_at(_size);
    }

    template<typename T, std::size_t Capacity>
    typename static_vector<T, Capacity>::const_iterator
    static_vector<T, Capacity>::end() const noexcept
    {
        return slot_at(_size);
    }

    template<typename T, std::size_t Capacity>
    typename static_vector<T, Capacity>::const_iterator
    static_vector<T, Capacity>::cbegin() const noexcept
    {
        return begin();
    }

    template<typename T, std::size_t Capacity>
    typename static_vector<T, Capacity>::const_iterator
    static_vector<T, Capacity>::cend() const noexcept
    {
        return end();
    }

    template<typename T, std::size_t Capacity>
    typename static_vector<T, Capacity>::reference
    static_vector<T, Capacity>::front() noexcept
    {
        assert(_size > 0 and "front() on an empty static_vector");
        return *slot_at(0);
    }

    template<typename T, std::size_t Capacity>
    typename static_vector<T, Capacity>::const_reference
    static_vector<T, Capacity>::front() const noexcept
    {
        assert(_size > 0 and "front() on an empty static_vector");
        return *slot_at(0);
    }

    template<typename T, std::size_t Capacity>
    typename static_vector<T, Capacity>::reference
    static_vector<T, Capacity>::back() noexcept
    {
        assert(_size > 0 and "back() on an empty static_vector");
        return *slot_at(_size - 1);
    }

    template<typename T, std::size_t Capacity>
    typename static_vector<T, Capacity>::const_reference
    static_vector<T, Capacity>::back() const noexcept
    {
        assert(_size > 0 and "back() on an empty static_vector");
        return *slot_at(_size - 1);
    }

    template<typename T, std::size_t Capacity>
    T* static_vector<T, Capacity>::slot_at(size_type index) noexcept
    {
        return std::launder(reinterpret_cast<T*>(_storage + index * sizeof(T)));
    }

    template<typename T, std::size_t Capacity>
    const T* static_vector<T, Capacity>::slot_at(size_type index) const noexcept
    {
        return std::launder(reinterpret_cast<const T*>(_storage + index * sizeof(T)));
    }

} // namespace etools::memory

#endif // ETOOLS_MEMORY_STATIC_VECTOR_TPP_
