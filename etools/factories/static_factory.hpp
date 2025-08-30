// SPDX-License-Identifier: MIT
/**
* @file static_factory.hpp
*
* @ingroup etools_factories etools::factories
*
* @details
* This header provides a zero-allocation, header-only facility to construct one of
* several `DerivedTypes...` by a `key` extracted from each derived type via an `Extractor<T>`.
* The mapping from keys to dense indices is resolved entirely at compile time through
* `etools::hashing::optimal_mph`, yielding O(1) runtime dispatch with no dynamic memory.
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
* - `etools::factories::static_factory<Base, Extractor, DerivedTypes...>` – public facade.
* - `etools::factories::details::static_factory<...>` – internal implementation.
*
* ## Example
* @code
* #include <etools/factories/static_factory.hpp>
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
* using factory = etools::factories::static_factory<
*   Base,
*   key_extractor,
*   etools::meta::typelist<A, B, C>
* >;
*
* int main() {
*   // Default-construct A
*   Base* pa = factory::emplace(A::key);
*
*   // Construct B with an int argument
*   Base* pb = factory::emplace(B::key, 42);
*
*   // Construct C with either copy- or move-constructed string
*   std::string hello = "hello";
*   Base* pc1 = factory::emplace(C::key, hello);        // copy ctor
*   Base* pc2 = factory::emplace(C::key, std::string("hi")); // move ctor
* }
* @endcode
*
* At compile time:
* - `factory::emplace(B::key, 42)` is accepted because `B` is constructible from `int`.
* - If you attempted `factory::emplace(B::key, std::string("oops"))`, that overload
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
#ifndef ETOOLS_FACTORIES_STATIC_FACTORY_HPP_
#define ETOOLS_FACTORIES_STATIC_FACTORY_HPP_
#include "../meta/typelist.hpp"
#include "../meta/traits.hpp"
#include "../memory/slot.hpp"
#include "../hashing/optimal_mph.hpp"
#include <utility>
namespace etools::factories{
    namespace details{
        /**
        * @class static_factory
        * @brief Implementation of a compile-time registry for constructing derived types by key.
        *
        * @tparam Base       Polymorphic base type.
        * @tparam Extractor  `template<class T> struct Extractor { static constexpr auto value; };`
        * @tparam DerivedTypes... Registered derived types.
        *
        * @note Keys are extracted as `std::remove_cv_t<decltype(Extractor<T>::value)>`
        *       and must be pairwise-distinct. The lookup uses a minimal-overhead MPH.
        */
        template<typename Base, template<typename> typename Extractor, typename... DerivedTypes>
        class static_factory{
            /**
            * @typedef sample_t 
            * 
            * @brief The first derived type used to deduce key key type.
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
        public:

            /**
            * @brief Construct/replace the instance associated with `key` in its preallocated slot.
            * 
            * @tparam Args Constructor argument types forwarded to the selected `Derived`.
            * 
            * @param[in] key  The key corresponding to a registered derived type.
            * @param[in] args Constructor arguments forwarded to the selected `Derived`.
            * 
            * @return Pointer to the constructed `Base` subobject, or `nullptr` if `key` is not found.
            * 
            * @note O(1) runtime: one MPH lookup + one branchless dispatch.
            * 
            * @warning `Extractor<T>::value` must be a usable constant expression and unique per `T`.
            * @warning If an object with the same key is allocated already, it's destructor is called
            *          And a new instance replaces it.
            */
            template<typename... Args>
            [[nodiscard]] Base* emplace(key_t key, Args&&... args) noexcept;

        private:
            /**
            * @brief Accessor for the canonical compile-time lookup artifact.
            * 
            * @return `constexpr const&` to the MPH singleton for the extracted keys.
            */
            static constexpr const auto& mpht() noexcept;
            
            /**
            * @brief Try to emplace a specific derived type if its constructor matches `Args`.
            *
            * @tparam T   Target derived type to construct.
            * @tparam Args Pack of argument types forwarded to `lot<T>::emplace`.
            *
            * @param[out] out  Receives the constructed object pointer (as `Base*` ) if construction happens.
            * @param[in]  args Constructor arguments perfectly forwarded to `T` .
            *
            * @return `true` if `T` is constructible from `Args` and construction was attempted;
            *         otherwise `false` (no attempt was made).
            *
            * @note This function is SFINAE-guarded via `if constexpr` :
            *       if `T` is not constructible from `Args`, the branch compiles to a no-op without
            *       instantiating any ill-formed code paths. This prevents compilation failures when the
            *       factory is called with argument lists that do not match all registered types.
            *
            * @warning This function does not validate the key/index; callers should only invoke it
            *          from a validated dispatch path.
            */
            template<typename T, typename... Args>
            static bool try_emplace_if_constructible(Base*& out, Args&&... args) noexcept;


            /**
            * @brief Dispatch to the `index`-th derived’s slot and `emplace` with perfect forwarding.
            * 
            * @tparam Args Constructor argument types.
            * 
            * @param[in] index Dense index in `[0..count-1]` returned by the MPH.
            * @param[in] args  Constructor arguments forwarded to the derived type.
            * 
            * @return Pointer to the constructed base subobject, or `nullptr` if `index` is out of range.
            */
            template<typename... Args>
            static Base* dispatch(std::size_t index, Args&&... args) noexcept;
            
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
            *       `Args...` signature. Use `meta::pack_at_t` instead of a recursive `nth_t` to avoid
            *       quadratic meta-work.
            */
            template<typename... Args, std::size_t... Is>
            static Base* dispatch_fold(std::size_t index, std::index_sequence<Is...>, Args&&... args) noexcept;

            static_assert((not std::is_abstract_v<DerivedTypes> && ...), "DerivedType can't be abstract because there is no way to construct it.");
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
    template<typename Base, template<typename> typename Exctractor, typename... DerivedTypes>
    class static_factory : public details::static_factory<Base, Exctractor, DerivedTypes...>{};

    /**
    * @brief Typelist adapter specialization.
    */
    template<typename Base, template<typename> typename Extractor, typename...DerivedTypes>
    class static_factory<Base, Extractor, meta::typelist<DerivedTypes...>> : public details::static_factory<Base, Extractor, DerivedTypes...>{};
            
} // namespace etools::factories

#include "static_factory.tpp"
#endif //ETOOLS_FACTORIES_STATIC_FACTORY_HPP_