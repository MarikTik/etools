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
*
* ## Compile-time Considerations
* - Dispatch is implemented via a fold expression across all registered types.
* - For very large registries (1,000+ types with many constructor variations),
*   compile times may become significant. On non-professional systems this can
*   impact developer experience. The generated code remains efficient at runtime.
* - **Compile-time:** Roughly O(N × K), where N is the number of registered types
*   and K is the number of distinct constructor argument signatures seen.
* ## Components
* - `etools::factories::dispatch_factory<Base, Extractor, DerivedTypes...>` – public facade.
* - `etools::factories::details::dispatch_factory<...>` – internal implementation.
*
* ## Example
* @code
* #include <etools/factories/dispatch_factory.hpp>
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
* struct C : Base {
*   static constexpr std::uint16_t key = 7;
*   std::string s;
*   C(const std::string& ss) : s(ss) {}
*   C(std::string&& ss) noexcept : s(std::move(ss)) {}
*   const char* tag() const noexcept override { return "C"; }
* };
*
* template<class T>
* struct key_extractor {
*   static constexpr auto value = T::key;
* };
*
* using factory_t = etools::factories::dispatch_factory<
*   Base,
*   key_extractor,
*   etools::meta::typelist<A, B, C>
* >;
*
* int main() {
*   factory_t factory;   // owns storage for one A, one B, one C
*
*   // Default-construct A
*   Base* pa = factory.emplace(A::key);
*
*   // Construct B with an int argument
*   Base* pb = factory.emplace(B::key, 42);
*
*   // Construct C with either copy- or move-constructed string
*   std::string hello = "hello";
*   Base* pc1 = factory.emplace(C::key, hello);        // copy ctor
*   Base* pc2 = factory.emplace(C::key, std::string("hi")); // move ctor
*
*   // All three objects are destroyed when `factory` goes out of scope.
* }
* @endcode
*
* At compile time:
* - `factory.emplace(B::key, 42)` is accepted because `B` is constructible from `int`.
* - If you attempted `factory.emplace(B::key, std::string("oops"))`, that overload
*   would compile to a no-op and return `nullptr` at runtime, because no `Derived`
*   type is constructible from `(std::string)`.
*
* @note This mechanism ensures both type-safety and efficiency: invalid constructor
*       signatures are eliminated at compile time, while dispatch to the correct
*       derived type is constant-time at runtime.
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
#include "../meta/typelist.hpp"
#include "../meta/traits.hpp"
#include "../hashing/optimal_mph.hpp"
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
namespace etools::factories{
    namespace details{
        /**
        * @class dispatch_factory
        * @brief Implementation of a compile-time registry for constructing derived types by key.
        *
        * @tparam Base       Polymorphic base type.
        * @tparam Extractor  `template<class T> struct Extractor { static constexpr auto value; };`
        * @tparam DerivedTypes... Registered derived types.
        *
        * The factory **owns** the storage for its derived objects: one
        * `std::optional<Derived>` per registered type, held in a tuple. Constructing
        * an object via `emplace()` places it into the corresponding cell; the objects are
        * destroyed when the factory is destroyed (RAII). There is no global/static storage.
        *
        * @note Keys are extracted as `std::remove_cv_t<decltype(Extractor<T>::value)>`
        *       and must be pairwise-distinct. The lookup uses a minimal-overhead MPH.
        * @note The factory is a pinned type: copy and move are both deleted (like
        *       `std::mutex`). It owns live objects in-place, so relocating it is not
        *       supported; hold it by reference, as a `static`, or on the stack.
        * @note Not thread-safe: `emplace` mutates shared storage. Use one factory per thread,
        *       or synchronise externally.
        * @note Returned `Base*` is non-owning: the factory retains ownership in its cell. Do
        *       not `delete` it; it is destroyed when replaced (re-`emplace` on the same key) or
        *       when the factory is destroyed.
        */
        template<typename Base, template<typename> typename Extractor, typename... DerivedTypes>
        class dispatch_factory{
            /**
            * @typedef sample_t
            *
            * @brief The first derived type used to deduce key type.
            */
            using sample_t = meta::nth_t<0, DerivedTypes...>;

