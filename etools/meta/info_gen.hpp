// SPDX-License-Identifier: MIT
/**
* @file info_gen.hpp
*
* @brief Compile-time trait generators for detecting members, methods, and types in C++ classes.
*
* @ingroup etools_meta etool
*
* This header defines a set of macros that generate SFINAE-based type traits for detecting:
* - Member variables
* - Nested types
* - Member functions (by name and signature)
* - Static methods
*
* These utilities are designed for use in template metaprogramming and conditional compilation
* where adaptability based on type structure is required.
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
#ifndef ETOOLS_INFO_GEN_HPP_
#define ETOOLS_INFO_GEN_HPP_
#include <type_traits>
#include <cstddef>

/**
* @brief Generates a type trait to detect the presence of a specific member in a class.
*
* This macro defines a template struct `has_member_<member>` that evaluates to `true` if
* the type `T` has a member named `<member>`, and `false` otherwise.
* It also defines a helper variable template `has_member_<member>_v`.
*
* @param member The name of the member to check for existence.
*
* @note The generated struct can be used in compile-time checks (e.g., `if constexpr`, `static_assert`)
* to conditionally enable or disable code based on the presence of a member in a type.
* 
* @note <member> means any qualified name in a type T besides typedefs, so this metafunction
* checks for member variables, static member variables, instance methods, static methods.
*
* @warning This macro uses SFINAE based on taking the address of the member (`&T::member`).
* It may not correctly detect static members or members that cannot have their address taken.
* It generally requires the member to be accessible from the context where the trait is used
* i.e. be `public` accessible.
*
* @example
* // Generate the trait for a member named 'id'
* generate_has_member(id)
*
* struct MyStructWithId { int id; };
* struct MyStructWithoutId { double value; };
*
* static_assert(etools::meta::has_member_id_v<MyStructWithId>, "MyStructWithId should have member 'id'"); // will compile
* static_assert(!etools::meta::has_member_id_v<MyStructWithoutId>, "MyStructWithoutId should not have member 'id'"); // won't compile
*/
#define generate_has_member(member)                                                    \
namespace etools::meta {                                                               \
    template <typename T, typename = void>                                             \
    struct has_member_##member : std::false_type {};                                   \
                                                                                       \
    template <typename T>                                                              \
    struct has_member_##member<T, std::void_t<decltype(&T::member)>>                   \
        : std::true_type {};                                                           \
                                                                                       \
    template <typename T>                                                              \
    inline constexpr bool has_member_##member##_v = has_member_##member<T>::value;     \
                                                                                       \
} // etools::meta                                                                                     

/**
* @brief Generates a type trait to detect the presence of a specific *instance member variable* in a class.
*
* This macro defines a template struct `has_member_variable_<member>` that evaluates to `true`
* if the type `T` has a *non-static, accessible instance member variable* named `<member>`, and `false` otherwise.
*
* It also defines a helper variable template `has_member_variable_<member>_v`.
*
* Unlike `generate_has_member`, this variant excludes detection of static members and member functions.
*
* It uses `decltype(&T::member)` and SFINAE to detect whether the member is a **non-static data member**
* (i.e., a true instance variable, not a method or static field), and ensures it is publicly accessible.
*
* @param member The name of the member variable to check for existence.
*
* @note This trait is SFINAE-friendly and can be used in `if constexpr`, `static_assert`, etc.
* It requires the member to be accessible in the current context (e.g., not private or protected).
*
* @warning This macro will:
* - Not detect static members
* - Not detect methods (member functions)
* - Not detect inherited or nested members unless directly declared in `T`
* - Not detect inaccessible (private/protected) members from the current scope
*
* @example
* generate_has_member_variable(id)
*
* struct A { int id; };
* struct B { static int id; };
* struct C { private: int id; };
* struct D {};
*
* static_assert(etools::meta::has_member_variable_id_v<A>, "A has public instance variable id");
* static_assert(!etools::meta::has_member_variable_id_v<B>, "B has static member id");          
* static_assert(!etools::meta::has_member_variable_id_v<C>, "C has private member id");         
* static_assert(!etools::meta::has_member_variable_id_v<D>, "D has no id");                     
*/
#define generate_has_member_variable(var)                                              \
namespace etools::meta {                                                               \
    template <typename T, typename = void>                                             \
    struct has_member_variable_##var : std::false_type {};                             \
                                                                                       \
    template <typename T>                                                              \
    struct has_member_variable_##var<                                                  \
        T, std::enable_if_t<                                                           \
            std::is_member_object_pointer_v<decltype(&T::var)>                         \
        >                                                                              \
    > : std::true_type {};                                                             \
                                                                                       \
    template <typename T>                                                              \
    inline constexpr bool has_member_variable_##var##_v =                              \
        has_member_variable_##var<T>::value;                                           \
}

