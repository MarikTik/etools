// SPDX-License-Identifier: MIT
/**
* @file unique_variant.hpp
*
* @brief Builds a `std::variant` (or `typelist`) over the *distinct* types in a
*        parameter pack, silently dropping repeats.
*
* @ingroup etools_meta etools::meta
*
* `std::variant<Ts...>` permits repeated types in `Ts...` — `std::variant<int, int>`
* is well-formed — but two consumers immediately stop being well-formed once it
* happens:
*
* - **Converting construction / assignment** (`variant_type{value}`) becomes
*   ambiguous whenever more than one alternative could accept `value`, forcing
*   callers back to the clumsier `std::in_place_index<N>` form and requiring
*   them to track the index by hand.
* - **`std::visit` with an `etools::meta::overload{...}` set** (or any other
*   one-callable-per-alternative visitor) becomes ill-formed outright: two
*   lambdas taking the same alternative type collide when their `operator()`s
*   are merged via `using Fs::operator()...;`.
*
* Both problems are inherent to `std::variant` and `overload` themselves — no
* visitor-side cleverness can fix an ambiguity that exists in the variant's
* *type*. The fix has to happen upstream, at the point the variant's type list
* is assembled: deduplicate the pack first, then hand the result to
* `std::variant`.
*
* This header exists for exactly that situation: a type list assembled from
* some other pack that is *not* already known to be distinct — most commonly,
* a pack of `T::some_member_type` extracted from a heterogeneous collection of
* things (e.g. the payload types produced by several independent producers),
* where several producers may legitimately share the same payload type.
* `is_distinct_v` (`traits.hpp`) is the right tool when duplicates should be a
* compile error; `unique_variant_t` is the right tool when duplicates are
* expected and should simply collapse to one alternative.
*
* ### Example
* @code
* #include "etools/meta/unique_variant.hpp"
* using namespace etools::meta;
*
* // Three producers, two of which happen to share a payload type.
* struct producer_a { using payload_t = int;    };
* struct producer_b { using payload_t = double; };
* struct producer_c { using payload_t = int;    }; // same as producer_a
*
* using result_t = unique_variant_t<
*     producer_a::payload_t, producer_b::payload_t, producer_c::payload_t
* >;
* static_assert(std::is_same_v<result_t, std::variant<int, double>>);
* // Not std::variant<int, double, int> -- constructing or visiting that
* // would be ambiguous for the `int` case.
* @endcode
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2026-07-15
*
* @copyright
* MIT License
* Copyright (c) 2026 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_META_UNIQUE_VARIANT_HPP_
#define ETOOLS_META_UNIQUE_VARIANT_HPP_
#include <type_traits> // For std::conditional_t, std::is_same_v
#include <variant>     // For std::variant
#include "typelist.hpp" // For typelist, the intermediate representation of the deduplicated pack

namespace etools::meta::details {

    /**
    * @struct unique_typelist_builder
    *
    * @brief Recursive worker that folds `Ts...` into a `typelist` of its
    *        distinct members, in first-occurrence order.
    *
    * Not part of the public API — use `unique_typelist_t` / `unique_variant_t`.
    *
    * The accumulator is carried as the first template argument, wrapped in a
    * `typelist<Seen...>`, so the whole computation is a single template
    * instantiation chain rather than a class hierarchy: each step either
    * folds `T` into `Seen...` (if `T` has not appeared yet) or discards it
    * (if it has) and recurses on `Rest...`. The base case — `Rest...` empty —
    * is provided by the primary template below, which stops the recursion by
    * *not* matching the specialization's `T, Rest...` pattern.
    *
    * @tparam Accumulated The `typelist` of distinct types collected so far.
    * @tparam Ts          The remaining, not-yet-examined types.
    */
    template<typename Accumulated, typename... Ts>
    struct unique_typelist_builder {
        /// @brief Base case: nothing left to examine, so the accumulator is the answer.
        using type = Accumulated;
    };

    /// @brief Recursive case: fold `T` into `Seen...` unless it already occurs there.
    template<typename... Seen, typename T, typename... Rest>
    struct unique_typelist_builder<typelist<Seen...>, T, Rest...>
        : std::conditional_t<
            (std::is_same_v<T, Seen> or ...),
            unique_typelist_builder<typelist<Seen...>, Rest...>,       // T already seen: drop it
            unique_typelist_builder<typelist<Seen..., T>, Rest...>     // T is new: keep it
        > {};

    /**
    * @struct typelist_to_variant
    *
    * @brief Converts a `typelist<Ts...>` into `std::variant<Ts...>`.
    *
    * Not part of the public API — use `unique_variant_t`. Kept as a separate
    * step (rather than folding directly into `std::variant`) so the
    * deduplicated type list is independently available as a `typelist` via
    * `unique_typelist_t`, e.g. for callers who want to run further
    * metaprogramming over the distinct types before deciding what container
    * to place them in.
    */
    template<typename List>
    struct typelist_to_variant;

    /// @brief Specialization that unpacks the `typelist` into `std::variant`'s argument list.
    template<typename... Ts>
    struct typelist_to_variant<typelist<Ts...>> {
        using type = std::variant<Ts...>;
    };

} // namespace etools::meta::details

