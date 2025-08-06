/**
* @file meta.hpp
*
* @brief Aggregated header for compile-time introspection utilities.
*
* @ingroup etools
*
* @defgroup etools_meta etools::meta
*
* This file provides a single include point for all trait-generation macros defined under
* the `etools::meta` namespace. These macros allow C++ metaprogrammers to check for the
* existence of members, methods, or nested types in user-defined types, enabling highly
* flexible template logic.
*
* @author Mark Tikhonov <mtik.philosopher@gmail.com>
*
* @date 2025-08-05
*
* @copyright
* MIT License
* Copyright (c) 2025 Mark Tikhonov
* See the accompanying LICENSE file for details.
*/
#ifndef ETOOLS_META_HPP_
#define ETOOLS_META_HPP_
#include "info_gen.hpp"
#include "traits.hpp"
#endif //ETOOLS_META_HPP_