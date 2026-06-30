// SPDX-License-Identifier: MIT
/**
* @file dispatch_factory.hpp
*
* @ingroup etools_factories etools::factories
*
* @details
* This header provides a zero-allocation, header-only facility to construct one of
* several `DerivedTypes...` by a `key` extracted from each derived type via an `Extractor<T>`.
* The mapping from keys to dense indices is resolved entirely at compile time through
* `etools::hashing::optimal_mph`, yielding constant-time key lookup with no dynamic memory.
*
* ## Core Concepts
* - Each `Derived` type must expose a unique, constant expression key
*   (`static constexpr key_t key`).
* - An `Extractor<T>` metafunction supplies the key for compile-time hashing.
* - At runtime, `emplace(key, args...)` dispatches to the correct derived type slot.
* - Constructor arguments are *perfect-forwarded* and checked at **compile time**
*   against each candidate `Derived`.
*   This means: if `D` is not constructible from `(Args&&...)`, its branch
*   compiles down to a no-op and does not participate. Only types with matching
*   constructors can be instantiated for a given argument list.
* - Each type may have `N > 1` concurrent instances.  Pass
*   `etools::factories::utils::capacity<T, N>` to declare per-type slot count.
*   Bare types are accepted and treated as `capacity<T, 1>`.
*
* ## Compile-time Considerations
* - Dispatch is implemented via a fold expression across all registered types.
* - For very large registries (1,000+ types with many constructor variations),
*   compile times may become significant. On non-professional systems this can
*   impact developer experience. The generated code remains efficient at runtime.
* - **Compile-time:** Roughly O(N x K), where N is the number of registered types
*   and K is the number of distinct constructor argument signatures seen.
*
* ## Components
* - `etools::factories::dispatch_factory<Base, Extractor, Regs...>` - full implementation.
* - Typelist adapter: `dispatch_factory<Base, Extractor, meta::typelist<Ts...>>` unwraps
*   the list and delegates to the primary.
*
* ## Example
* @code
* #include <etools/factories/dispatch_factory.hpp>
* #include <etools/factories/utils/capacity.hpp>
* #include <etools/meta/typelist.hpp>
*
* struct Base {
*   virtual ~Base() = default;
*   virtual const char* tag() const noexcept = 0;
* };
*
* struct A : Base {
*   static constexpr std::uint16_t key = 2;
*   int x{};
*   A() = default;
*   const char* tag() const noexcept override { return "A"; }
* };
*
* struct B : Base {
*   static constexpr std::uint16_t key = 5;
*   int v{};
*   explicit B(int vv) : v(vv) {}
*   const char* tag() const noexcept override { return "B"; }
* };
*
* template<class T>
* struct key_extractor {
*   static constexpr auto value = T::key;
* };
*
* using namespace etools::factories::utils;
*
* // A gets one slot; B gets three concurrent slots. Bare types default to capacity<T,1>.
* using factory_t = etools::factories::dispatch_factory<
*   Base,
*   key_extractor,
*   capacity<A, 1>,
*   capacity<B, 3>
* >;
*
* int main() {
*   factory_t factory;
*
*   auto ha  = factory.emplace(A::key);          // slot 0 of A
*   auto hb0 = factory.emplace(B::key, 10);      // slot 0 of B
*   auto hb1 = factory.emplace(B::key, 20);      // slot 1 of B
*   auto hb2 = factory.emplace(B::key, 30);      // slot 2 of B
*   auto hb3 = factory.emplace(B::key, 40);      // empty: all 3 B slots occupied
* }
* @endcode
*
* @author
* Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2025-08-16
*
* @copyright
* MIT License
* Copyright (c) 2025 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_FACTORIES_DISPATCH_FACTORY_HPP_
#define ETOOLS_FACTORIES_DISPATCH_FACTORY_HPP_
#include "utils/capacity.hpp"
#include "../meta/typelist.hpp"
#include "../meta/traits.hpp"
#include "../hashing/optimal_mph.hpp"
#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
namespace etools::factories {
    namespace details {
        /**
        * @brief Normalises a registration argument to `utils::capacity<T, N>`.
        *
        * A bare `T` becomes `capacity<T, 1>`; an already-wrapped `capacity<T, N>`
        * passes through unchanged. Used by the public facade to allow mixing bare
        * types and explicit `capacity<>` registrations in the same argument list.
        */
        template<typename T>
        struct as_capacity { using type = utils::capacity<T, 1>; };
        template<typename T, std::size_t N>
        struct as_capacity<utils::capacity<T, N>> { using type = utils::capacity<T, N>; };
        template<typename T>
        using as_capacity_t = typename as_capacity<T>::type;
    } // namespace details

    /**
    * @class dispatch_factory
    * @brief Zero-allocation compile-time registry for constructing derived types by key.
    *
    * @tparam Base       Polymorphic base type.
    * @tparam Extractor  `template<class T> struct Extractor { static constexpr auto value; };`
    * @tparam Regs...    Registration arguments: `utils::capacity<DerivedType, N>` or a bare
    *                    `DerivedType` (treated as `capacity<DerivedType, 1>`). May be mixed.
    *
    * The factory **owns** the storage for its derived objects: one
    * `std::array<std::optional<Derived>, N>` per registered type, held in a tuple.
    * `emplace()` places an object into the first free slot of the matching type array;
    * it returns an empty handle if all `N` slots are occupied. Objects are destroyed
    * when their handle is dropped or when the factory is destroyed (RAII).
    * There is no global/static storage and no heap allocation.
    *
    * @note Keys are extracted as `std::remove_cv_t<decltype(Extractor<T>::value)>`
    *       and must be pairwise-distinct. The lookup uses a minimal-overhead MPH.
    * @note The factory is a pinned type: copy and move are both deleted (like
    *       `std::mutex`). It owns live objects in-place, so relocating it is not
    *       supported; hold it by reference, as a `static`, or on the stack.
    * @note Not thread-safe: `emplace` mutates shared storage. Use one factory per thread,
    *       or synchronise externally.
    */
    template<typename Base, template<typename> typename Extractor, typename... Regs>
    class dispatch_factory {
        // Normalise every Reg to capacity<T,N> uniformly, so bare types and
        // capacity<> tags are handled identically throughout the class body.
        template<typename R>
        using reg = details::as_capacity_t<R>;

        /**
        * @typedef sample_t
        *
        * @brief The first derived type used to deduce key type.
        */
        using sample_t  = typename reg<meta::nth_t<0, Regs...>>::type;

        /** @typedef key_t
        *
        * @brief The type of the unique key for each derived type, deduced via Extractor metafunction.
        */
        using key_t  = std::remove_cv_t<decltype(Extractor<sample_t>::value)>;

        /**
        * @brief Number of distinct registered types (equals sizeof...(Regs)).
        */
        static constexpr std::size_t type_count = sizeof...(Regs);

        /**
        * @brief Maximum per-type slot count across all registrations.
        */
        static constexpr std::size_t max_count = std::max({reg<Regs>::count...});

        /**
        * @typedef slot_index_t
        *
        * @brief Smallest unsigned type that can represent any intra-array slot index.
        *
        * Derived from `max_count` so `cell_deleter` never carries a wasteful
        * `std::size_t` field when keys and counts are narrow.
        */
        using slot_index_t = meta::smallest_uint_t<max_count - 1>;

        static_assert((std::is_same_v<key_t, std::remove_cv_t<decltype(Extractor<typename reg<Regs>::type>::value)>> and ...), "all registered types must expose the same key type");
        static_assert(sizeof...(Regs) > 0, "register at least one type");
        static_assert(((reg<Regs>::count > 0) and ...), "capacity<T, N> requires N > 0");

        /**
        * @brief Custom deleter for the owning handle returned by `emplace`.
        *
        * Does **not** free memory (the object lives in the factory's array cell);
        * instead it calls the factory's private `reset(key, slot_index)`, which runs
        * the matching `std::optional`'s destructor in place. Zero-allocation RAII.
        * A default-constructed deleter (`factory == nullptr`) is a no-op, which is
        * what an empty (failed) handle carries.
        *
        * @warning The handle must not outlive the factory: the deleter dereferences `factory`.
        */
        struct cell_deleter {
            dispatch_factory* factory = nullptr;
            key_t key{};
            slot_index_t slot_index{};
            /**
            * @brief Called by `unique_ptr` when the handle is dropped or reset.
            *
            * Calls `factory->reset(key, slot_index)` to destroy the object in its
            * array cell in place. A null `factory` (empty handle) is a no-op.
            *
            * @param[in] p  Pointer to the `Base` subobject (unused - storage is in the cell).
            */
            void operator()(Base* p) const noexcept;
        };
    public:
        /**
        * @brief Owning handle to a constructed object: a `unique_ptr<Base>` whose deleter
        *        tears the object down in its factory array cell (no heap free).
        */
        using handle_t = std::unique_ptr<Base, cell_deleter>;
        /// @brief Constructs an empty factory; every slot starts unoccupied.
        dispatch_factory() = default;
        /// @brief Deleted copy constructor - the factory owns in-place storage.
        dispatch_factory(const dispatch_factory&) = delete;
        /// @brief Deleted copy assignment operator.
        dispatch_factory& operator=(const dispatch_factory&) = delete;
        /// @brief Deleted move constructor - pinned type; relocating live objects is unsupported.
        dispatch_factory(dispatch_factory&&) = delete;
        /// @brief Deleted move assignment operator.
        dispatch_factory& operator=(dispatch_factory&&) = delete;
        /**
        * @brief Construct an instance of the type associated with `key` in the first
        *        free slot of its owned array.
        *
        * @tparam Args Constructor argument types forwarded to the selected `Derived`.
        *
        * @param[in] key  The key corresponding to a registered derived type.
        * @param[in] args Constructor arguments forwarded to the selected `Derived`.
        *
        * @return An owning `handle_t` (`unique_ptr<Base, cell_deleter>`). The handle is
        *         **empty** (`get() == nullptr`) if:
        *         - `key` is not found in the registry, or
        *         - no registered type is constructible from `Args...`, or
        *         - all `N` slots for the matching type are already occupied.
        *         Dropping a non-empty handle (or calling `.reset()` on it) destroys
        *         the object in its cell and frees the slot for reuse.
        *
        * @post On success the object lives in this factory's owned cell and is destroyed when
        *       the returned handle is dropped or when the factory is destroyed - whichever first.
        *
        * @warning The handle must not outlive the factory: the deleter dereferences `factory`.
        *
        * @note Runtime cost: an O(1) perfect-hash lookup, a fold mapping the runtime index to
        *       its compile-time slot, then a linear scan of at most `N` optional cells for the
        *       first free one.
        *
        * @note `noexcept` is conditional: `emplace` is `noexcept` iff every registered type
        *       that is constructible from `Args...` is also nothrow-constructible from them.
        */
        template<typename... Args>
        [[nodiscard]] handle_t emplace(key_t key, Args&&... args) noexcept(
            ((not std::is_constructible_v<typename reg<Regs>::type, Args&&...> or std::is_nothrow_constructible_v<typename reg<Regs>::type, Args&&...>) 
            and ...)
        );

    private:
        /**
        * @brief Destroy the object at `slot_index` within the cell for `key`, if any.
        *
        * Private: the only caller is `cell_deleter`. No-op if `key` is unknown or cell empty.
        */
        void reset(key_t key, slot_index_t slot_index) noexcept;
        /**
        * @brief Invoke `fn(std::integral_constant<std::size_t, I>{})` for the unique `I`
        *        in `Is...` where `I == index`, then stop. No-op if `index` is not in `Is`.
        *
        * @tparam Fn    Callable accepting a `std::integral_constant<std::size_t, I>`.
        * @tparam Is    Compile-time index sequence; typically `0..type_count-1`.
        *
        * @param[in] index  Runtime index to match.
        * @param[in] fn     Action to invoke on the matching index.
        *
        * @note Zero runtime overhead: `Fn` is monomorphized and the constant is folded
        *       by the compiler at any optimization level.
        */
        template<std::size_t... Is, typename Fn>
        static void index_dispatch(std::size_t index, std::index_sequence<Is...>, Fn&& fn) noexcept;
        /**
        * @brief Accessor for the canonical compile-time lookup artifact.
        *
        * @return `constexpr const&` to the MPH singleton for the extracted keys.
        */
        static constexpr const auto& mpht() noexcept;
        /**
        * @brief Dispatch to the first free slot of the `index`-th type and emplace.
        *
        * @param[in]  index    Dense type index in `[0..type_count-1]` returned by the MPH.
        * @param[out] out_slot Receives the slot index of the constructed object on success.
        * @param[in]  args     Constructor arguments forwarded to the derived type.
        *
        * @return Pointer to the constructed base subobject, or `nullptr` if the type is not
        *         constructible from `Args` or all its slots are occupied.
        */
        template<typename... Args>
        Base* dispatch(std::size_t index, slot_index_t& out_slot, Args&&... args);
        /**
        * @brief Owned storage: one array of optionals per registered type, in declaration order.
        *
        * `std::get<I>(_slots)` yields `std::array<std::optional<T>, N>` for registration `I`.
        * The MPH maps a key to the tuple index in declaration order.
        */
        std::tuple<std::array<std::optional<typename reg<Regs>::type>, reg<Regs>::count>...> _slots{};

        static_assert((not std::is_abstract_v<typename reg<Regs>::type> && ...), "registered type cannot be abstract: it cannot be constructed.");
        static_assert((std::is_nothrow_destructible_v<typename reg<Regs>::type> && ...),
            "registered type must be nothrow-destructible; destruction runs in noexcept paths.");
    };

    /**
    * @brief Typelist adapter: unwraps `meta::typelist<Ts...>` and delegates to the primary
    *        `dispatch_factory`. Bare types and `capacity<T,N>` tags may be mixed in the list.
    */
    template<typename Base, template<typename> typename Extractor, typename... Ts>
    class dispatch_factory<Base, Extractor, meta::typelist<Ts...>>
        : public dispatch_factory<Base, Extractor, Ts...> {};

} // namespace etools::factories

#include "dispatch_factory.tpp"
#endif //ETOOLS_FACTORIES_DISPATCH_FACTORY_HPP_