namespace etools::meta {

    /**
    * @struct unique_typelist
    *
    * @brief Deduplicates a parameter pack into a `typelist` of its distinct
    *        members, preserving first-occurrence order.
    *
    * `unique_typelist<int, double, int, float, double>::type` is
    * `typelist<int, double, float>` — each repeat is dropped at the position
    * it would have re-occurred, not moved or reordered; the *first*
    * occurrence of each type determines its position in the result.
    *
    * This is the general-purpose primitive `unique_variant` is built on. Most
    * callers reaching for "distinct packet/payload types across several
    * producers" want `unique_variant_t` directly; reach for
    * `unique_typelist_t` instead when the deduplicated pack needs to feed
    * something other than `std::variant` (a different tagged union type, a
    * dispatch table keyed by type, ...).
    *
    * @tparam Ts The (possibly repeating) parameter pack to deduplicate.
    *
    * @note Empty pack: `unique_typelist<>::type` is `typelist<>` — well-formed,
    *       trivially "already unique".
    *
    * @see unique_typelist_t
    * @see unique_variant
    */
    template<typename... Ts>
    struct unique_typelist {
        using type = typename details::unique_typelist_builder<typelist<>, Ts...>::type;
    };

    /**
    * @brief Convenience alias for `unique_typelist<Ts...>::type`.
    *
    * @tparam Ts The (possibly repeating) parameter pack to deduplicate.
    *
    * @code
    * using result_t = etools::meta::unique_typelist_t<int, double, int>;
    * static_assert(std::is_same_v<result_t, etools::meta::typelist<int, double>>);
    * @endcode
    *
    * @see unique_typelist
    */
    template<typename... Ts>
    using unique_typelist_t = typename unique_typelist<Ts...>::type;

    /**
    * @struct unique_variant
    *
    * @brief Builds a `std::variant` over the *distinct* types in `Ts...`,
    *        preserving first-occurrence order and silently dropping repeats.
    *
    * See the file-level documentation for *why* this is necessary rather
    * than instantiating `std::variant<Ts...>` directly: a repeated type in a
    * `std::variant`'s alternative list breaks unambiguous converting
    * construction and makes a one-lambda-per-alternative `std::visit` (via
    * `etools::meta::overload`) ill-formed. `unique_variant` removes the
    * repeats before `std::variant` ever sees them, so both remain usable
    * regardless of how many times a given type recurs in `Ts...`.
    *
    * @tparam Ts The (possibly repeating) parameter pack of candidate
    *            alternative types.
    *
    * @note Empty pack: `unique_variant<>::type` is `std::variant<>` — a
    *       well-formed but uninhabited type (it has no alternative to hold,
    *       so it cannot be default-constructed and every one of its states is
    *       `valueless_by_exception`). This mirrors what an empty
    *       `Ts...` structurally means ("no types were offered") rather than
    *       treating it as an error; callers assembling `Ts...` from something
    *       that must be non-empty (e.g. `hub<Channels...>`'s `sizeof...(Channels)
    *       >= 1` invariant) never hit this case in practice.
    *
    * @see unique_variant_t
    * @see unique_typelist
    */
    template<typename... Ts>
    struct unique_variant {
        using type = typename details::typelist_to_variant<unique_typelist_t<Ts...>>::type;
    };

    /**
    * @brief Convenience alias for `unique_variant<Ts...>::type`.
    *
    * @tparam Ts The (possibly repeating) parameter pack of candidate
    *            alternative types.
    *
    * @code
    * #include "etools/meta/unique_variant.hpp"
    * #include "etools/meta/overload.hpp"
    *
    * using payload_t = etools::meta::unique_variant_t<int, double, int>;
    * static_assert(std::is_same_v<payload_t, std::variant<int, double>>);
    *
    * payload_t p{3.14};
    * std::visit(etools::meta::overload{
    *     [](int)    { ... },
    *     [](double) { ... },
    * }, p);
    * @endcode
    *
    * @see unique_variant
    */
    template<typename... Ts>
    using unique_variant_t = typename unique_variant<Ts...>::type;

} // namespace etools::meta

#endif // ETOOLS_META_UNIQUE_VARIANT_HPP_
