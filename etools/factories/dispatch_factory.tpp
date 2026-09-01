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
#include <cassert>
#include <cstddef>
namespace etools::factories {

    /**
    * @namespace etools::factories::detail
    * @brief Internal helpers for `dispatch_factory`; not part of the public API.
    */
    namespace detail {

        /**
        * @brief The factory's contract checks, as plain functions.
        *
        * Each of these is a one-line `assert` that could equally well be written
        * inline at its call site. It is not, for one reason: `assert` embeds
        * `__PRETTY_FUNCTION__`, and inside `dispatch_factory` that expands to the
        * class's full template-id - which names *every registered type* with its
        * complete parameter list.
        *
        * The cost of that is quadratic. Each per-slot instantiation carries its own
        * copy of a message that grows with the number of registered types, so N
        * types produce N messages of O(N) length. Measured on an ESP32 with 100
        * tasks, a single one of those strings reached 1,183,522 bytes and
        * `.flash.rodata` grew to 1,262,324 bytes - enough on its own to push a
        * project that links comfortably in release over the 1,310,720-byte flash
        * budget. The same build with `NDEBUG` fits in 249,485 bytes.
        *
        * These functions are not templates, so their `__PRETTY_FUNCTION__` is a
        * fixed sentence and the messages stop scaling. The checks themselves are
        * unchanged, and still compile away entirely under `NDEBUG`.
        *
        * @note Taking plain integers rather than the factory's own `key_t` /
        *       `slot_index_t` is deliberate: those are dependent types, and naming
        *       one in a signature would put the template-id back into the message.
        */
        inline void assert_slots_all_empty(bool all_empty) noexcept
        {
            assert(all_empty
                   && "dispatch_factory destroyed while it still owns live objects: "
                      "every handle must be dropped before the factory goes out of scope");
            (void)all_empty;
        }

        /// @brief The key resolved to a registered type. See @ref assert_slots_all_empty.
        inline void assert_key_is_registered(std::size_t index, std::size_t type_count) noexcept
        {
            assert(index < type_count
                   && "dispatch_factory::reset called with a key that is not registered; "
                      "the key must come from a successful emplace");
            (void)index;
            (void)type_count;
        }

        /// @brief The slot lies inside its type's array. See @ref assert_slots_all_empty.
        inline void assert_slot_in_range(std::size_t slot_index, std::size_t slot_count) noexcept
        {
            assert(slot_index < slot_count
                   && "dispatch_factory::reset called with an out-of-range slot; "
                      "the slot must come from a successful emplace");
            (void)slot_index;
            (void)slot_count;
        }

    } // namespace detail

    template <typename Base, template<typename> typename Extractor, typename... Regs>
    void dispatch_factory<Base, Extractor, Regs...>::cell_deleter::operator()(Base*) const noexcept
    {
        factory->reset(key, slot_index); // unique_ptr only calls this when ptr != nullptr, so factory is always valid here
    }

    template <typename Base, template<typename> typename Extractor, typename... Regs>
    dispatch_factory<Base, Extractor, Regs...>::~dispatch_factory() noexcept
    {
        bool all_empty = true;
        meta::for_each(_slots, [&all_empty](const auto& arr) noexcept {
            all_empty = all_empty and std::all_of(arr.begin(), arr.end(),
                [](const auto& opt) noexcept { return !opt.has_value(); });
        });
        detail::assert_slots_all_empty(all_empty);
    }

    template <typename Base, template<typename> typename Extractor, typename... Regs>
    template <typename... Args>
    auto dispatch_factory<Base, Extractor, Regs...>::emplace(key_t key, Args&&... args)
        noexcept(nothrow_emplace_v<Args...>)
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
    void dispatch_factory<Base, Extractor, Regs...>::reset(key_t key, slot_index_t slot_index) noexcept
    {
        constexpr const auto& table = mpht();
        std::size_t index = table(key);
        // Key and slot originate from a successful emplace - both must be valid.
        detail::assert_key_is_registered(index, type_count);
        index_dispatch(index, std::index_sequence_for<Regs...>{},
            [this, slot_index](auto I) noexcept {
                auto& arr = meta::get<I()>(_slots);
                detail::assert_slot_in_range(slot_index, arr.size());
                arr[slot_index].reset();
            });
    }

    template <typename Base, template<typename> typename Extractor, typename... Regs>
    template <std::size_t... Is, typename Fn>
    void dispatch_factory<Base, Extractor, Regs...>::index_dispatch(
        std::size_t index, std::index_sequence<Is...>, Fn&& fn)
        noexcept(noexcept(fn(std::integral_constant<std::size_t, 0>{})))
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
        noexcept(nothrow_emplace_v<Args...>)
    {
        // This function is the compilation bottleneck for a large registry: one
        // call site instantiates a body per registered type, and measured at 260
        // types a single `emplace<buffer_view>` took the translation unit from
        // 3.94 s to 21.24 s.
        //
        // It is *not* `nth_t`, which an earlier note here blamed: `nth_t` resolves
        // through `__type_pack_element` (see meta/traits.hpp), so it is O(1) per
        // lookup, and instantiating it for all 260 indices measures 0.23 s.
        Base* result = nullptr;
        index_dispatch(index, std::index_sequence_for<Regs...>{},
            [this, &result, &out_slot, &args...](auto I)
                noexcept(nothrow_emplace_v<Args...>)
            {
                using target_t = typename reg_t<meta::nth_t<I(), Regs...>>::type;
                if constexpr (std::is_constructible_v<target_t, Args&&...>) {
                    auto& arr = meta::get<I()>(_slots);
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
