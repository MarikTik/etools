// SPDX-License-Identifier: MIT
/**
* @file static_factory.tpp
*
* @brief Definition of static_construction_factory.hpp methods.
* 
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2025-08-16
*
* @copyright
* MIT License
* Copyright (c) 2025 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_FACTORIES_STATIC_FACTORY_TPP_
#define ETOOLS_FACTORIES_STATIC_FACTORY_TPP_
#include "static_factory.hpp"
namespace etools::factories::details{
    template <typename Base, template<typename> typename Extractor, typename... DerivedTypes>
    template <typename... Args>
    inline Base* static_factory<Base, Extractor, DerivedTypes...>::emplace(key_t key, Args &&...args) noexcept
    {
        constexpr const auto& table = mpht();
        std::size_t index = table(key);           // note that `table()` is resolved to a constexpr singleton here.
        if (index >= capacity) return nullptr; // noting that `capacity` == `table.not_found()`
        Base* b = dispatch(index, std::forward<Args>(args)...);
        return b; 
    }
    
    
    template <typename Base, template<typename> typename Extractor,typename... DerivedTypes>
    constexpr const auto &static_factory<Base, Extractor, DerivedTypes...>::mpht() noexcept {
        using table_t = etools::hashing::optimal_mph<key_t>;         
        return table_t::template instance<
        static_cast<key_t>(Extractor<DerivedTypes>::value)...
        >();
    }
    
    template<typename Base, template<typename> typename Extractor, typename... DerivedTypes>
    template<typename T, typename... Args>
    inline bool static_factory<Base, Extractor, DerivedTypes...>::
    try_emplace_if_constructible(Base*& out, Args&&... args) noexcept {
        // Preserve the value category of each argument expression in the probe:
        //   - lvalue arg  -> decltype(std::forward<Arg>(arg)) is T&   (cannot bind to T&&)
        //   - rvalue arg  -> decltype(std::forward<Arg>(arg)) is T&&  (can bind to T&&)
        if constexpr (std::is_constructible_v<T, Args&&...>) {
            
            out = etools::memory::slot<T>::instance().emplace(std::forward<Args>(args)...);
            return true;
        } else {
            (void)out;
            return false; // arguments don't match this T's constructor
        }
    }
    
    template <typename Base, template<typename> typename Extractor, typename... DerivedTypes>
    template <typename... Args, std::size_t... Is>
    Base* static_factory<Base, Extractor, DerivedTypes...>
    ::dispatch_fold(std::size_t index, std::index_sequence<Is...>, Args&&... args) noexcept
    {
        Base* result = nullptr;
        
        // Only the matching branch (index == Is) performs the emplace; others are cheap tests.
        // The boolean fold keeps short-circuit behavior.
        ((index == Is
            ? try_emplace_if_constructible<meta::nth_t<Is, DerivedTypes...>>(result,
                std::forward<Args>(args)...)
                : false) || ...);
                
                return result;
    }
            
    template <typename Base, template<typename> typename Extractor, typename... DerivedTypes>
    template <typename... Args>
    Base *static_factory<Base, Extractor, DerivedTypes...>::dispatch(std::size_t index, Args&&... args) noexcept{
        
        // Compilation bottlenck in `dispatch_impl` for very large amount of keys ( >2000 )
        // due to call to nth_t. For k different constructors and n keys, the expected
        // compile time complexity is O(k*n).
        
        return dispatch_fold(index, std::index_sequence_for<DerivedTypes...>{},
            std::forward<Args>(args)...);
    }
        
        
} // namespace etools::facilities
#endif //ETOOLS_FACTORIES_STATIC_FACTORY_TPP_