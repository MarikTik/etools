# etools - Embedded C++17 Utility Library

**etools** is a modular, header-only C++17 utility library and the foundational tools
component of [elib](https://github.com/MarikTik/elib). It provides carefully designed
building blocks for template metaprogramming, compile-time hashing, in-place memory
management, and zero-allocation object dispatch - all targeting real embedded platforms
(ESP32, Arduino-class MCUs) where there is no heap, no exceptions, and no RTTI.

Every facility shares the same constraints: no dynamic allocation on any hot path, no
C++ exceptions, no RTTI, C++17 minimum. The library can be included in whole via the
umbrella header or pulled in one module at a time.

```cpp
#include "etools/etools.hpp"

// Compile-time perfect hash - no runtime tables, no allocations.
using H = etools::hashing::optimal_mph<std::uint8_t>;
constexpr const auto& table = H::instance<10, 20, 30, 40>();

static_assert(table(20) == 1);
static_assert(table(99) == table.not_found());

// Zero-allocation polymorphic factory - objects live in factory-owned array slots.
etools::factories::dispatch_factory<Base, key_of, A, B, C> factory;
auto h = factory.emplace(A::key, /* ctor args */);   // handle_t = unique_ptr<Base, cell_deleter>
```

---

## Table of Contents

- [Overview](#overview)
- [Requirements](#requirements)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Module: etools/meta](#module-etoolsmeta)
  - [traits.hpp](#traitshpp)
  - [typelist.hpp](#typelisthpp)
  - [typeset.hpp](#typesethpp)
  - [flags.hpp](#flagshpp)
  - [info_gen.hpp](#info_genhpp)
  - [utility.hpp](#utilityhpp)
  - [overload.hpp](#overloadhpp)
  - [unique_variant.hpp](#unique_varianthpp)
  - [sort.hpp](#sorthpp)
- [Module: etools/hashing](#module-etoolshashing)
  - [utils.hpp](#utils-hashing)
  - [llut.hpp](#lluthpp)
  - [fks.hpp](#fkshpp)
  - [optimal_mph.hpp](#optimal_mphhpp)
- [Module: etools/memory](#module-etoolsmemory)
  - [slot.hpp](#slothpp)
  - [buffer.hpp](#bufferhpp)
  - [buffer_view.hpp](#buffer_viewhpp)
- [Module: etools/factories](#module-etoolsfactories)
  - [capacity.hpp](#capacityhpp)
  - [dispatch_factory.hpp](#dispatch_factoryhpp)
- [Limitations](#limitations)
- [Testing](#testing)
- [Project Layout](#project-layout)
- [License](#license)

---

## Overview

etools is organized into four independent modules that can be included selectively:

| Module | Umbrella header | Namespace | Purpose |
|--------|-----------------|-----------|---------|
| meta | `etools/meta/meta.hpp` | `etools::meta` | Type traits, typelists, bitmask enums, introspection macros |
| hashing | `etools/hashing/hashing.hpp` | `etools::hashing` | Compile-time minimal perfect hash tables |
| memory | `etools/memory/memory.hpp` | `etools::memory` | In-place storage, buffer ownership and views |
| factories | `etools/factories/factories.hpp` | `etools::factories` | Zero-allocation polymorphic factory by key |

The full umbrella `etools/etools.hpp` includes all four.

All modules:

- Are **header-only** (`.tpp` files are included by their corresponding headers; you
  include only `.hpp` files).
- Carry **no runtime initialization order dependencies**: singletons are `inline constexpr`
  variable templates with static storage duration.
- Are **embedded-safe**: no `new`/`delete`, no `std::vector`, no exceptions on hot paths.

---

## Requirements

- **C++17** or newer. The library uses `if constexpr`, fold expressions, `std::optional`,
  `std::byte`, structured bindings, `inline constexpr`, and `std::launder`.
- An embedded GCC or Clang toolchain that supports the above (ESP-IDF v5.x and Arduino-
  ESP32 core both qualify).
- `<cassert>` is used for debug-build precondition checks. Define `NDEBUG` in production
  builds to strip all assertions.
- `etools/memory/buffer.hpp` and `buffer_view.hpp` depend on **eser** (the elib sibling
  serialization library) for `pack`/`unpack`. All other modules have no sibling dependencies.

---

## Installation

etools is header-only. Clone or copy the repository so that the `etools/` folder is on
your include path, then include what you need:

```cpp
#include "etools/etools.hpp"                  // everything
#include "etools/meta/meta.hpp"               // meta module only
#include "etools/hashing/optimal_mph.hpp"     // one specific header
```

With CMake (as part of elib or standalone):

```cmake
include(FetchContent)
FetchContent_Declare(
    etools
    GIT_REPOSITORY https://github.com/MarikTik/etools.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(etools)

target_link_libraries(my_target PRIVATE etools)
```

---

## Quick Start

### Compile-time type utilities

```cpp
#include "etools/meta/traits.hpp"

// Smallest unsigned type that can hold a given value.
static_assert(std::is_same_v<etools::meta::smallest_uint_t<200>, std::uint8_t>);
static_assert(std::is_same_v<etools::meta::smallest_uint_t<70000>, std::uint32_t>);

// N-th type in a pack (O(1) with builtin, MI fallback otherwise).
using second = etools::meta::nth_t<1, int, double, float>;  // double

// Distinctness check for types.
static_assert(etools::meta::is_distinct_v<int, double, char>);
static_assert(!etools::meta::is_distinct_v<int, double, int>);
```

### Perfect hash lookup

```cpp
#include "etools/hashing/optimal_mph.hpp"

using MPH = etools::hashing::optimal_mph<std::uint16_t>;
constexpr const auto& H = MPH::instance<2, 5, 7, 8, 9>();

static_assert(H(7)   == 2);
static_assert(H(999) == H.not_found());   // not_found() == size() == 5
```

### In-place object with manual lifetime

```cpp
#include "etools/memory/slot.hpp"

struct Sensor { int reading; };

etools::memory::slot<Sensor> cell;
Sensor& s = cell.emplace(42);   // constructs Sensor{42} in cell's own storage
assert(cell.has_value());
assert((*cell).reading == 42);
cell.reset();                   // destroys the Sensor; cell is now empty
```

### Zero-allocation polymorphic factory

```cpp
#include "etools/factories/dispatch_factory.hpp"

struct Base { virtual ~Base() = default; };
struct Cat : Base { static constexpr uint8_t key = 1; };
struct Dog : Base { static constexpr uint8_t key = 2; };

template<class T> struct key_of { static constexpr auto value = T::key; };

using factory_t = etools::factories::dispatch_factory<Base, key_of, Cat, Dog>;
factory_t factory;

auto h = factory.emplace(Cat::key);  // handle_t; Cat lives in factory's own storage
// h is dropped here -> Cat is destroyed and the slot is freed
```

---

## Module: etools/meta

All meta utilities live in namespace `etools::meta`.

### traits.hpp

Provides custom type traits and small helpers that complement `<type_traits>`.

---

#### `is_distinct<Ts...>` / `is_distinct_v<Ts...>`

Compile-time check that all types in a parameter pack are pairwise distinct.

```cpp
static_assert( etools::meta::is_distinct_v<int, double, char>);
static_assert(!etools::meta::is_distinct_v<int, double, int>);
```

Implemented as a recursive fold over `std::is_same_v`; O(N^2) in the number of types.
For checking distinctness of *values* at scale, prefer `meta::all_distinct_fast` in
`utility.hpp`.

---

#### `underlying_v<T>(v)`

Returns the underlying integer value of an `enum` or `enum class`.

```cpp
enum class color : uint8_t { red = 0, green = 1, blue = 2 };
uint8_t n = etools::meta::underlying_v(color::green);   // 1
```

---

#### `always_false_v<T>`

A type-dependent `false` constant. Use this to trigger a `static_assert` only when a
specific `if constexpr` branch is instantiated, without triggering it unconditionally.

```cpp
template<typename T>
void process(T) {
    if constexpr (std::is_integral_v<T>) { /* ... */ }
    else static_assert(etools::meta::always_false_v<T>, "unsupported type");
}
```

---

#### `type_identity<T>` / `type_identity_t<T>`

Identity metafunction. Useful for preventing template argument deduction in specific
contexts. Matches `std::type_identity` (C++20) as a C++17 polyfill.

---

#### `nth<N, Ts...>` / `nth_t<N, Ts...>`

Retrieves the N-th type (zero-based) from a parameter pack. Produces a friendly
`static_assert` on out-of-bounds access.

Uses `__type_pack_element` (clang/GCC compiler builtin) when available for O(1)
instantiation depth; falls back to a multiple-inheritance O(1)-lookup portable
implementation otherwise.

```cpp
using T = etools::meta::nth_t<2, int, double, float, char>;  // float
```

---

#### `smallest_uint_t<V>`

Selects the smallest standard unsigned integer type (`uint8_t`, `uint16_t`, `uint32_t`,
or `uint64_t`) capable of representing the compile-time value `V`.

```cpp
etools::meta::smallest_uint_t<255>    // uint8_t
etools::meta::smallest_uint_t<256>    // uint16_t
etools::meta::smallest_uint_t<70000>  // uint32_t
```

Used internally by `dispatch_factory` to keep slot-index fields as narrow as possible.

---

#### `add_const_if<T, Condition>` / `add_const_if_t<T, Condition>`

Conditionally adds `const` to a type at compile time.

```cpp
etools::meta::add_const_if_t<int, true>   // const int
etools::meta::add_const_if_t<int, false>  // int
```

---

#### `member<Extractor, First, Rest...>` / `member_t<Extractor, Ts...>`

Extracts and validates the common type of a static member across multiple types using an
extractor metafunction. A `static_assert` fires if any types yield a different member type.

```cpp
template<typename T>
struct key_extractor { static constexpr auto value = T::key; };

struct A { static constexpr uint8_t key = 1; };
struct B { static constexpr uint8_t key = 2; };

using key_t = etools::meta::member_t<key_extractor, A, B>;  // uint8_t
```

---

### typelist.hpp

A zero-size, compile-time container for a sequence of types.

```cpp
#include "etools/meta/typelist.hpp"

using types = etools::meta::typelist<int, double, char>;
```

`typelist<Ts...>` carries no data and is never instantiated. It serves as a type-level
container compatible with partial specialization pattern matching:

```cpp
template<typename List> struct size_of;
template<typename... Ts>
struct size_of<etools::meta::typelist<Ts...>> {
    static constexpr std::size_t value = sizeof...(Ts);
};
static_assert(size_of<types>::value == 3);
```

`dispatch_factory` accepts a `typelist` directly as its third argument via a partial
specialization:

```cpp
using types = meta::typelist<Cat, Dog, Fish>;
using factory_t = dispatch_factory<Animal, key_of, types>;
```

---

### typeset.hpp

Associates a boolean flag with each type in a compile-time-fixed type set. Flags are
stored in a `std::bitset`, giving O(1) runtime access by type.

```cpp
#include "etools/meta/typeset.hpp"

etools::meta::typeset<int, double, char> flags;
flags.set<int>();
flags.set<char>();

assert( flags.test<int>());
assert(!flags.test<double>());
assert( flags.test<char>());

flags.reset<int>();
assert(!flags.test<int>());
```

All types in the pack must be distinct (enforced by `static_assert`).

**API summary:**

| Method | Description |
|--------|-------------|
| `test<T>() const noexcept` | Returns `true` iff the flag for `T` is set. |
| `set<T>() noexcept` | Sets the flag for `T`. |
| `reset<T>() noexcept` | Clears the flag for `T`. |

**Note:** The internal type-to-index mapping uses recursive template instantiation.
For very large type packs (hundreds of types), compile times may be noticeable.

---

### flags.hpp

Opt-in bitwise operators and flag-decomposition helpers for `enum class` bitmasks.

#### Opt-in mechanism

Specialize `enable_flags` immediately after your enum definition to enable all
bitwise operators for that enum:

```cpp
#include "etools/meta/flags.hpp"

enum class perms : std::uint8_t {
    none    = 0,
    read    = 1u << 0,
    write   = 1u << 1,
    execute = 1u << 2
};

template <>
struct etools::meta::enable_flags<perms> : std::true_type {};
```

The specialization must be visible in every translation unit that uses the operators.
Enums that have not opted in are completely unaffected.

#### Bitwise operators

Once opted-in, all standard bitwise operators are available:

```cpp
perms p = perms::read | perms::write;   // OR
p |= perms::execute;                    // compound OR
p &= ~perms::write;                     // compound AND + NOT
bool can_read = (p & perms::read) != perms{0};
```

Operators: `|`, `|=`, `&`, `&=`, `^`, `^=`, `~`.

Operations are performed on `std::underlying_type_t<Enum>` and re-cast to `Enum`.
Always declare flag enums with an **unsigned** underlying type to avoid undefined
behavior in `operator~`.

#### `enumerate_flags(value)`

Returns an `std::array<Enum, Size>` of length equal to the underlying type's bit width
(or a user-specified `Size`). Position `i` holds the single-bit enumerator `Enum(1<<i)`
if that bit is set, or `Enum(0)` otherwise. Set bits are **not** compacted; positions
are preserved.

```cpp
auto bits = etools::meta::enumerate_flags(perms::read | perms::execute);
for (std::size_t i = 0; i < bits.size(); ++i) {
    if (bits[i] != perms{0}) {
        // bit i was set
    }
}
```

#### `extract_flags(value)`

Returns `std::pair<std::array<Enum, Size>, std::size_t>`. The set flags are packed at
the front of the array in ascending bit order; `count` (the second element) is the
number of set flags. Trailing entries are `Enum(0)`.

```cpp
auto [flags, count] = etools::meta::extract_flags(perms::read | perms::execute);
for (std::size_t i = 0; i < count; ++i) {
    // flags[i] is perms::read or perms::execute
}
```

No allocation occurs; the array is sized at compile time from `Size`.

---

### info_gen.hpp

Preprocessor macros that generate SFINAE-based type traits for detecting members,
nested types, static members, or member variables by name. Each macro generates a
`struct` and an `inline constexpr bool` variable template in namespace `etools::meta`.

---

The macros form a complete detection matrix:

| Macro | Detects | Overload-safe |
|-------|---------|---------------|
| `generate_has_member` | any public member (variable, method, or static) | no |
| `generate_has_member_variable` | public non-static instance variable only | n/a |
| `generate_has_static_member_variable` | public static data member only | n/a |
| `generate_has_static_member` | any public static member (data or function) | no |
| `generate_has_static_method` | public static member function only | no |
| `generate_has_method` | public non-static member function only | no |
| `generate_has_callable` | any member callable with no arguments | yes |
| `generate_has_nested_type` | public nested type alias or class | n/a |

Macros that take the address of the member (`&T::name`) fail to compile when `name` is
overloaded. Use `generate_has_callable` in those cases.

---

#### `generate_has_member(name)`

Detects any public member named `name` (variable, method, or static member).

```cpp
generate_has_member(id)

struct A { int id; };
struct B { static int id; };
struct C { void id() {} };
struct D {};

static_assert( etools::meta::has_member_id_v<A>);
static_assert( etools::meta::has_member_id_v<B>);
static_assert( etools::meta::has_member_id_v<C>);
static_assert(!etools::meta::has_member_id_v<D>);
```

---

#### `generate_has_member_variable(name)`

Detects a public, non-static instance member variable named `name` only. Excludes
static members and methods.

```cpp
generate_has_member_variable(value)

struct A { int value; };
struct B { static int value; };
struct C { void value() {} };

static_assert( etools::meta::has_member_variable_value_v<A>);
static_assert(!etools::meta::has_member_variable_value_v<B>);
static_assert(!etools::meta::has_member_variable_value_v<C>);
```

---

#### `generate_has_static_member_variable(name)`

Detects a public static data member named `name`. Excludes static functions and
instance members.

```cpp
generate_has_static_member_variable(counter)

struct A { static int counter; };
struct B { int counter; };
struct C { static void counter(); };

static_assert( etools::meta::has_static_member_variable_counter_v<A>);
static_assert(!etools::meta::has_static_member_variable_counter_v<B>);
static_assert(!etools::meta::has_static_member_variable_counter_v<C>);
```

---

#### `generate_has_static_member(name)`

Detects any public static member (data or function) named `name`.

```cpp
generate_has_static_member(tag)

struct A { static const char* tag; };
struct B { static const char* tag(); };
struct C { const char* tag; };

static_assert( etools::meta::has_static_member_tag_v<A>);
static_assert( etools::meta::has_static_member_tag_v<B>);
static_assert(!etools::meta::has_static_member_tag_v<C>);
```

---

#### `generate_has_static_method(name)`

Detects a public static member **function** named `name` only. Excludes static data
members and instance methods. Complements `generate_has_static_member_variable` to give
precise control over which kind of static member is expected.

```cpp
generate_has_static_method(create)

struct A { static A create(); };
struct B { static int create; };   // static data, not a function
struct C { A create(); };          // non-static method

static_assert( etools::meta::has_static_method_create_v<A>);
static_assert(!etools::meta::has_static_method_create_v<B>);
static_assert(!etools::meta::has_static_method_create_v<C>);
```

Fails to compile when `create` is overloaded.

---

#### `generate_has_method(name)`

Detects a public non-static member function named `name`. Excludes instance variables,
static data members, and static functions.

```cpp
generate_has_method(init)

struct A { void init(); };
struct B { static void init(); };
struct C { int init; };
struct D {};

static_assert( etools::meta::has_method_init_v<A>);
static_assert(!etools::meta::has_method_init_v<B>);
static_assert(!etools::meta::has_method_init_v<C>);
static_assert(!etools::meta::has_method_init_v<D>);
```

Fails to compile when `init` is overloaded. Use `generate_has_callable` instead.

---

#### `generate_has_callable(name)`

Overload-safe detection: evaluates to `true` if the expression `t.name()` (where `t` is
an lvalue of type `T`) is well-formed. Works correctly when `name` has multiple overloads
as long as at least one accepts zero arguments.

```cpp
generate_has_callable(update)

struct A { void update(); };
struct B { void update(int); };              // no no-arg overload
struct C { void update(); void update(int); }; // overloaded, no-arg exists

static_assert( etools::meta::has_callable_update_v<A>);
static_assert(!etools::meta::has_callable_update_v<B>);
static_assert( etools::meta::has_callable_update_v<C>);
```

Does not distinguish static from non-static; tests callability via an instance only.

---

#### `generate_has_nested_type(name)`

Detects a public nested type alias or `struct`/`class` definition named `name`.

```cpp
generate_has_nested_type(value_type)

struct Container { using value_type = int; };
struct Other { double data; };

static_assert( etools::meta::has_nested_type_value_type_v<Container>);
static_assert(!etools::meta::has_nested_type_value_type_v<Other>);
```

---

### utility.hpp

Constexpr utility functions for pack operations and pairwise distinctness checking.

---

#### `tpack_max<T, First, Rest...>()`

Returns the maximum value among `{First, Rest...}` at compile time. Accepts integral
types and `enum`/`enum class` (comparison is done on the underlying type; the result
is returned as `T`).

```cpp
constexpr auto m = etools::meta::tpack_max<int, 3, 1, 7, 2>();  // 7
```

---

#### `all_distinct_fast(keys)` / `all_distinct_bitmap(keys)` / `all_distinct_probe(keys)`

Three `constexpr` functions that check whether all elements of a `std::array<T, N>`
are pairwise distinct:

- **`all_distinct_bitmap`**: for key types with at most 16 value bits (`uint8_t`,
  `uint16_t`). Allocates one bit per possible value during constant evaluation; O(N)
  with tiny constants.
- **`all_distinct_probe`**: for wider key types. Uses a compile-time open-addressed hash
  set with linear probing; O(N) with a larger constant.
- **`all_distinct_fast`**: the dispatch wrapper. Selects the bitmap approach for
  `std::numeric_limits<T>::digits <= 16`, otherwise the probe approach.

```cpp
constexpr std::array<uint8_t, 3> keys{1, 5, 3};
static_assert(etools::meta::all_distinct_fast(keys));

constexpr std::array<uint8_t, 3> dup{1, 5, 1};
static_assert(!etools::meta::all_distinct_fast(dup));
```

Used internally by `dispatch_factory` and the MPH backends to enforce key uniqueness at
compile time with a clear diagnostic.

---

### overload.hpp

Provides `overload<Fs...>`: the standard C++17 "overloaded visitor" idiom that merges
multiple callables into a single overload set. The primary use case is `std::visit` -
one lambda per variant alternative rather than a monolithic visitor.

```cpp
#include "etools/meta/overload.hpp"

std::variant<int, float, std::string> v = 3.14f;

std::visit(etools::meta::overload{
    [](int i)                { /* handle int    */ },
    [](float f)              { /* handle float  */ },
    [](const std::string& s) { /* handle string */ },
}, v);
```

The deduction guide makes explicit template arguments unnecessary; `overload{f1, f2}`
deduces `overload<F1, F2>` automatically.

**Constraints:**
- Every `Fs` must be copy- or move-constructible (stateful lambdas captured by value
  satisfy this trivially).
- If two callables accept the same argument types, the overload set is ill-formed and
  the compiler reports the conflict at the call site.

`overload` pairs naturally with `unique_variant_t` (see below): deduplicate the variant's
alternative list first, then visit with one lambda per unique alternative.

---

### unique_variant.hpp

Builds a `std::variant` (or intermediate `typelist`) over the **distinct** types in a
parameter pack, preserving first-occurrence order and silently dropping repeats.

#### Why this is necessary

`std::variant<int, int>` is well-formed, but two things immediately break once a type
appears more than once:

- **Converting construction** (`variant{value}`) becomes ambiguous when multiple
  alternatives could accept the value, forcing callers to use `std::in_place_index<N>`.
- **`std::visit` with `overload{...}`** is ill-formed outright: two lambdas taking the
  same type collide when their `operator()`s are merged via `using Fs::operator()...`.

The fix must happen at the type level, before the variant is formed. `unique_variant_t`
deduplicates the pack first so both usages remain unambiguous regardless of how many
times a type recurs.

#### Public API

| Alias | Result |
|-------|--------|
| `unique_typelist_t<Ts...>` | `typelist</* distinct Ts in first-occurrence order */>` |
| `unique_variant_t<Ts...>` | `std::variant</* distinct Ts in first-occurrence order */>` |

```cpp
#include "etools/meta/unique_variant.hpp"
#include "etools/meta/overload.hpp"

struct producer_a { using payload_t = int;    };
struct producer_b { using payload_t = double; };
struct producer_c { using payload_t = int;    };  // same as producer_a

using payload_t = etools::meta::unique_variant_t<
    producer_a::payload_t,
    producer_b::payload_t,
    producer_c::payload_t
>;
static_assert(std::is_same_v<payload_t, std::variant<int, double>>);

payload_t p{3.14};
std::visit(etools::meta::overload{
    [](int i)    { /* ... */ },
    [](double d) { /* ... */ },
}, p);
```

Use `unique_typelist_t` when the deduplicated pack needs to feed something other than
`std::variant` (a dispatch table, another tagged-union type, further metaprogramming).
Use `is_distinct_v` (`traits.hpp`) instead when duplicates should be a compile error
rather than silently collapsed.

---

### sort.hpp

Stable compile-time sort of a parameter pack using a caller-supplied binary comparator
metafunction. Produces a `typelist` in the requested order.

#### Comparator contract

`Cmp` is a template of the form:

```cpp
template<typename T, typename U>
struct MyCmp : std::bool_constant</* true if T should come before U */> {};
```

It must satisfy strict weak ordering (irreflexive, asymmetric, transitive). Types for
which neither `Cmp<T,U>` nor `Cmp<U,T>` holds are considered equivalent; their
relative order in the output matches their input order (the sort is **stable**).

#### Built-in comparators

| Comparator | Ordering |
|------------|----------|
| `size_greater<T,U>` | Largest `sizeof` first |
| `size_less<T,U>` | Smallest `sizeof` first |
| `align_greater<T,U>` | Strictest `alignof` first |
| `align_less<T,U>` | Weakest `alignof` first |

#### API

| Alias / struct | Result |
|----------------|--------|
| `sort_t<Cmp, Ts...>` | `typelist</* Ts sorted by Cmp */>` |
| `sort_t<Cmp, typelist<Ts...>>` | Same, accepting a `typelist` directly |
| `sort<Cmp, Ts...>::type` | Struct form of the above |

```cpp
#include "etools/meta/sort.hpp"
using namespace etools::meta;

// Sort by descending size - useful for padding-free struct layout.
using sorted = sort_t<size_greater, char, double, int, float>;
static_assert(std::is_same_v<sorted, typelist<double, int, float, char>>);

// Accepts a typelist directly.
using in_list = typelist<char, double, int, float>;
static_assert(std::is_same_v<sort_t<size_greater, in_list>, sorted>);

// Custom comparator: trivial types before non-trivial.
template<typename T, typename U>
struct prefer_trivial : std::bool_constant<
    std::is_trivial_v<T> && !std::is_trivial_v<U>
> {};

using sorted2 = sort_t<prefer_trivial, std::string, int, std::vector<int>, float>;
// int and float precede std::string and std::vector<int>
```

The algorithm is top-down merge sort: O(N log N) instantiation depth and O(log N)
maximum recursion depth. The pack is split into two halves by distributing elements
alternately (riffle split), each half is sorted recursively, and the sorted halves
are merged. The merge step favours the left half on equivalence, which is what
preserves stability.

---

## Module: etools/hashing

All hashing utilities live in namespace `etools::hashing`.

### utils (hashing)

`etools/hashing/utils.hpp` provides a set of `constexpr` functions used internally by
the MPH backends. These are also available for direct use.

#### Integer mixers

| Function | Width | Algorithm |
|----------|-------|-----------|
| `mix_u8(x)` | 8-bit | xorshift-multiply cascade |
| `mix_u16(x)` | 16-bit | xorshift-multiply cascade |
| `mix_u32(x)` | 32-bit | MurmurHash3 fmix32 |
| `mix_u64(x)` | 64-bit | SplitMix64 finalizer |

None of these are cryptographic. They are fast, branch-free bit diffusers suitable for
bucket selection and multiply-shift second-level hashing.

#### `mix_width<T, Key>(key)`

Mixes a key using the mixer for a specific target width `T`. Useful when you want more
mixed bits than the key's native width provides.

```cpp
std::size_t h = etools::hashing::mix_width<std::uint64_t>(my_uint8_key);
```

#### `mix_native<Key>(key)`

Mixes using the mixer that matches the target machine's `sizeof(std::size_t)`. On a
32-bit MCU this calls `mix_u32`; on a 64-bit host it calls `mix_u64`. Prefer
`mix_width` when you need deterministic cross-platform behavior.

#### `ceil_pow2(x)` / `ceil_pow2_saturate(x)`

Next power of two greater than or equal to `x`. `ceil_pow2` wraps on overflow;
`ceil_pow2_saturate` clamps to the largest representable power of two.

```cpp
etools::hashing::ceil_pow2<uint32_t>(5)   // 8
etools::hashing::ceil_pow2<uint32_t>(8)   // 8
etools::hashing::ceil_pow2<uint32_t>(0)   // 1
```

#### `bit_width<R>(x)` / `ceil_log2<R>(x)`

`bit_width<R>(x)` returns `floor(log2(x)) + 1` (0 for x=0). `ceil_log2<R>(x)` returns
the smallest r such that `2^r >= x` (0 for x<=1).

#### `bucket_of<Key>(k, bucket_count)`

Maps a key to a bucket index in `[0, bucket_count)` using `mix_native` and a power-of-
two mask. `bucket_count` must be a power of two.

#### `top_bits<T>(x, r)`

Extracts the `r` most-significant bits of `x` as a `std::size_t`, right-aligned.

---

### llut.hpp

**Light Look-Up Table**: a direct-address MPH backend for compact key sets.

`llut<Key>` produces a table of length `max(Keys) + 1`. Each key maps to its dense
index (by pack order) and absent positions hold a sentinel equal to `N = sizeof...(Keys)`.
Lookup is a single array subscript - O(1) with minimal overhead.

```cpp
#include "etools/hashing/llut.hpp"

using T = etools::hashing::llut<std::uint8_t>;
constexpr const auto& table = T::instance<2, 5, 7>();

static_assert(table.size()     == 3);
static_assert(table.capacity() == 8);    // max key = 7 -> array length = 8
static_assert(table(2) == 0);
static_assert(table(5) == 1);
static_assert(table(7) == 2);
static_assert(table(0) == table.not_found());
static_assert(table(9) == table.not_found());   // out of range -> sentinel
```

**API:**

| Member | Description |
|--------|-------------|
| `instance<Keys...>()` (static) | Returns `constexpr const&` to the per-key-set singleton. |
| `size()` (static) | Number of keys in the set (`N`). Also the sentinel value. |
| `capacity()` (static) | Length of the backing array (`max(Keys) + 1`). |
| `not_found()` (static) | Sentinel equal to `size()`. |
| `operator()(key)` | O(1) lookup. Returns an index in `[0..N-1]` or `not_found()`. |

The backing array type uses `meta::smallest_uint_t<N>` for compact storage. The
singleton has `static` storage duration and is ODR-merged across translation units.

**When to use LLUT directly:** when you know the key span is compact (e.g., small
contiguous command IDs). For sparse or wide keys, prefer `fks` or let `optimal_mph`
decide.

---

### fks.hpp

**Fredman-Komlos-Szemeredi perfect hashing**: a two-level MPH backend for sparse or
wide key sets. Given N distinct keys, builds a structure with O(N) total slots (constant
factor roughly 3) and O(1) lookup with no collisions.

**Algorithm sketch:**
1. First-level: keys are bucketed by `mix_native(key) & (M - 1)` where `M = ceil_pow2(N)`.
2. Second-level (per bucket): a per-bucket table of size `S_b = ceil_pow2(s_b^2)` is
   built (where `s_b` is the bucket size); an odd multiplier `a_b` is chosen so that
   the multiply-shift hash places all `s_b` keys without collision.
3. Lookup: compute bucket, apply per-bucket hash, index into the flat slot array, then
   verify the stored key for membership.

```cpp
#include "etools/hashing/fks.hpp"

using F = etools::hashing::fks<std::uint64_t>;
constexpr const auto& H = F::instance<1ULL, 5ULL, 2ULL, 10ULL, 7ULL>();

static_assert(H.size() == 5);
static_assert(H(1)  == 0);
static_assert(H(5)  == 1);
static_assert(H(2)  == 2);
static_assert(H(10) == 3);
static_assert(H(7)  == 4);
static_assert(H(999) == H.not_found());
```

**Runtime pattern:**

```cpp
std::size_t idx = H(my_key);
if (idx < H.size()) {
    // found at dense index idx
} else {
    // not a member
}
```

**API:**

| Member | Description |
|--------|-------------|
| `instance<Keys...>()` (static) | Returns `constexpr const&` to the per-key-set singleton. |
| `size()` (static) | Number of keys (`N`). |
| `capacity()` (static) | Total second-level slot count (sum of all `S_b`). |
| `buckets()` (static) | First-level bucket count (`ceil_pow2(N)`, always a power of two). |
| `not_found()` (static) | Sentinel equal to `size()`. |
| `operator()(key)` | O(1) lookup. Returns an index in `[0..N-1]` or `not_found()`. |

Key distinctness is enforced at construction time via `meta::all_distinct_fast`. The
entire structure is built as a `constexpr` constructor body; no runtime initialization
occurs.

**When to use FKS directly:** when you know the key domain is wide or sparse (e.g.,
CAN message IDs, arbitrary protocol opcodes). For compact keys, `llut` may be faster
and smaller. Use `optimal_mph` to have the selection made automatically.

---

### optimal_mph.hpp

**Automatic MPH backend selector**: compares compile-time memory models for LLUT and
FKS over the given key set and returns a `constexpr const&` to the cheaper backend's
singleton.

**Selection heuristic (pure integer math, evaluated at compile time):**

Let N = number of keys, K = max(Keys) + 1, T_idx = `smallest_uint_t<N>`, alpha = 3:

- LLUT memory estimate: `K * sizeof(T_idx)`
- FKS memory estimate: `N * (alpha * sizeof(T_idx) + 2 * sizeof(size_t) + 1 + sizeof(Key))`

If LLUT estimate exceeds FKS estimate, FKS is selected; otherwise LLUT.

```cpp
#include "etools/hashing/optimal_mph.hpp"

using Opt = etools::hashing::optimal_mph<std::uint16_t>;

// Dense-ish set: LLUT is likely chosen.
constexpr const auto& A = Opt::instance<2, 5, 7, 8, 9>();
static_assert(A.size() == 5);
static_assert(A(7)   == 2);
static_assert(A(999) == A.not_found());

// Sparse set: FKS is likely chosen.
constexpr const auto& B = Opt::instance<1, 10000, 60000>();
static_assert(B(60000) == 2);
```

The exact return type is an unspecified backend type (either `llut_impl<...>` or
`fks_impl<...>`); rely only on the cross-backend contract: `operator()`, `size()`,
`capacity()`, `not_found()`.

`dispatch_factory` uses `optimal_mph` internally to build its key-to-index map.

---

## Module: etools/memory

All memory utilities live in namespace `etools::memory`.

### slot.hpp

**`slot<T>`** is a value type providing manually controlled lifetime for exactly one `T`,
stored in-place in aligned local storage. Think `std::optional<T>` with a different move
contract and a nothrow-destructible constraint.

```cpp
#include "etools/memory/slot.hpp"

struct Motor {
    int speed;
    explicit Motor(int s) : speed(s) {}
    ~Motor() noexcept = default;
};

etools::memory::slot<Motor> cell;
Motor& m = cell.emplace(1000);   // constructs Motor{1000} in cell's storage
assert(cell.has_value());
assert(cell->speed == 1000);
cell.reset();                    // destroys Motor; slot is empty again
                                 // ~slot() would also call reset() automatically
```

**Key characteristics:**

- **In-place storage.** `sizeof(slot<T>) == sizeof(T) + alignment padding + 1 (flag)`.
  No heap allocation ever.
- **Value type.** Each `slot<T>` owns independent storage; there is no shared state.
- **Non-copyable.** A raw-storage cell is not generally copyable.
- **Movable iff `T` is move-constructible.** A moved-from `slot` is left **empty** (not
  engaged-but-moved-from like `std::optional`). `std::is_move_constructible_v<slot<T>>`
  reports the truth.
- **RAII destructor.** The slot destructor calls `reset()` automatically.
- **`T` must be nothrow-destructible.** Enforced by `static_assert`; a throwing
  destructor in a noexcept path cannot be handled safely.

**API:**

| Member | Description |
|--------|-------------|
| `emplace(args...)` | Destroys any existing `T`, constructs a new one in-place. Returns `T&`. `noexcept` iff `T(args...)` is `noexcept`. |
| `reset()` | Destroys the contained `T` if present. Idempotent. Always `noexcept`. |
| `has_value() const` | Returns `true` iff the slot is engaged. |
| `operator bool()` (explicit) | Equivalent to `has_value()`. |
| `operator*()` | Returns `T&`/`const T&`. Asserts in debug if empty. |
| `operator->()` | Returns `T*`/`const T*`. Asserts in debug if empty. |

`slot<T&>` and `slot<T&&>` are deleted with a clear diagnostic.

> **v1.0.0 note:** `slot<T>` is the blueprint for a planned fixed-capacity memory pool.
> The standalone `slot` carries a `_constructed` flag because it manages occupancy itself;
> a future pool will track occupancy externally and will not need this flag. The
> relocate-on-move semantics and nothrow-dtor constraint are intentionally shaped toward
> that future pool-cell contract. The pool itself is not yet implemented.

---

### buffer.hpp

**`buffer<Deleter>`** is a move-only owning container for a heap (or pool) allocated
byte array, with serialization and deserialization via eser.

```cpp
#include "etools/memory/buffer.hpp"

// Heap-backed buffer.
auto raw = std::make_unique<std::byte[]>(256);
etools::memory::buffer env(std::move(raw), 256);
env.pack(42u, 3.14f);          // serializes via eser::flat::serialize

// Unpack.
if (auto t = env.unpack<uint32_t, float>()) {
    auto [n, f] = *t;
}

// Static memory, no deallocation.
static std::byte buf[64];
auto noop = [](std::byte*) {};
etools::memory::buffer<decltype(noop)> static_env(
    std::unique_ptr<std::byte[], decltype(noop)>{buf, noop}, 64);
```

**Constructors:**

| Constructor | Use case |
|-------------|----------|
| `buffer()` | Default: null, zero capacity. Only for Deleter-default-constructible types. |
| `buffer(data, capacity)` | Owning empty buffer ready for packing. |
| `buffer(data, capacity, size)` | Owning pre-filled buffer ready for unpacking. |

**API:**

| Member | Description |
|--------|-------------|
| `pack(args...)` | Serializes `args...` into the buffer. Returns bytes written (0 if too small). Overwrites previous content. |
| `unpack<Ts...>()` | Deserializes to `std::optional<std::tuple<Ts...>>`. Returns `nullopt` if too short. |
| `data()` | `const std::byte*` to the start of the block. |
| `size()` | Bytes used (set by the last `pack` call or the constructor). |
| `capacity()` | Total bytes in the block. |

`buffer` is non-copyable. Move transfers ownership; the moved-from buffer becomes null.

**Dependency:** `pack` and `unpack` call into `eser::flat::serialize` /
`eser::flat::deserialize`. **eser must be available** on the include path.

---

### buffer_view.hpp

**`buffer_view`** is a non-owning, read-only view over a serialized byte range.
Analogous to `std::string_view` for byte buffers.

```cpp
#include "etools/memory/buffer_view.hpp"

// Typically constructed from a buffer or received byte span.
const std::byte* ptr = ...;
std::size_t len = ...;
etools::memory::buffer_view view(ptr, len);

if (auto t = view.unpack<uint32_t, float>()) {
    auto [id, temp] = *t;
}
```

`buffer_view` is trivially copyable, trivially movable, and trivially destructible. It
stores only a pointer and a size. The lifetime of the viewed memory must exceed the
lifetime of the view.

**API:**

| Member | Description |
|--------|-------------|
| `buffer_view(data, size)` | Constructs from raw pointer and byte count. `noexcept`. |
| `unpack<Ts...>()` | Same semantics as `buffer::unpack`. Returns `nullopt` if too short. |
| `data()` | `const std::byte*` to the viewed range. |
| `size()` | Byte count of the viewed range. |

**Dependency:** `unpack` uses `eser::flat::deserialize`.

---

## Module: etools/factories

All factory types live in namespace `etools::factories`. The capacity helper lives in
`etools::factories::utils`.

### capacity.hpp

`utils::capacity<T, N>` is a registration tag pairing a derived type `T` with a slot
count `N`. It is used with `dispatch_factory` to declare how many concurrent instances
of a given type the factory should support.

```cpp
#include "etools/factories/utils/capacity.hpp"

using etools::factories::utils::capacity;

capacity<Dog, 3>   // Dog can have 3 simultaneous live instances
capacity<Cat, 1>   // Cat gets 1 slot (same as passing Cat directly)
```

The struct exposes:

| Member | Value |
|--------|-------|
| `type` | The registered derived type `T`. |
| `count` | The slot count `N`. |

Bare types passed directly to `dispatch_factory` (without wrapping in `capacity`) are
automatically normalized to `capacity<T, 1>` via an internal `as_capacity_t` alias.
Bare types and explicit `capacity<T, N>` tags may be mixed freely in the same factory.

---

### dispatch_factory.hpp

**`dispatch_factory<Base, Extractor, Regs...>`** is the centerpiece of the factories
module. It is a zero-allocation, compile-time registry that constructs one of several
registered derived types by a runtime key, using an optimal perfect hash for the lookup.

Objects live in the factory's own in-place storage (a `std::tuple` of `std::array<std::optional<T>, N>` slices, one per registered type). There is no heap involvement.

#### Template parameters

| Parameter | Description |
|-----------|-------------|
| `Base` | Polymorphic base type. All registered types must derive from it. |
| `Extractor` | `template<class T> struct Extractor { static constexpr auto value = T::key; };` A metafunction that extracts a unique compile-time key from each derived type. |
| `Regs...` | Registration arguments: bare `DerivedType` (treated as `capacity<DerivedType, 1>`) or explicit `capacity<DerivedType, N>`. May be freely mixed. |

#### Registering types

```cpp
#include "etools/factories/dispatch_factory.hpp"
#include "etools/factories/utils/capacity.hpp"

struct Base { virtual ~Base() = default; };

struct Cat : Base {
    static constexpr uint8_t key = 1;
    int hunger{};
    explicit Cat(int h) : hunger(h) {}
};

struct Dog : Base {
    static constexpr uint8_t key = 2;
    int energy{};
    explicit Dog(int e) : energy(e) {}
};

struct Fish : Base {
    static constexpr uint8_t key = 3;
};

template<class T>
struct key_of { static constexpr auto value = T::key; };

using namespace etools::factories::utils;

// Cat: 1 slot. Dog: 3 concurrent slots. Fish: 1 slot (bare type).
using factory_t = etools::factories::dispatch_factory<
    Base,
    key_of,
    capacity<Cat, 1>,
    capacity<Dog, 3>,
    Fish
>;
```

#### `emplace`

```cpp
factory_t factory;

auto hcat = factory.emplace(Cat::key, 5);   // Cat{5}, handle_t (unique_ptr<Base, cell_deleter>)
auto hd0  = factory.emplace(Dog::key, 100); // slot 0 of Dog
auto hd1  = factory.emplace(Dog::key, 200); // slot 1 of Dog
auto hd2  = factory.emplace(Dog::key, 300); // slot 2 of Dog
auto hd3  = factory.emplace(Dog::key, 400); // null handle: all 3 Dog slots occupied

hd0.reset();                                // frees Dog slot 0
auto hd0b = factory.emplace(Dog::key, 150); // succeeds now
```

`emplace` returns a `handle_t = std::unique_ptr<Base, cell_deleter>`. The handle is
**empty** (`get() == nullptr`) when:
- the key is not registered,
- no registered type is constructible from the given arguments, or
- all slots for the matching type are occupied.

Dropping a non-empty handle (or calling `.reset()` on it) destroys the object in its
slot and makes it available for reuse.

`emplace` is `[[nodiscard]]` and `noexcept` iff every registered type that is
constructible from `Args...` is also nothrow-constructible from those arguments.

#### Typelist adapter

For large registries, types can be listed in a `meta::typelist` rather than expanded
directly into the factory template arguments:

```cpp
using animals = meta::typelist<Cat, capacity<Dog, 3>, Fish>;
using factory_t = etools::factories::dispatch_factory<Base, key_of, animals>;
// Equivalent to the expanded form above.
```

Bare types and `capacity<T, N>` tags may be freely mixed inside the typelist.

#### Compile-time contracts

The following are checked via `static_assert` at instantiation time:

- At least one type must be registered.
- Every registered type must be `std::is_base_of<Base, T>`.
- All registered types must expose the same key type through `Extractor`.
- All slot counts must be `> 0`.
- No registered type may be abstract.
- All registered types must be nothrow-destructible.
- All keys must be pairwise distinct (checked with `meta::all_distinct_fast`, O(N)).

#### Runtime behavior

Key-to-index lookup is O(1) via the `optimal_mph` perfect hash built at compile time
from the extracted keys. No hash table is initialized at runtime; the table is a
`constexpr` singleton.

For each `emplace(key, args...)` call, the factory:
1. Looks up the dense type index (O(1) MPH lookup).
2. Dispatches to the corresponding slot array via a fold expression.
3. Scans the slot array linearly for the first unoccupied `std::optional` cell (O(N) in
   the per-type slot count, typically 1-4).
4. Emplaces the object and returns a handle wrapping a `cell_deleter`.

Total cost: one O(1) hash lookup + a short linear scan of at most `N` optional cells.

#### Pinned type

`dispatch_factory` is a **pinned** type: copy and move are both deleted. The factory
owns live objects in-place; relocating it is not supported. Hold the factory by
reference, as a `static` local, or on the stack of the owner's scope.

#### Lifetime contract

Handles must not outlive the factory. The `cell_deleter` holds a raw pointer to the
factory; dereferencing it after the factory is destroyed is undefined behavior. In debug
builds, the factory's destructor `assert`s that all slots are empty when it is
destroyed, catching missing handle lifetimes early.

#### Compile-time notes

The dispatch fold expression is instantiated once per distinct `Args...` pack. For large
registries with many distinct constructor signatures, compile times grow as O(N * K)
where N is the type count and K is the number of distinct argument signatures. Runtime
performance is unaffected.

---

## Limitations

- **No thread safety.** `emplace` mutates the factory's slot arrays. Use one factory per
  thread or synchronize externally.
- **No exceptions.** The library does not use or require C++ exceptions. `emplace` may
  return an empty handle instead of throwing. Debug assertions are the only safety net
  for programming errors.
- **No heap allocation on hot paths.** `dispatch_factory` and `slot` are explicitly
  no-heap. `buffer` owns a heap block but that block is allocated once by the caller
  and then managed without further allocation.
- **Unsigned key types only.** The MPH backends (`llut`, `fks`) require
  `std::is_unsigned_v<KeyType>`. Signed or enum keys must be cast or extracted as
  unsigned values.
- **`buffer` / `buffer_view` require eser.** The `pack` / `unpack` methods call into
  the eser library. If you only need `slot` or the hashing/meta utilities, eser is not
  required.
- **`slot<T>` requires nothrow-destructible `T`.** Destruction runs in `noexcept`
  contexts; a throwing destructor cannot be handled correctly.
- **Pinned factory.** `dispatch_factory` cannot be moved or copied. It cannot be stored
  in standard containers that require movability (e.g., `std::vector`).

---

## Testing

Tests live under `tests/`. The test suite uses Catch2 and covers:

- `tests/factories/test_dispatch_factory.cpp` - 58 tests across 7 suites:
  - Basic construction and key lookup.
  - Multi-slot capacity, slot exhaustion, and reuse.
  - Mixed `capacity<T, N>` and bare-type registrations.
  - Typelist adapter specialization.
  - Noexcept propagation.
  - Handle lifetime and destructor assertion.
  - Edge cases: zero-argument construction, overfilling, partial reset, memory integrity
    across fill-drain cycles.

Build and run tests with CMake:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build
```

---

## Project Layout

```
etools/
  etools.hpp                  # Umbrella include for all modules

  meta/
    meta.hpp                  # Module umbrella
    traits.hpp                # Type traits (nth_t, smallest_uint_t, is_distinct, ...)
    typelist.hpp              # typelist<Ts...>
    typeset.hpp               # typeset<Ts...> - bitset-backed per-type flags
    flags.hpp                 # Bitwise operators for opted-in enum class bitmasks
    info_gen.hpp              # Introspection macros (generate_has_member, ...)
    overload.hpp              # overload<Fs...> - merged callable overload set for std::visit
    sort.hpp                  # sort_t<Cmp, Ts...> - stable O(N log N) compile-time sort by comparator
    unique_variant.hpp        # unique_variant_t / unique_typelist_t - deduplicated variant
    utility.hpp               # tpack_max, all_distinct_fast, ...

  hashing/
    hashing.hpp               # Module umbrella
    utils.hpp                 # mix_u*/mix_native, ceil_pow2, top_bits, ...
    llut.hpp                  # Direct-address MPH backend
    fks.hpp                   # Two-level FKS perfect hash backend
    optimal_mph.hpp           # Backend selector facade

  memory/
    memory.hpp                # Module umbrella
    slot.hpp                  # slot<T> - in-place value with manual lifetime
    buffer.hpp                # buffer<Deleter> - owning byte buffer with eser integration
    buffer_view.hpp           # buffer_view - non-owning read-only byte view

  factories/
    factories.hpp             # Module umbrella
    dispatch_factory.hpp      # dispatch_factory<Base, Extractor, Regs...>
    utils/
      capacity.hpp            # capacity<T, N> registration tag

tests/
  factories/
    test_dispatch_factory.cpp

example/
  eserREADME.md               # Style reference for elib documentation

tools/                        # Build and development scripts
```

---

## License

MIT License. Copyright (c) 2025 Mark Tikhonov.

You are free to use, modify, and distribute this software for any purpose, provided the
original copyright notice and license text are retained. See [LICENSE](LICENSE) for the
full text.