            /**
            * @typedef key_t
            *
            * @brief The type of the unique key for each derived type, deduced via Extractor metafunction.
            */
            using key_t = std::remove_cv_t<decltype(Extractor<sample_t>::value)>;

            /**
            * @var capacity
            *
            * @brief The number of derived types in this registry.
            */
            static constexpr std::size_t capacity = sizeof...(DerivedTypes);

            static_assert((std::is_same_v<key_t, std::remove_cv_t<decltype(Extractor<DerivedTypes>::value)>> && ...), "all registered types must expose the same key type");
            static_assert(sizeof...(DerivedTypes) > 0, "register at least one type");

            /**
            * @brief Custom deleter for the owning handle returned by `emplace`.
            *
            * Does **not** free memory (the object lives in the factory's cell); instead it calls
            * the factory's private `reset(key)`, which runs the cell's destruction in place. This
            * is the `etools::memory::buffer` custom-deleter pattern: zero-allocation ownership with
            * RAII teardown. A default-constructed deleter (`factory == nullptr`) is a no-op, which
            * is what a null/failed handle carries.
            *
            * @warning The handle must not outlive the factory: the deleter dereferences `factory`.
            */
            struct cell_deleter {
                dispatch_factory* factory = nullptr;
                key_t key{};
                void operator()(Base*) const noexcept { if (factory) factory->reset(key); }
            };
        public:
            /**
            * @brief Owning handle to a constructed object: a `unique_ptr<Base>` whose deleter
            *        tears the object down in its factory cell (no heap free).
            */
            using handle = std::unique_ptr<Base, cell_deleter>;

            /**
            * @brief Constructs an empty factory; every slot starts unoccupied.
            */
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
            * @brief Construct/replace the instance associated with `key` in its owned slot.
            *
            * @tparam Args Constructor argument types forwarded to the selected `Derived`.
            *
            * @param[in] key  The key corresponding to a registered derived type.
            * @param[in] args Constructor arguments forwarded to the selected `Derived`.
            *
            * @return An owning `handle` (a `unique_ptr<Base>`). The handle is **empty**
            *         (`get() == nullptr`) if `key` is not found, or if no registered type is
            *         constructible from `Args...`. Dropping a non-empty handle (or calling
            *         `.reset()` on it) destroys the object in its factory cell.
            *
            * @post On success the object lives in this factory's owned cell and is destroyed when
            *       the returned handle is dropped, when a later `emplace` on the same key replaces
            *       it, or when the factory is destroyed - whichever comes first.
            *
            * @warning The handle must not outlive the factory, and a key must not be re-`emplace`d
            *          while a handle to it is still alive (the stale handle would then reset the
            *          replacement). Both hold trivially for single-threaded, run-to-completion use.
            *
            * @note Runtime cost: an O(1) perfect-hash lookup, then a fold that maps the runtime
            *       index to its compile-time slot. The fold is a linear `index == Is` chain that
            *       optimizers typically lower to a jump table over the dense index range.
            *
            * @note `noexcept` is conditional: `emplace` is `noexcept` iff every registered type
            *       that is constructible from `Args...` is also nothrow-constructible from them.
            *       A throwing constructor on a matching type therefore propagates out of `emplace`
            *       rather than calling `std::terminate`.
            *
            * @warning `Extractor<T>::value` must be a usable constant expression and unique per `T`.
            * @warning If an object with the same key already exists it is destroyed and replaced.
            */
            template<typename... Args>
            [[nodiscard]] handle emplace(key_t key, Args&&... args)
                noexcept(((not std::is_constructible_v<DerivedTypes, Args&&...>
                           or std::is_nothrow_constructible_v<DerivedTypes, Args&&...>) and ...));

        private:
            /**
            * @brief Destroy the object currently held in the cell for `key`, if any.
            *
            * Private: the only caller is `cell_deleter`. Tearing a cell down through the factory
            * directly is intentionally not part of the public surface; ownership flows through the
            * `handle` returned by `emplace`. No-op if `key` is unknown or its cell is empty.
            */
            void reset(key_t key) noexcept;

