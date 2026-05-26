// SPDX-License-Identifier: MIT
/**
* @file flags.tpp
*
* @brief Definitions for the templates declared in flags.hpp.
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
*/
#ifndef ETOOLS_META_FLAGS_TPP_
#define ETOOLS_META_FLAGS_TPP_
#include "flags.hpp"
#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace etools::meta {

    template <typename Enum>
    constexpr std::enable_if_t<enable_flags<Enum>::value, Enum>
    operator|(Enum lhs, Enum rhs) noexcept {
        using underlying = std::underlying_type_t<Enum>;
        return static_cast<Enum>(
            static_cast<underlying>(lhs) | static_cast<underlying>(rhs)
        );
    }

    template <typename Enum>
    constexpr std::enable_if_t<enable_flags<Enum>::value, Enum&>
    operator|=(Enum& lhs, Enum rhs) noexcept {
        lhs = lhs | rhs;
        return lhs;
    }

    template <typename Enum>
    constexpr std::enable_if_t<enable_flags<Enum>::value, Enum>
    operator&(Enum lhs, Enum rhs) noexcept {
        using underlying = std::underlying_type_t<Enum>;
        return static_cast<Enum>(
            static_cast<underlying>(lhs) & static_cast<underlying>(rhs)
        );
    }

    template <typename Enum>
    constexpr std::enable_if_t<enable_flags<Enum>::value, Enum&>
    operator&=(Enum& lhs, Enum rhs) noexcept {
        lhs = lhs & rhs;
        return lhs;
    }

    template <typename Enum>
    constexpr std::enable_if_t<enable_flags<Enum>::value, Enum>
    operator^(Enum lhs, Enum rhs) noexcept {
        using underlying = std::underlying_type_t<Enum>;
        return static_cast<Enum>(
            static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs)
        );
    }

    template <typename Enum>
    constexpr std::enable_if_t<enable_flags<Enum>::value, Enum&>
    operator^=(Enum& lhs, Enum rhs) noexcept {
        lhs = lhs ^ rhs;
        return lhs;
    }

    template <typename Enum>
    constexpr std::enable_if_t<enable_flags<Enum>::value, Enum>
    operator~(Enum lhs) noexcept {
        using underlying = std::underlying_type_t<Enum>;
        return static_cast<Enum>(~static_cast<underlying>(lhs));
    }

    template <typename Enum, std::size_t Size>
    constexpr std::array<Enum, Size> enumerate_flags(Enum value) noexcept {
        static_assert(
            std::is_enum_v<Enum>,
            "etools::meta::enumerate_flags: Enum must be an enum type "
            "(prefer enum class with an unsigned underlying type)."
        );
        static_assert(
            enable_flags<Enum>::value,
            "etools::meta::enumerate_flags: Enum must opt-in to bitwise "
            "operations by specializing etools::meta::enable_flags<Enum> "
            "as std::true_type."
        );
        using underlying = std::underlying_type_t<Enum>;
        std::array<Enum, Size> flags{};
        for (std::size_t i = 0; i < Size; ++i) {
            const auto bit = static_cast<Enum>(static_cast<underlying>(1) << i);
            flags[i] = value & bit;
        }
        return flags;
    }

    template <typename Enum, std::size_t Size>
    constexpr std::pair<std::array<Enum, Size>, std::size_t>
    extract_flags(Enum value) noexcept {
        static_assert(
            std::is_enum_v<Enum>,
            "etools::meta::extract_flags: Enum must be an enum type "
            "(prefer enum class with an unsigned underlying type)."
        );
        static_assert(
            enable_flags<Enum>::value,
            "etools::meta::extract_flags: Enum must opt-in to bitwise "
            "operations by specializing etools::meta::enable_flags<Enum> "
            "as std::true_type."
        );
        using underlying = std::underlying_type_t<Enum>;
        std::array<Enum, Size> flags{};
        std::size_t pos = 0;
        for (std::size_t i = 0; i < Size; ++i) {
            const auto bit = static_cast<Enum>(static_cast<underlying>(1) << i);
            const auto masked = value & bit;
            if (masked != static_cast<Enum>(0))
                flags[pos++] = masked;
        }
        return {flags, pos};
    }

} // namespace etools::meta

#endif // ETOOLS_META_FLAGS_TPP_
