// SPDX-License-Identifier: MIT
/**
* @file flags.hpp
*
* @brief Opt-in bitwise operators and flag enumeration utilities for
*        strongly-typed enums (`enum class`).
*
* @ingroup etools_meta etools::meta
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2026-05-26
*
* @copyright
* MIT License
* Copyright (c) 2026 Mark Tikhonov
* See the accompanying LICENSE file for details.
*
* ## Overview
*
* C++ `enum class` types deliberately forbid implicit integer conversions and
* therefore do not participate in bitwise operations. This is desirable for
* domain enums (e.g. `state::idle`, `state::running`) but inconvenient when the
* enum is genuinely a bitmask — header flags, feature toggles, error masks.
*
* This header provides a single, type-safe mechanism to opt a specific
* `enum class` into bitwise behavior, plus two introspection helpers to walk
* the bits of such a mask.
*
* ## Mechanism
*
* The opt-in is a single explicit specialization of the `enable_flags` trait:
*
* @code{.cpp}
* enum class my_flags : std::uint16_t {
*     none  = 0,
*     read  = 1u << 0,
*     write = 1u << 1,
*     exec  = 1u << 2
* };
*
* template <>
* struct etools::meta::enable_flags<my_flags> : std::true_type {};
* @endcode
*
* Each operator overload in this header is constrained via SFINAE
* (`std::enable_if_t<enable_flags<Enum>::value, ...>`), so:
*   - enums that have *not* opted in are completely unaffected;
*   - overload resolution never accidentally pulls these operators into an
*     unrelated translation unit.
*
* ## What is provided
*
* - `enable_flags<Enum>`: opt-in trait, defaults to `std::false_type`.
* - Bitwise operator overloads:
*   `operator|`, `operator|=`, `operator&`, `operator&=`,
*   `operator^`, `operator^=`, `operator~`.
* - `enumerate_flags(value)`: positional listing — index `i` of the returned
*   array is non-zero iff bit `i` is set in `value`.
* - `extract_flags(value)`: compacted listing — the set bits are placed at the
*   front of the returned array, and the count of set bits is returned
*   alongside it.
*
* ## Important Notes
*
* - Only enums explicitly opted in via `enable_flags` see these operators.
* - Always declare flag enums with an *unsigned* underlying type
*   (`std::uint8_t`, `std::uint16_t`, …). Signed underlying types make
*   `operator~` and high-bit values implementation-defined.
* - The user is responsible for choosing enumerator values that behave as
*   bitmasks (powers of two, or composites thereof). This header does not
*   enforce that.
*
* ## Specialization placement
*
* The `enable_flags<...>` specialization must be visible wherever the bitwise
* operators are used; therefore specialize it in the header that declares the
* enum, immediately after the enum definition. Specializing it inside a `.cpp`
* translation unit only is enough to make the operators silently disappear
* elsewhere.
*/
#ifndef ETOOLS_META_FLAGS_HPP_
#define ETOOLS_META_FLAGS_HPP_
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace etools::meta {

    /**
    * @brief Opt-in trait that enables bitwise operators for a specific
    *        `enum class` type.
    *
    * The primary template inherits from `std::false_type`, so by default no
    * enum participates in the operator overloads defined in this header.
    * To enable them for a user enum `E`, provide an explicit specialization
    * that inherits from `std::true_type`:
    *
    * @code{.cpp}
    * template <>
    * struct etools::meta::enable_flags<E> : std::true_type {};
    * @endcode
    *
    * @tparam Enum The `enum class` type being queried.
    *
    * @note The specialization must be reachable (by ordinary name lookup) at
    *       every point where a bitwise operator on `Enum` is required.
    * @note Specializing for non-enum types is a programming error; the
    *       operator overloads themselves additionally check `std::is_enum_v`
    *       via `static_assert`/SFINAE, but the trait does not.
    */
    template <typename Enum>
    struct enable_flags : std::false_type {};

    /**
    * @brief Convenience variable template for `enable_flags<Enum>::value`.
    *
    * @tparam Enum The `enum class` type being queried.
    *
    * @note Provided purely as a readability shorthand; semantics are
    *       identical to `enable_flags<Enum>::value`.
    */
    template <typename Enum>
    inline constexpr bool enable_flags_v = enable_flags<Enum>::value;

    /**
    * @name Bitwise Operators
    * @brief Overloaded bitwise operators for `enum class` types that opt-in
    *        via `enable_flags`.
    *
    * Every operator below is SFINAE-constrained on
    * `enable_flags<Enum>::value` and is therefore invisible to enums that
    * have not opted in. The operations are performed on
    * `std::underlying_type_t<Enum>` and the result is cast back to `Enum`,
    * preserving the strong type of the enum at the call site.
    */
    ///@{

    /**
    * @brief Bitwise OR.
    *
    * @tparam Enum An `enum class` type opted-in via `enable_flags`.
    * @param[in] lhs Left operand.
    * @param[in] rhs Right operand.
    * @return `lhs | rhs` evaluated on the underlying integer representation,
    *         re-cast to `Enum`.
    */
    template <typename Enum>
    constexpr std::enable_if_t<enable_flags<Enum>::value, Enum>
    operator|(Enum lhs, Enum rhs) noexcept;

    /**
    * @brief Compound bitwise OR-assignment.
    *
    * @tparam Enum An `enum class` type opted-in via `enable_flags`.
    * @param[in,out] lhs Destination operand; updated in place.
    * @param[in]     rhs Right operand.
    * @return Reference to `lhs` after assignment.
    */
    template <typename Enum>
    constexpr std::enable_if_t<enable_flags<Enum>::value, Enum&>
    operator|=(Enum& lhs, Enum rhs) noexcept;

    /**
    * @brief Bitwise AND.
    *
    * @tparam Enum An `enum class` type opted-in via `enable_flags`.
    * @param[in] lhs Left operand.
    * @param[in] rhs Right operand.
    * @return `lhs & rhs` evaluated on the underlying integer representation,
    *         re-cast to `Enum`.
    *
    * @note A common idiom for membership tests is
    *       `(value & mask) != Enum(0)`.
    */
    template <typename Enum>
    constexpr std::enable_if_t<enable_flags<Enum>::value, Enum>
    operator&(Enum lhs, Enum rhs) noexcept;

    /**
    * @brief Compound bitwise AND-assignment.
    *
    * @tparam Enum An `enum class` type opted-in via `enable_flags`.
    * @param[in,out] lhs Destination operand; updated in place.
    * @param[in]     rhs Right operand.
    * @return Reference to `lhs` after assignment.
    */
    template <typename Enum>
    constexpr std::enable_if_t<enable_flags<Enum>::value, Enum&>
    operator&=(Enum& lhs, Enum rhs) noexcept;

    /**
    * @brief Bitwise XOR.
    *
    * @tparam Enum An `enum class` type opted-in via `enable_flags`.
    * @param[in] lhs Left operand.
    * @param[in] rhs Right operand.
    * @return `lhs ^ rhs` evaluated on the underlying integer representation,
    *         re-cast to `Enum`.
    *
    * @note Useful for toggling individual flags
    *       (`value = value ^ flag_a`).
    */
    template <typename Enum>
    constexpr std::enable_if_t<enable_flags<Enum>::value, Enum>
    operator^(Enum lhs, Enum rhs) noexcept;

    /**
    * @brief Compound bitwise XOR-assignment.
    *
    * @tparam Enum An `enum class` type opted-in via `enable_flags`.
    * @param[in,out] lhs Destination operand; updated in place.
    * @param[in]     rhs Right operand.
    * @return Reference to `lhs` after assignment.
    */
    template <typename Enum>
    constexpr std::enable_if_t<enable_flags<Enum>::value, Enum&>
    operator^=(Enum& lhs, Enum rhs) noexcept;

    /**
    * @brief Bitwise NOT.
    *
    * @tparam Enum An `enum class` type opted-in via `enable_flags`.
    * @param[in] lhs Operand.
    * @return `~lhs` evaluated on the underlying integer representation,
    *         re-cast to `Enum`.
    *
    * @warning The result includes every bit of the underlying type, including
    *          bits that do not correspond to any defined enumerator. Combine
    *          with a known full-mask value (e.g. `value & all_flags`) when
    *          that matters.
    * @warning Using `~` on an enum whose underlying type is signed is
    *          implementation-defined. Always declare flag enums with an
    *          unsigned underlying type.
    */
    template <typename Enum>
    constexpr std::enable_if_t<enable_flags<Enum>::value, Enum>
    operator~(Enum lhs) noexcept;

    ///@}

    /**
    * @brief Positional enumeration of every bit in a flag value.
    *
    * Walks bits `[0, Size)` of `value`'s underlying integer representation
    * and produces an array whose `i`-th element is:
    *   - the single-bit `Enum` value `Enum(1u << i)` if bit `i` is set;
    *   - `Enum(0)` otherwise.
    *
    * Element positions are preserved — unlike `extract_flags`, set bits are
    * *not* compacted toward the front. This makes the result suitable for
    * code that needs to know *which* bit was set, not just how many.
    *
    * @tparam Enum The `enum class` type to inspect.
    * @tparam Size Number of bit positions to examine. Defaults to the full
    *         width of `std::underlying_type_t<Enum>` in bits.
    * @param[in] value The bitmask to enumerate.
    * @return `std::array<Enum, Size>` indexed by bit position. Inactive
    *         positions hold `Enum(0)`.
    *
    * @pre `Enum` is an `enum` type (`std::is_enum_v<Enum>`).
    * @pre `Enum` is opted-in via `enable_flags<Enum>::value`.
    *
    * @note Both preconditions are enforced by `static_assert` inside the
    *       function body; violations produce a diagnostic at instantiation.
    * @note `Size` may legitimately be smaller than the full underlying width
    *       when the caller knows that only the low `Size` bits are
    *       meaningful — this trims the returned array and the loop.
    *
    * ## Example
    * @code
    * auto bits = etools::meta::enumerate_flags(my_flags::read | my_flags::exec);
    * for (std::size_t i = 0; i < bits.size(); ++i) {
    *     if (bits[i] != my_flags{0}) {
    *         // bit i was set; bits[i] equals the single-bit enumerator
    *     }
    * }
    * @endcode
    */
    template <typename Enum,
              std::size_t Size = sizeof(std::underlying_type_t<Enum>) * 8ULL>
    constexpr std::array<Enum, Size> enumerate_flags(Enum value) noexcept;

    /**
    * @brief Compact extraction of the set flags in a bitmask.
    *
    * Decomposes `value` into its constituent single-bit `Enum` values and
    * returns them packed at the front of a fixed-size array. The accompanying
    * `std::size_t` is the count of set bits, i.e. one past the index of the
    * last meaningful element. Trailing elements (from `count` to `Size`) are
    * left as `Enum(0)`.
    *
    * Use this overload when the caller wants to iterate only the *active*
    * flags and does not care about their original bit position. If the bit
    * position matters, use `enumerate_flags` instead.
    *
    * @tparam Enum The `enum class` type to inspect.
    * @tparam Size Number of bit positions to examine. Defaults to the full
    *         width of `std::underlying_type_t<Enum>` in bits, which is also
    *         the maximum number of active flags representable in the result.
    * @param[in] value The bitmask to decompose.
    * @return A pair `{flags, count}` where:
    *         - `flags[0 .. count)` are the single-bit `Enum` values that were
    *           set in `value`, in ascending bit order;
    *         - `flags[count .. Size)` are `Enum(0)`;
    *         - `count` is the population count of `value` restricted to the
    *           low `Size` bits.
    *
    * @pre `Enum` is an `enum` type (`std::is_enum_v<Enum>`).
    * @pre `Enum` is opted-in via `enable_flags<Enum>::value`.
    *
    * @note Both preconditions are enforced by `static_assert` inside the
    *       function body; violations produce a diagnostic at instantiation.
    * @note Because the array is sized at compile time, no allocation occurs
    *       and the function is suitable for use in embedded code paths.
    *
    * ## Example
    * @code
    * auto [flags, count] =
    *     etools::meta::extract_flags(my_flags::read | my_flags::exec);
    * for (std::size_t i = 0; i < count; ++i) {
    *     // flags[i] is one of: my_flags::read, my_flags::exec
    * }
    * @endcode
    */
    template <typename Enum,
              std::size_t Size = sizeof(std::underlying_type_t<Enum>) * 8ULL>
    constexpr std::pair<std::array<Enum, Size>, std::size_t>
    extract_flags(Enum value) noexcept;

} // namespace etools::meta

#include "flags.tpp"
#endif // ETOOLS_META_FLAGS_HPP_
