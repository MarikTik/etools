// SPDX-License-Identifier: MIT
/**
* @file registry.tpp
*
* @brief implementation of registry.hpp methods.
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2025-07-29
*
* @copyright
* MIT License
* Copyright (c) 2025 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETASK_TOOLS_REGISTRY_TPP_
#define ETASK_TOOLS_REGISTRY_TPP_
#include "registry.hpp"
#include "../memory/slot.hpp"
#include <algorithm>

#define registry_template \
    template < \
        typename Base, \
        template<typename> typename Extractor, \
        typename... DerivedTypes, \
        typename... ConstructorArgs \
    >
#define registry_type \
    registry<  \
        Base, \
        Extractor, \
        meta::typelist<DerivedTypes...>, \
        meta::typelist<ConstructorArgs...> \
    >


namespace etools::facilities {
    registry_template
    inline registry_type &registry_type::instance() noexcept {
        static registry_type instance; // Static instance for singleton pattern
        return instance;
    }
 
    
    registry_template
    inline Base* registry_type::get(key_t key) noexcept {
        auto it = find(key);
        return it == end() ? nullptr : it.getter();
    }

    registry_template
    inline Base* registry_type::construct(key_t key, ConstructorArgs &&...args) noexcept
    {
        // required to write the full implementation without calling `find()` due to it
        // returing end() since the memory is not yet initialized. 
        auto it = std::lower_bound(_index_table.cbegin(), _index_table.cend(), key,
            [](const auto &entry, const auto &value){
                return entry.key < value;
            }
        );
 
        if (
            it == _index_table.cend() or
            it->key not_eq key
        ) return nullptr;

        return _routing_table[it->index].constructor(std::forward<ConstructorArgs>(args)...);
    }

    registry_template
    inline void registry_type::destroy(key_t key) noexcept {
        auto it = find(key);
        if (it != end()) it.destructor();
    }

    registry_template
    registry_type::registry() noexcept
        : _routing_table{make_routing_table()}, _index_table{make_index_table()} 
    {
        std::sort(_index_table.begin(), _index_table.end(), [](const auto& a, const auto& b) {
            return a.key < b.key;
        });
    }

    registry_template
    registry_type::~registry() noexcept{
        for (std::size_t i = 0; i < _index_table.size(); i++) {
            const auto& slot = _index_table[i]; 
            _routing_table[slot.index].destructor();
        }
    }

    registry_template
    inline typename registry_type::iterator registry_type::begin() noexcept {
        return iterator(_routing_table.cbegin(), _routing_table.cend());
    }

    registry_template
    inline typename registry_type::iterator registry_type::end() noexcept  {
        return iterator(_routing_table.cend(), _routing_table.cend());
    }

    registry_template
    inline typename registry_type::const_iterator registry_type::begin() const noexcept {
        return const_iterator(_routing_table.cbegin(), _routing_table.cend());
    }

    registry_template
    inline typename registry_type::const_iterator registry_type::end() const noexcept  {
        return const_iterator(_routing_table.cend(), _routing_table.cend());
    }
    
    registry_template
    inline typename registry_type::const_iterator registry_type::cbegin() const noexcept {
        return const_iterator(_routing_table.cbegin(), _routing_table.cend());
    }

    registry_template
    inline typename registry_type::const_iterator registry_type::cend() const noexcept  {
        return const_iterator(_routing_table.cend(), _routing_table.cend());
    }

    registry_template
    inline typename registry_type::const_iterator registry_type::find(typename registry_type::key_t key) const noexcept{
        auto it = std::lower_bound(_index_table.cbegin(), _index_table.cend(), key,
            [](const auto &entry, const auto &value){
                return entry.key < value;
            }
        );
        if (
            it == _index_table.cend() or
            it->key not_eq key or 
            not _routing_table[it->index].getter()
        ) return this->cend();
        
        return const_iterator(_routing_table.cbegin() + it->index, _routing_table.cend());
    }

    registry_template
    inline typename registry_type::iterator registry_type::find(typename registry_type::key_t key) noexcept{
        auto it = std::lower_bound(_index_table.cbegin(), _index_table.cend(), key,
            [](const auto &entry, const auto &value){
                return entry.key < value;
            }
        );
        if (
            it == _index_table.cend() or
            it->key not_eq key or
            not _routing_table[it->index].getter() // check that the object is alive.

        ) return end();

        return iterator(_routing_table.cbegin() + it->index, _routing_table.cend());
    }

    registry_template
    constexpr typename registry_type::routing_table_t registry_type::make_routing_table() const {
        return {{
            {
                []() noexcept -> Base* { 
                    return memory::slot<DerivedTypes>::instance().get(); 
                },
                [](ConstructorArgs&&... args) noexcept -> Base* { 
                    return memory::slot<DerivedTypes>::instance().construct(std::forward<ConstructorArgs>(args)...); 
                },
                []() noexcept -> void { 
                    memory::slot<DerivedTypes>::instance().destroy(); 
                }
            }...
        }};
    }

    registry_template
    template <std::size_t... Is>
    constexpr typename registry_type::index_table_t registry_type::make_index_table_impl(std::index_sequence<Is...>) const {
        return {{
            mapping{Extractor<DerivedTypes>::value, static_cast<index_t>(Is)}...
        }};
    }
    
    registry_template
    constexpr typename registry_type::index_table_t registry_type::make_index_table() const{
        return make_index_table_impl(std::index_sequence_for<DerivedTypes...>{});
    }


    registry_template
    template <typename PtrType>
    inline registry_type::template registry_iterator<PtrType>::registry_iterator(
        typename registry_type::template registry_iterator<PtrType>::route_iter_t it,
        typename registry_type::template registry_iterator<PtrType>::route_iter_t end
    ) : _it(it), _end(end)
    {
        this->advance_if_null();
    }

    registry_template
    template <typename PtrType>
    inline auto registry_type::registry_iterator<PtrType>::operator*() const -> reference
    {
        return static_cast<PtrType>(_it->getter());
    }

    registry_template
    template <typename PtrType>
    inline auto registry_type::registry_iterator<PtrType>::operator->() const -> pointer
    {
        return static_cast<PtrType>(_it->getter());
    }

    registry_template
    template <typename PtrType>
    inline auto registry_type::registry_iterator<PtrType>::operator++() -> registry_iterator&
    {
        ++_it;
        advance_if_null();
        return *this;
    }


    registry_template
    template <typename PtrType>
    inline auto registry_type::registry_iterator<PtrType>::operator++(int) -> registry_iterator
    {
        auto tmp = *this;
        ++(*this);
        return tmp;
    }

    registry_template
    template <typename PtrType>
    inline void registry_type::registry_iterator<PtrType>::advance_if_null() {
        while (_it != _end && _it->getter() == nullptr) {
            ++_it;
        }
    }

    registry_template
    template <typename PtrType>
    inline auto registry_type::registry_iterator<PtrType>::getter() const -> reference
    {
        return static_cast<PtrType>(_it->getter());
    }

    registry_template
    template <typename PtrType>
    inline void registry_type::registry_iterator<PtrType>::destructor() const {
        _it->destructor();
    }
} // namespace etools::facilities

#endif // ETASK_TOOLS_REGISTRY_TPP_