// SPDX-License-Identifier: MIT
/**
* @file dispatch_factory.tpp
*
* @brief Definition of dispatch_factory.hpp methods.
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
#ifndef ETOOLS_FACTORIES_DISPATCH_FACTORY_TPP_
#define ETOOLS_FACTORIES_DISPATCH_FACTORY_TPP_
#include "dispatch_factory.hpp"
#include <tuple>
namespace etools::factories::details{
    template <typename Base, template<typename> typename Extractor, typename... DerivedTypes>
    inline void dispatch_factory<Base, Extractor, DerivedTypes...>::cell_deleter::operator()(Base*) const noexcept
    {
        if (factory) factory->reset(key);
    }

    template <typename Base, template<typename> typename Extractor, typename... DerivedTypes>
    template <typename... Args>
    inline auto dispatch_factory<Base, Extractor, DerivedTypes...>::emplace(key_t key, Args &&...args)
        noexcept(((not std::is_constructible_v<DerivedTypes, Args&&...>
                   or std::is_nothrow_constructible_v<DerivedTypes, Args&&...>) and ...))
        -> handle
    {
        // `table` is the constexpr MPH singleton; `table(key)` is an O(1) runtime lookup.
        constexpr const auto& table = mpht();
        std::size_t index = table(key);
        if (index >= capacity) return handle{};               // unknown key -> empty handle
        Base* b = dispatch(index, std::forward<Args>(args)...);
        if (not b) return handle{};                           // no type constructible from Args
        return handle{b, cell_deleter{this, key}};
    }

    template <typename Base, template<typename> typename Extractor, typename... DerivedTypes>
    template <std::size_t... Is, typename Fn>
    inline void dispatch_factory<Base, Extractor, DerivedTypes...>::index_dispatch(std::size_t index, std::index_sequence<Is...>, Fn&& fn) noexcept
    {
        ((index == Is ? (fn(std::integral_constant<std::size_t, Is>{}), true) : false) || ...);
    }

    template <typename Base, template<typename> typename Extractor, typename... DerivedTypes>
    inline void dispatch_factory<Base, Extractor, DerivedTypes...>::reset(key_t key) noexcept
    {
        constexpr const auto& table = mpht();
        std::size_t index = table(key);
        if (index >= capacity) return;
        index_dispatch(index, std::index_sequence_for<DerivedTypes...>{},
            [this](auto I) { std::get<I()>(_slots).reset(); });
    }

    template <typename Base, template <typename> typename Extractor, typename... DerivedTypes>
    constexpr const auto &dispatch_factory<Base, Extractor, DerivedTypes...>::mpht() noexcept
    {
        using table_t = etools::hashing::optimal_mph<key_t>;
        return table_t::template instance<
        static_cast<key_t>(Extractor<DerivedTypes>::value)...
        >();
    }

    template <typename Base, template<typename> typename Extractor, typename... DerivedTypes>
    template <typename... Args>
    Base* dispatch_factory<Base, Extractor, DerivedTypes...>::dispatch(std::size_t index, Args&&... args)
    {
        // Compilation bottleneck for very large registries (>2000 types) due to nth_t.
        // For k constructor signatures and n types: O(k*n) compile time.
        // Future: replace nth_t with meta::pack_at_t to amortize to O(n+k).
        Base* result = nullptr;
        index_dispatch(index, std::index_sequence_for<DerivedTypes...>{},
            [this, &result, &args...](auto I) {
                using target_t = meta::nth_t<I(), DerivedTypes...>;
                if constexpr (std::is_constructible_v<target_t, Args&&...>)
                    result = &std::get<I()>(_slots).emplace(std::forward<Args>(args)...);
            });
        return result;
    }


} // namespace etools::factories::details
#endif //ETOOLS_FACTORIES_DISPATCH_FACTORY_TPP_
