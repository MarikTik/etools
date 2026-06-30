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
namespace etools::factories {

    template <typename Base, template<typename> typename Extractor, typename... Regs>
    inline void dispatch_factory<Base, Extractor, Regs...>::cell_deleter::operator()(Base*) const noexcept
    {
        if (factory) factory->reset(key, slot_index);
    }

    template <typename Base, template<typename> typename Extractor, typename... Regs>
    template <typename... Args>
    inline auto dispatch_factory<Base, Extractor, Regs...>::emplace(key_t key, Args&&... args)
        noexcept(((not std::is_constructible_v<typename reg_t<Regs>::type, Args&&...>
                   or std::is_nothrow_constructible_v<typename reg_t<Regs>::type, Args&&...>) and ...))
        -> handle_t
    {
        constexpr const auto& table = mpht();
        std::size_t index = table(key);
        if (index >= type_count) return handle_t{};
        slot_index_t slot{};
        Base* b = dispatch(index, slot, std::forward<Args>(args)...);
        if (not b) return handle_t{};
        return handle_t{b, cell_deleter{this, key, slot}};
    }

    template <typename Base, template<typename> typename Extractor, typename... Regs>
    inline void dispatch_factory<Base, Extractor, Regs...>::reset(key_t key, slot_index_t slot_index) noexcept
    {
        constexpr const auto& table = mpht();
        std::size_t index = table(key);
        if (index >= type_count) return;
        index_dispatch(index, std::index_sequence_for<Regs...>{},
            [this, slot_index](auto I) {
                auto& arr = std::get<I()>(_slots);
                if (slot_index < arr.size()) arr[slot_index].reset();
            });
    }

    template <typename Base, template<typename> typename Extractor, typename... Regs>
    template <std::size_t... Is, typename Fn>
    inline void dispatch_factory<Base, Extractor, Regs...>::index_dispatch(std::size_t index, std::index_sequence<Is...>, Fn&& fn) noexcept
    {
        ((index == Is ? (fn(std::integral_constant<std::size_t, Is>{}), true) : false) || ...);
    }

    template <typename Base, template <typename> typename Extractor, typename... Regs>
    constexpr const auto& dispatch_factory<Base, Extractor, Regs...>::mpht() noexcept
    {
        using table_t = etools::hashing::optimal_mph<key_t>;
        return table_t::template instance<
            static_cast<key_t>(Extractor<typename reg_t<Regs>::type>::value)...
        >();
    }

    template <typename Base, template<typename> typename Extractor, typename... Regs>
    template <typename... Args>
    Base* dispatch_factory<Base, Extractor, Regs...>::dispatch(std::size_t index, slot_index_t& out_slot, Args&&... args)
    {
        // Compilation bottleneck for very large registries (>2000 types) due to nth_t.
        // For k constructor signatures and n types: O(k*n) compile time.
        // Future: replace nth_t with meta::pack_at_t to amortize to O(n+k).
        Base* result = nullptr;
        index_dispatch(index, std::index_sequence_for<Regs...>{},
            [this, &result, &out_slot, &args...](auto I) {
                using target_t = typename reg_t<meta::nth_t<I(), Regs...>>::type;
                if constexpr (std::is_constructible_v<target_t, Args&&...>) {
                    auto& arr = std::get<I()>(_slots);
                    for (std::size_t i = 0; i < arr.size(); ++i) {
                        if (!arr[i].has_value()) {
                            result   = &arr[i].emplace(std::forward<Args>(args)...);
                            out_slot = static_cast<slot_index_t>(i);
                            return;
                        }
                    }
                    // all N slots occupied -> result stays nullptr
                }
            });
        return result;
    }

} // namespace etools::factories
#endif //ETOOLS_FACTORIES_DISPATCH_FACTORY_TPP_