/**
* @brief Generates a type trait to detect the presence of a specific *static member variable* in a class.
*
* This macro defines a template struct `has_static_member_<member>` that evaluates to `true`
* if the type `T` has a *public, static member variable* named `<member>`, and `false` otherwise.
*
* It also defines a helper variable template `has_static_member_<member>_v`.
*
* It uses `decltype(&T::member)` and SFINAE to check whether the member exists and is a static **data** member.
*
* @param member The name of the static member variable to check for existence.
*
* @note This trait:
* - Ignores instance variables (non-static)
* - Ignores static or non-static functions
* - Requires the member to be publicly accessible
*
* @example
* generate_has_static_member(counter)
*
* struct A { static int counter; };
* struct B { int counter; };
* struct C { static void counter(); };
*
* static_assert(etools::meta::has_static_member_counter_v<A>, "A has static member variable");
* static_assert(!etools::meta::has_static_member_counter_v<B>, "B has instance variable");       
* static_assert(!etools::meta::has_static_member_counter_v<C>, "C has static function");        
*/
#define generate_has_static_member_variable(var)                                                     \
namespace etools::meta {                                                                             \
    template <typename T, typename = void>                                                           \
    struct has_static_member_variable_##var : std::false_type {};                                    \
                                                                                                     \
    template <typename T>                                                                            \
    struct has_static_member_variable_##var<                                                         \
        T, std::enable_if_t<                                                                         \
            std::is_member_pointer_v<decltype(&T::var)> == false &&                                  \
            std::is_function_v<std::remove_pointer_t<decltype(&T::var)>> == false                    \
        >                                                                                            \
    > : std::true_type {};                                                                           \
                                                                                                     \
    template <typename T>                                                                            \
    inline constexpr bool has_static_member_variable_##var##_v =                                     \
        has_static_member_variable_##var<T>::value;                                                  \
}


/**
* @brief Generates a type trait to detect the presence of a public static member in a class.
*
* This macro defines a template struct `has_static_member_<member>` that evaluates to `true` if
* the type `T` has a public static member named `<member>`, and `false` otherwise. This includes
* both static member variables and static member functions.
* It also defines a helper variable template `has_static_member_<member>_v`.
*
* @param member The name of the member (variable or function) to check for existence.
*
* @note The underlying mechanism uses SFINAE to check if the member's address can be taken and
* if it is NOT an instance member pointer.
*
* @warning This macro will only detect public static members. Private or protected static members
* will not be detected from outside the class context.
*
* @example
* // Generate the trait for a member named 'static_foo'
* generate_has_static_member(static_foo)
*
* struct MyStructWithStatic { static int static_foo; };
* struct MyStructWithoutStatic { int static_foo; };
*
* static_assert(etools::meta::has_static_member_static_foo_v<MyStructWithStatic>); // true
* static_assert(!etools::meta::has_static_member_static_foo_v<MyStructWithoutStatic>); // false
*/
#define generate_has_static_member(member)                                                  \
namespace etools::meta {                                                                    \
    template <typename T, typename = void>                                                  \
    struct has_static_member_##member : std::false_type {};                                 \
                                                                                            \
    template <typename T>                                                                   \
    struct has_static_member_##member<                                                      \
        T,                                                                                  \
        std::void_t<decltype(&T::member)>>                                                  \
    {                                                                                       \
        static constexpr bool value = !std::is_member_pointer_v<decltype(&T::member)>;      \
    };                                                                                      \
                                                                                            \
    template <typename T>                                                                   \
    inline constexpr bool has_static_member_##member##_v =                                  \
        has_static_member_##member<T>::value;                                               \
}


/**
* @brief Generates a type trait to detect the presence of a specific nested type alias or nested_type in a class.
*
* This macro defines a template struct `has_nested_type_<nested_type>` that evaluates to `true` if
* `typename T::<nested_type>` is a valid nested type name within class `T`, and `false` otherwise.
* It also defines a helper variable template `has_nested_type_<nested_type>_v`.
*
* @param nested_type The name of the nested type (e.g., alias created with `using` or `nested_type`) to check for existence.
*
* @note The generated struct/variable can be used in compile-time checks (e.g., `if constexpr`, `static_assert`)
* to conditionally enable or disable code based on the presence of a nested type definition in a class.
* This is useful for checking conventions or enabling specific template logic.
*
* @warning Requires C++17 for `std::void_t` (though often available in C++11/14 via library implementations).
* The nested type must be accessible from the context where the trait is used (typically public).
*
* @example
* // Generate the trait for a nested type named 'value_type'
* generate_has_nested_type(value_type)
*
* struct Container { using value_type = int; };
* struct SimpleClass { double data; };
*
* static_assert(has_nested_type_value_type_v<Container>, "Container should have nested type 'value_type'");
* static_assert(!has_nested_type_value_type_v<SimpleClass>, "SimpleClass should not have nested type 'value_type'");
*
* // Example usage with your 'device_t' case:
* generate_has_nested_type(device_t)
*
* class MyTask { public: using device_t = class MyDevice; };
* class AnotherTask { // something here };
*
* static_assert(etools::meta::has_nested_type_device_t_v<MyTask>, "MyTask should define device_t");
* static_assert(!etools::meta::has_nested_type_device_t_v<AnotherTask>, "AnotherTask should not define device_t");
*/
#define generate_has_nested_type(nested_type)                                       \
namespace etools::meta {                                                            \
                                                                                    \
    template <typename T, typename = void>                                          \
    struct has_nested_type_##nested_type : std::false_type {};                      \
                                                                                    \
    template <typename T>                                                           \
    struct has_nested_type_##nested_type<T, std::void_t<typename T::nested_type>>   \
        : std::true_type {};                                                        \
                                                                                    \
    template <typename T>                                                           \
    inline constexpr bool has_nested_type_##nested_type##_v =                       \
        has_nested_type_##nested_type<T>::value;                                    \
} // etools::meta
 
#endif //ETOOLS_INFO_GEN_HPP_