            /**
            * @brief Fold that resets the `std::optional` whose tuple index equals `index`.
            */
            template<std::size_t... Is>
            void reset_fold(std::size_t index, std::index_sequence<Is...>) noexcept;
            /**
            * @brief Accessor for the canonical compile-time lookup artifact.
            *
            * @return `constexpr const&` to the MPH singleton for the extracted keys.
            */
            static constexpr const auto& mpht() noexcept;

            /**
            * @brief Try to emplace into the `Index`-th owned slot if its type matches `Args`.
            *
            * @tparam Index Tuple index of the target derived type / cell.
            * @tparam Args  Pack of argument types forwarded to `std::optional<T>::emplace`.
            *
            * @param[out] out  Receives the constructed object pointer (as `Base*`) if construction happens.
            * @param[in]  args Constructor arguments perfectly forwarded to the derived type.
            *
            * @return `true` if the target type is constructible from `Args` and construction was
            *         attempted; otherwise `false` (no attempt was made).
            *
            * @note This function is SFINAE-guarded via `if constexpr`:
            *       if the target type is not constructible from `Args`, the branch compiles to a
            *       no-op without instantiating any ill-formed code paths. This prevents compilation
            *       failures when the factory is called with argument lists that do not match all
            *       registered types.
            *
            * @warning This function does not validate the key/index; callers should only invoke it
            *          from a validated dispatch path.
            */
            template<std::size_t Index, typename... Args>
            bool try_emplace_if_constructible(Base*& out, Args&&... args);


            /**
            * @brief Dispatch to the `index`-th owned slot and `emplace` with perfect forwarding.
            *
            * @tparam Args Constructor argument types.
            *
            * @param[in] index Dense index in `[0..count-1]` returned by the MPH.
            * @param[in] args  Constructor arguments forwarded to the derived type.
            *
            * @return Pointer to the constructed base subobject, or `nullptr` if `index` is out of range.
            */
            template<typename... Args>
            Base* dispatch(std::size_t index, Args&&... args);

            /**
            * @brief Fold-based dispatch implementation over an index sequence.
            *
            * @tparam Args Constructor argument types.
            * @tparam Is   Index sequence `[0..count-1]`.
            *
            * @param[in] index Dense index in `[0..count-1]`.
            * @param[in] args  Constructor arguments forwarded to the derived type.
            *
            * @return Pointer to the constructed base subobject, or `nullptr` if not matched.
            *
            * @note Compile-time cost is linear in the number of registered types for each distinct
            *       `Args...` signature.
            */
            template<typename... Args, std::size_t... Is>
            Base* dispatch_fold(std::size_t index, std::index_sequence<Is...>, Args&&... args);

            /**
            * @brief Owned storage: one cell per registered derived type, in declaration order.
            *
            * The MPH maps a key to a dense index in declaration order, which is exactly the
            * tuple index, so `std::get<index>(_slots)` selects the right cell.
            */
            std::tuple<std::optional<DerivedTypes>...> _slots{};

            static_assert((not std::is_abstract_v<DerivedTypes> && ...), "DerivedType can't be abstract because there is no way to construct it.");
            static_assert((std::is_nothrow_destructible_v<DerivedTypes> && ...),
                "DerivedType must be nothrow-destructible; the factory destroys owned objects "
                "during teardown and this code path is noexcept.");
        };
    }

    /**
    * @brief Public facade for the compile-time registry.
    * @tparam Base       Polymorphic base type.
    * @tparam Extractor  Key extractor `template<class T> struct Extractor { static constexpr auto value; };`
    * @tparam DerivedTypes... Registered derived types.
    *
    * @note You may pass a `meta::typelist<...>` as the third template parameter; it is unwrapped.
    */
    template<typename Base, template<typename> typename Extractor, typename... DerivedTypes>
    class dispatch_factory : public details::dispatch_factory<Base, Extractor, DerivedTypes...>{};

    /**
    * @brief Typelist adapter specialization.
    */
    template<typename Base, template<typename> typename Extractor, typename...DerivedTypes>
    class dispatch_factory<Base, Extractor, meta::typelist<DerivedTypes...>> : public details::dispatch_factory<Base, Extractor, DerivedTypes...>{};
            
} // namespace etools::factories

#include "dispatch_factory.tpp"
#endif //ETOOLS_FACTORIES_DISPATCH_FACTORY_HPP_