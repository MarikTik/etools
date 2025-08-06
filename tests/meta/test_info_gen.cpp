#include <gtest/gtest.h>
#include <etools/meta/info_gen.hpp>


struct basic_public_members {
    int id;
    double value;
    const char* name = "test";

    void foo() {}
    int bar(int a, float b) const { return a + static_cast<int>(b); }
    constexpr bool is_valid() const noexcept { return true; }
};

class mixed_access_members {
public:
    int public_field;
    void public_method() {}

protected:
    float protected_field;
    void protected_method() {}

private:
    double private_field;
    void private_method(int) {}
};

struct static_and_constexpr_members {
    static inline int static_counter = 0;
    static constexpr const char* static_name = "staticy";
    static int get_static() { return static_counter; }

    constexpr int get_constexpr() const { return 42; }
    const int constant = 10;
};


struct complex_method_signatures {
    using alias_t = int;

    void overloaded();
    void overloaded(int);
    double returns_complex(double x, int y) const noexcept { return x + y; }
    template <typename... Args>
    void variadic(Args&&...) {}
};

struct edge_case_struct {
    static const int static_const_array[3];
    char buffer[16];

    edge_case_struct() = delete;
    edge_case_struct(int) {}

    struct inner_t {
        int deep_field;
    };

    inner_t inner;
};
const int edge_case_struct::static_const_array[3] = {1, 2, 3};



///////////////////////////////////////////////////////////// `generate_has_member` TESTS  /////////////////////////////////////////////////////////////

generate_has_member(id);
generate_has_member(foo);
generate_has_member(value);
generate_has_member(name);
generate_has_member(public_field);
generate_has_member(protected_field);
generate_has_member(private_field);
generate_has_member(static_counter);
generate_has_member(static_name);
generate_has_member(get_static);
generate_has_member(constant);
generate_has_member(get_constexpr);
generate_has_member(overloaded);
generate_has_member(returns_complex);
generate_has_member(variadic);
generate_has_member(alias_t);
generate_has_member(static_const_array);
generate_has_member(buffer);
generate_has_member(inner);
generate_has_member(deep_field);
generate_has_member(bar);
generate_has_member(is_valid);
generate_has_member(public_method);
generate_has_member(private_method);
generate_has_member(protected_method);

TEST(GenerateHasMemberTest, BasicPublicMembers_Positive) {
    EXPECT_TRUE((etools::meta::has_member_id_v<basic_public_members>));
    EXPECT_TRUE((etools::meta::has_member_value_v<basic_public_members>));
    EXPECT_TRUE((etools::meta::has_member_name_v<basic_public_members>));
    EXPECT_TRUE((etools::meta::has_member_foo_v<basic_public_members>));
    EXPECT_TRUE((etools::meta::has_member_bar_v<basic_public_members>));
    EXPECT_TRUE((etools::meta::has_member_is_valid_v<basic_public_members>));
}

TEST(GenerateHasMemberTest, BasicPublicMembers_Negative) {
    EXPECT_FALSE((etools::meta::has_member_static_counter_v<basic_public_members>));
    EXPECT_FALSE((etools::meta::has_member_inner_v<basic_public_members>));
}
TEST(GenerateHasMemberTest, MixedAccessMembers_Positive) {
    EXPECT_TRUE((etools::meta::has_member_public_field_v<mixed_access_members>));
    EXPECT_TRUE((etools::meta::has_member_public_method_v<mixed_access_members>));
}

TEST(GenerateHasMemberTest, MixedAccessMembers_Negative) {
    EXPECT_FALSE((etools::meta::has_member_protected_field_v<mixed_access_members>));
    EXPECT_FALSE((etools::meta::has_member_protected_method_v<mixed_access_members>));
    EXPECT_FALSE((etools::meta::has_member_private_field_v<mixed_access_members>));
    EXPECT_FALSE((etools::meta::has_member_private_method_v<mixed_access_members>));
    EXPECT_FALSE((etools::meta::has_member_variadic_v<mixed_access_members>));
    EXPECT_FALSE((etools::meta::has_member_static_name_v<mixed_access_members>));
}

TEST(GenerateHasMemberTest, StaticAndConstexprMembers_Positive) {
    EXPECT_TRUE((etools::meta::has_member_static_counter_v<static_and_constexpr_members>));
    EXPECT_TRUE((etools::meta::has_member_static_name_v<static_and_constexpr_members>));
    EXPECT_TRUE((etools::meta::has_member_get_static_v<static_and_constexpr_members>));
    EXPECT_TRUE((etools::meta::has_member_get_constexpr_v<static_and_constexpr_members>));
    EXPECT_TRUE((etools::meta::has_member_constant_v<static_and_constexpr_members>));
}

TEST(GenerateHasMemberTest, StaticAndConstexprMembers_Negative) {
    EXPECT_FALSE((etools::meta::has_member_bar_v<static_and_constexpr_members>));
    EXPECT_FALSE((etools::meta::has_member_buffer_v<static_and_constexpr_members>));
}

TEST(GenerateHasMemberTest, ComplexMethodSignatures_Positive) {
    EXPECT_TRUE((etools::meta::has_member_returns_complex_v<complex_method_signatures>));
}

TEST(GenerateHasMemberTest, ComplexMethodSignatures_Negative) {
    EXPECT_FALSE((etools::meta::has_member_overloaded_v<complex_method_signatures>)); // overloads can't be checked via &T::overloaded
    EXPECT_FALSE((etools::meta::has_member_variadic_v<complex_method_signatures>));   // template member
    EXPECT_FALSE((etools::meta::has_member_alias_t_v<complex_method_signatures>));    // not a member, a type alias
    EXPECT_FALSE((etools::meta::has_member_constant_v<complex_method_signatures>));   // not part of this struct
}

TEST(GenerateHasMemberTest, EdgeCaseStructs_Positive) {
    EXPECT_TRUE((etools::meta::has_member_static_const_array_v<edge_case_struct>));
    EXPECT_TRUE((etools::meta::has_member_buffer_v<edge_case_struct>));
    EXPECT_TRUE((etools::meta::has_member_inner_v<edge_case_struct>));
}

TEST(GenerateHasMemberTest, EdgeCaseStructs_Negative) {
    EXPECT_FALSE((etools::meta::has_member_deep_field_v<edge_case_struct>)); // nested member
    EXPECT_FALSE((etools::meta::has_member_variadic_v<edge_case_struct>));
}



///////////////////////////////////////////////////////////// `generate_has_member_variable` TESTS  /////////////////////////////////////////////////////////////


generate_has_member_variable(id);
generate_has_member_variable(foo);
generate_has_member_variable(public_field);
generate_has_member_variable(protected_field);
generate_has_member_variable(static_counter);
generate_has_member_variable(get_constexpr);
generate_has_member_variable(overloaded);
generate_has_member_variable(alias_t);
generate_has_member_variable(buffer);
generate_has_member_variable(public_method);
generate_has_member_variable(deep_field);
generate_has_member_variable(private_field);
generate_has_member_variable(constant);
generate_has_member_variable(static_name);
generate_has_member_variable(returns_complex);
generate_has_member_variable(inner);
generate_has_member_variable(static_const_array);
generate_has_member_variable(name);
generate_has_member_variable(value);
generate_has_member_variable(is_valid);
generate_has_member_variable(get_static);

TEST(GenerateHasMemberVariableTest, BasicPublicMembers_Positive) {
    EXPECT_TRUE((etools::meta::has_member_variable_id_v<basic_public_members>));
    EXPECT_TRUE((etools::meta::has_member_variable_value_v<basic_public_members>));
    EXPECT_TRUE((etools::meta::has_member_variable_name_v<basic_public_members>));
}

TEST(GenerateHasMemberVariableTest, BasicPublicMembers_Negative) {
    EXPECT_FALSE((etools::meta::has_member_variable_foo_v<basic_public_members>));  // method
    EXPECT_FALSE((etools::meta::has_member_variable_is_valid_v<basic_public_members>)); // method
}

TEST(GenerateHasMemberVariableTest, MixedAccessMembers_Positive) {
    EXPECT_TRUE((etools::meta::has_member_variable_public_field_v<mixed_access_members>));
}

TEST(GenerateHasMemberVariableTest, MixedAccessMembers_Negative) {
    EXPECT_FALSE((etools::meta::has_member_variable_protected_field_v<mixed_access_members>)); // protected
    EXPECT_FALSE((etools::meta::has_member_variable_private_field_v<mixed_access_members>));   // private
    EXPECT_FALSE((etools::meta::has_member_variable_public_method_v<mixed_access_members>));   // method
}

TEST(GenerateHasMemberVariableTest, StaticAndConstexprMembers_Positive) {
    EXPECT_TRUE((etools::meta::has_member_variable_constant_v<static_and_constexpr_members>));
}

TEST(GenerateHasMemberVariableTest, StaticAndConstexprMembers_Negative) {
    EXPECT_FALSE((etools::meta::has_member_variable_static_counter_v<static_and_constexpr_members>)); // static
    EXPECT_FALSE((etools::meta::has_member_variable_static_name_v<static_and_constexpr_members>));    // static
    EXPECT_FALSE((etools::meta::has_member_variable_get_static_v<static_and_constexpr_members>));     // method
}

TEST(GenerateHasMemberVariableTest, ComplexMethodSignatures_Positive) {
    // No valid member variables here
}

TEST(GenerateHasMemberVariableTest, ComplexMethodSignatures_Negative) {
    EXPECT_FALSE((etools::meta::has_member_variable_alias_t_v<complex_method_signatures>)); // type alias
    EXPECT_FALSE((etools::meta::has_member_variable_overloaded_v<complex_method_signatures>)); // method
    EXPECT_FALSE((etools::meta::has_member_variable_returns_complex_v<complex_method_signatures>)); // method
}

TEST(GenerateHasMemberVariableTest, EdgeCaseStruct_Positive) {
    EXPECT_TRUE((etools::meta::has_member_variable_buffer_v<edge_case_struct>));
    EXPECT_TRUE((etools::meta::has_member_variable_inner_v<edge_case_struct>));
}

TEST(GenerateHasMemberVariableTest, EdgeCaseStruct_Negative) {
    EXPECT_FALSE((etools::meta::has_member_variable_static_const_array_v<edge_case_struct>)); // static
    EXPECT_FALSE((etools::meta::has_member_variable_deep_field_v<edge_case_struct>)); // nested
}


///////////////////////////////////////////////////////////// `generate_has_static_member_variable` TESTS  /////////////////////////////////////////////////////////////

generate_has_static_member_variable(id)
generate_has_static_member_variable(value)
generate_has_static_member_variable(name)
generate_has_static_member_variable(foo)
generate_has_static_member_variable(bar)
generate_has_static_member_variable(is_valid)
generate_has_static_member_variable(public_field)
generate_has_static_member_variable(public_method)
generate_has_static_member_variable(protected_field)
generate_has_static_member_variable(protected_method)
generate_has_static_member_variable(private_field)
generate_has_static_member_variable(private_method)
generate_has_static_member_variable(static_counter)
generate_has_static_member_variable(static_name)
generate_has_static_member_variable(get_static)
generate_has_static_member_variable(constant)
generate_has_static_member_variable(get_constexpr)
generate_has_static_member_variable(overloaded)
generate_has_static_member_variable(returns_complex)
generate_has_static_member_variable(variadic)
generate_has_static_member_variable(alias_t)
generate_has_static_member_variable(static_const_array)
generate_has_static_member_variable(buffer)
generate_has_static_member_variable(inner)
generate_has_static_member_variable(deep_field)



TEST(GenerateHasStaticMemberVariableTest, BasicMembersNotExist) {
    // None of the members in basic_public_members are static.
    EXPECT_FALSE((etools::meta::has_static_member_variable_id_v<basic_public_members>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_value_v<basic_public_members>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_name_v<basic_public_members>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_foo_v<basic_public_members>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_bar_v<basic_public_members>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_is_valid_v<basic_public_members>));
}

TEST(GenerateHasStaticMemberVariableTest, MixedAccessMembersNotExist) {
    // None of the members in mixed_access_members are static.
    EXPECT_FALSE((etools::meta::has_static_member_variable_public_field_v<mixed_access_members>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_public_method_v<mixed_access_members>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_protected_field_v<mixed_access_members>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_protected_method_v<mixed_access_members>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_private_field_v<mixed_access_members>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_private_method_v<mixed_access_members>));
}

TEST(GenerateHasStaticMemberVariableTest, StaticMembersExist) {
    // The macro should detect static data members.
    EXPECT_TRUE((etools::meta::has_static_member_variable_static_counter_v<static_and_constexpr_members>));
    EXPECT_TRUE((etools::meta::has_static_member_variable_static_name_v<static_and_constexpr_members>));
}

TEST(GenerateHasStaticMemberVariableTest, StaticAndConstexprMembersNotExist) {
    // The macro should correctly fail for non-static and function members.
    EXPECT_FALSE((etools::meta::has_static_member_variable_get_static_v<static_and_constexpr_members>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_constant_v<static_and_constexpr_members>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_get_constexpr_v<static_and_constexpr_members>));
    
    // Check for non-existent members.
    EXPECT_FALSE((etools::meta::has_static_member_variable_id_v<static_and_constexpr_members>));
}

TEST(GenerateHasStaticMemberVariableTest, ComplexMembersNotExist) {
    // None of the members in complex_method_signatures are static member variables.
    EXPECT_FALSE((etools::meta::has_static_member_variable_overloaded_v<complex_method_signatures>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_returns_complex_v<complex_method_signatures>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_variadic_v<complex_method_signatures>));
    
    // The macro should also not detect a type alias.
    EXPECT_FALSE((etools::meta::has_static_member_variable_alias_t_v<complex_method_signatures>));
}

TEST(GenerateHasStaticMemberVariableTest, EdgeCaseStructsExist) {
    // The macro should detect the static const array.
    EXPECT_TRUE((etools::meta::has_static_member_variable_static_const_array_v<edge_case_struct>));
}

TEST(GenerateHasStaticMemberVariableTest, EdgeCaseStructsNotExist) {
    // The macro should correctly fail for non-static members.
    EXPECT_FALSE((etools::meta::has_static_member_variable_buffer_v<edge_case_struct>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_inner_v<edge_case_struct>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_deep_field_v<edge_case_struct>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_name_v<edge_case_struct>));
}

TEST(GenerateHasStaticMemberVariableTest, AllMembersTested) {
    // This test ensures that all the defined members in the various structs
    // are checked for whether they are a static member variable or not.
    // This is a sanity check to ensure the test coverage is correct.
    EXPECT_FALSE((etools::meta::has_static_member_variable_id_v<basic_public_members>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_value_v<basic_public_members>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_name_v<basic_public_members>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_public_field_v<mixed_access_members>));
    EXPECT_TRUE((etools::meta::has_static_member_variable_static_counter_v<static_and_constexpr_members>));
    EXPECT_TRUE((etools::meta::has_static_member_variable_static_name_v<static_and_constexpr_members>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_constant_v<static_and_constexpr_members>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_overloaded_v<complex_method_signatures>));
    EXPECT_TRUE((etools::meta::has_static_member_variable_static_const_array_v<edge_case_struct>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_buffer_v<edge_case_struct>));
}

TEST(GenerateHasStaticMemberVariableTest, EmptyStructNotExist) {
    struct empty_struct {};
    EXPECT_FALSE((etools::meta::has_static_member_variable_id_v<empty_struct>));
    EXPECT_FALSE((etools::meta::has_static_member_variable_static_counter_v<empty_struct>));
}



///////////////////////////////////////////////////////////// `generate_has_static_member` TESTS  /////////////////////////////////////////////////////////////

generate_has_static_member(id)
generate_has_static_member(value)
generate_has_static_member(name)
generate_has_static_member(foo)
generate_has_static_member(bar)
generate_has_static_member(is_valid)
generate_has_static_member(public_field)
generate_has_static_member(public_method)
generate_has_static_member(protected_field)
generate_has_static_member(protected_method)
generate_has_static_member(private_field)
generate_has_static_member(private_method)
generate_has_static_member(static_counter)
generate_has_static_member(static_name)
generate_has_static_member(get_static)
generate_has_static_member(constant)
generate_has_static_member(get_constexpr)
generate_has_static_member(overloaded)
generate_has_static_member(returns_complex)
generate_has_static_member(variadic)
generate_has_static_member(alias_t)
generate_has_static_member(static_const_array)
generate_has_static_member(buffer)
generate_has_static_member(inner)
generate_has_static_member(deep_field)


// --- The Rewritten Test Suite with the Requested Naming Convention ---

TEST(GenerateHasStaticMemberTest, BasicPublicMembers_Positive) {
    // This struct has no static members, so no positive tests.
}

TEST(GenerateHasStaticMemberTest, BasicPublicMembers_Negative) {
    EXPECT_FALSE((etools::meta::has_static_member_id_v<basic_public_members>));
    EXPECT_FALSE((etools::meta::has_static_member_value_v<basic_public_members>));
    EXPECT_FALSE((etools::meta::has_static_member_name_v<basic_public_members>));
    EXPECT_FALSE((etools::meta::has_static_member_foo_v<basic_public_members>));
    EXPECT_FALSE((etools::meta::has_static_member_bar_v<basic_public_members>));
    EXPECT_FALSE((etools::meta::has_static_member_is_valid_v<basic_public_members>));
    EXPECT_FALSE((etools::meta::has_static_member_static_counter_v<basic_public_members>));
}

TEST(GenerateHasStaticMemberTest, MixedAccessMembers_Positive) {
    // This struct has no static members, so no positive tests.
}

TEST(GenerateHasStaticMemberTest, MixedAccessMembers_Negative) {
    EXPECT_FALSE((etools::meta::has_static_member_public_field_v<mixed_access_members>));
    EXPECT_FALSE((etools::meta::has_static_member_public_method_v<mixed_access_members>));
    EXPECT_FALSE((etools::meta::has_static_member_protected_field_v<mixed_access_members>));
    EXPECT_FALSE((etools::meta::has_static_member_protected_method_v<mixed_access_members>));
    EXPECT_FALSE((etools::meta::has_static_member_private_field_v<mixed_access_members>));
    EXPECT_FALSE((etools::meta::has_static_member_private_method_v<mixed_access_members>));
    EXPECT_FALSE((etools::meta::has_static_member_id_v<mixed_access_members>));
}

TEST(GenerateHasStaticMemberTest, StaticAndConstexprMembers_Positive) {
    EXPECT_TRUE((etools::meta::has_static_member_static_counter_v<static_and_constexpr_members>));
    EXPECT_TRUE((etools::meta::has_static_member_static_name_v<static_and_constexpr_members>));
    EXPECT_TRUE((etools::meta::has_static_member_get_static_v<static_and_constexpr_members>));
}

TEST(GenerateHasStaticMemberTest, StaticAndConstexprMembers_Negative) {
    EXPECT_FALSE((etools::meta::has_static_member_constant_v<static_and_constexpr_members>));
    EXPECT_FALSE((etools::meta::has_static_member_get_constexpr_v<static_and_constexpr_members>));
    EXPECT_FALSE((etools::meta::has_static_member_id_v<static_and_constexpr_members>));
}

TEST(GenerateHasStaticMemberTest, ComplexMethodSignatures_Positive) {
    // This struct has no static members, so no positive tests.
}

TEST(GenerateHasStaticMemberTest, ComplexMethodSignatures_Negative) {
    EXPECT_FALSE((etools::meta::has_static_member_overloaded_v<complex_method_signatures>));
    EXPECT_FALSE((etools::meta::has_static_member_returns_complex_v<complex_method_signatures>));
    EXPECT_FALSE((etools::meta::has_static_member_variadic_v<complex_method_signatures>));
    EXPECT_FALSE((etools::meta::has_static_member_alias_t_v<complex_method_signatures>));
    EXPECT_FALSE((etools::meta::has_static_member_static_const_array_v<complex_method_signatures>));
}

TEST(GenerateHasStaticMemberTest, EdgeCaseStruct_Positive) {
    EXPECT_TRUE((etools::meta::has_static_member_static_const_array_v<edge_case_struct>));
}

TEST(GenerateHasStaticMemberTest, EdgeCaseStruct_Negative) {
    EXPECT_FALSE((etools::meta::has_static_member_buffer_v<edge_case_struct>));
    EXPECT_FALSE((etools::meta::has_static_member_inner_v<edge_case_struct>));
    EXPECT_FALSE((etools::meta::has_static_member_deep_field_v<edge_case_struct>));
    EXPECT_FALSE((etools::meta::has_static_member_id_v<edge_case_struct>));
}

TEST(GenerateHasStaticMemberTest, EmptyStruct_Negative) {
    struct empty_struct {};
    EXPECT_FALSE((etools::meta::has_static_member_id_v<empty_struct>));
    EXPECT_FALSE((etools::meta::has_static_member_static_counter_v<empty_struct>));
}



///////////////////////////////////////////////////////////// `generate_has_nested_types` TESTS  /////////////////////////////////////////////////////////////


generate_has_nested_type(id)
generate_has_nested_type(value)
generate_has_nested_type(name)
generate_has_nested_type(foo)
generate_has_nested_type(bar)
generate_has_nested_type(is_valid)
generate_has_nested_type(public_field)
generate_has_nested_type(public_method)
generate_has_nested_type(protected_field)
generate_has_nested_type(protected_method)
generate_has_nested_type(private_field)
generate_has_nested_type(private_method)
generate_has_nested_type(static_counter)
generate_has_nested_type(static_name)
generate_has_nested_type(get_static)
generate_has_nested_type(constant)
generate_has_nested_type(get_constexpr)
generate_has_nested_type(overloaded)
generate_has_nested_type(returns_complex)
generate_has_nested_type(variadic)
generate_has_nested_type(alias_t)
generate_has_nested_type(static_const_array)
generate_has_nested_type(buffer)
generate_has_nested_type(inner)
generate_has_nested_type(deep_field)
generate_has_nested_type(inner_t)


// --- The New Test Suite for has_nested_type ---

TEST(GenerateHasNestedTypeTest, BasicPublicMembers_Positive) {
    // This struct has no nested types, so no positive tests.
}

TEST(GenerateHasNestedTypeTest, BasicPublicMembers_Negative) {
    EXPECT_FALSE((etools::meta::has_nested_type_id_v<basic_public_members>));
    EXPECT_FALSE((etools::meta::has_nested_type_foo_v<basic_public_members>));
    EXPECT_FALSE((etools::meta::has_nested_type_is_valid_v<basic_public_members>));
    EXPECT_FALSE((etools::meta::has_nested_type_alias_t_v<basic_public_members>));
}

TEST(GenerateHasNestedTypeTest, MixedAccessMembers_Positive) {
    // This struct has no nested types, so no positive tests.
}

TEST(GenerateHasNestedTypeTest, MixedAccessMembers_Negative) {
    EXPECT_FALSE((etools::meta::has_nested_type_public_field_v<mixed_access_members>));
    EXPECT_FALSE((etools::meta::has_nested_type_public_method_v<mixed_access_members>));
    EXPECT_FALSE((etools::meta::has_nested_type_private_field_v<mixed_access_members>));
    EXPECT_FALSE((etools::meta::has_nested_type_alias_t_v<mixed_access_members>));
}

TEST(GenerateHasNestedTypeTest, StaticAndConstexprMembers_Positive) {
    // This struct has no nested types, so no positive tests.
}

TEST(GenerateHasNestedTypeTest, StaticAndConstexprMembers_Negative) {
    EXPECT_FALSE((etools::meta::has_nested_type_static_counter_v<static_and_constexpr_members>));
    EXPECT_FALSE((etools::meta::has_nested_type_constant_v<static_and_constexpr_members>));
    EXPECT_FALSE((etools::meta::has_nested_type_get_static_v<static_and_constexpr_members>));
    EXPECT_FALSE((etools::meta::has_nested_type_alias_t_v<static_and_constexpr_members>));
}

TEST(GenerateHasNestedTypeTest, ComplexMethodSignatures_Positive) {
    // This struct has a type alias.
    EXPECT_TRUE((etools::meta::has_nested_type_alias_t_v<complex_method_signatures>));
}

TEST(GenerateHasNestedTypeTest, ComplexMethodSignatures_Negative) {
    EXPECT_FALSE((etools::meta::has_nested_type_overloaded_v<complex_method_signatures>));
    EXPECT_FALSE((etools::meta::has_nested_type_returns_complex_v<complex_method_signatures>));
    EXPECT_FALSE((etools::meta::has_nested_type_variadic_v<complex_method_signatures>));
    EXPECT_FALSE((etools::meta::has_nested_type_static_counter_v<complex_method_signatures>));
}

TEST(GenerateHasNestedTypeTest, EdgeCaseStruct_Positive) {
    // This struct has a nested struct definition.
    EXPECT_TRUE((etools::meta::has_nested_type_inner_t_v<edge_case_struct>));
}

TEST(GenerateHasNestedTypeTest, EdgeCaseStruct_Negative) {
    EXPECT_FALSE((etools::meta::has_nested_type_static_const_array_v<edge_case_struct>));
    EXPECT_FALSE((etools::meta::has_nested_type_buffer_v<edge_case_struct>));
    EXPECT_FALSE((etools::meta::has_nested_type_inner_v<edge_case_struct>)); // this is an instance, not a type
    EXPECT_FALSE((etools::meta::has_nested_type_deep_field_v<edge_case_struct>)); // this is a nested member
    EXPECT_FALSE((etools::meta::has_nested_type_alias_t_v<edge_case_struct>));
}


///////////////////////////////////////////////////////////// `generate_has_method` TESTS  /////////////////////////////////////////////////////////////


generate_has_method(foo, ())
generate_has_method(bar, (int, float) const)
generate_has_method(is_valid, () const noexcept)

// Mixed access members
generate_has_method(public_method, ())
generate_has_method(protected_method, ())
generate_has_method(private_method, (int))

// Static and constexpr members
generate_has_method(get_static, ())
generate_has_method(get_constexpr, () const)

// Complex signatures
generate_has_method(overloaded, ())
generate_has_method(overloaded, (int))
generate_has_method(returns_complex, (double, int) const noexcept)
generate_has_method(variadic, ())


// --- The New Test Suite for has_method ---

TEST(GenerateHasMethodTest, BasicPublicMembers_Positive) {
    // Check for public member functions with their exact signatures.
    EXPECT_TRUE((etools::meta::has_method_foo_v<basic_public_members>));
    EXPECT_TRUE((etools::meta::has_method_bar_v<basic_public_members>));
    EXPECT_TRUE((etools::meta::has_method_is_valid_v<basic_public_members>));
}

TEST(GenerateHasMethodTest, BasicPublicMembers_Negative) {
    // Check that non-function members and functions with wrong signatures are not detected.
    EXPECT_FALSE((etools::meta::has_method_id_v<basic_public_members>)); // Not a method
    EXPECT_FALSE((etools::meta::has_method_value_v<basic_public_members>)); // Not a method
    EXPECT_FALSE((etools::meta::has_method_name_v<basic_public_members>)); // Not a method
    EXPECT_FALSE((etools::meta::has_method_foo_v<basic_public_members, int>)); // Wrong signature
}

TEST(GenerateHasMethodTest, MixedAccessMembers_Positive) {
    // Only public methods should be detected from this context.
    EXPECT_TRUE((etools::meta::has_method_public_method_v<mixed_access_members>));
}

TEST(GenerateHasMethodTest, MixedAccessMembers_Negative) {
    // Check that non-public members and non-existent methods are not detected.
    EXPECT_FALSE((etools::meta::has_method_protected_method_v<mixed_access_members>)); // Protected
    EXPECT_FALSE((etools::meta::has_method_private_method_v<mixed_access_members>));   // Private
    EXPECT_FALSE((etools::meta::has_method_public_field_v<mixed_access_members>));     // Not a method
    EXPECT_FALSE((etools::meta::has_method_foo_v<mixed_access_members>)); // Non-existent method
}

TEST(GenerateHasMethodTest, StaticAndConstexprMembers_Positive) {
    // Check that static and constexpr methods are correctly detected.
    EXPECT_TRUE((etools::meta::has_method_get_static_v<static_and_constexpr_members>));
    EXPECT_TRUE((etools::meta::has_method_get_constexpr_v<static_and_constexpr_members>));
}

TEST(GenerateHasMethodTest, StaticAndConstexprMembers_Negative) {
    // Check that non-method members are not detected.
    EXPECT_FALSE((etools::meta::has_method_static_counter_v<static_and_constexpr_members>));
    EXPECT_FALSE((etools::meta::has_method_constant_v<static_and_constexpr_members>));
    EXPECT_FALSE((etools::meta::has_method_id_v<static_and_constexpr_members>)); // Non-existent
}

TEST(GenerateHasMethodTest, ComplexMethodSignatures_Positive) {
    // Check for overloaded methods and a complex signature with const and noexcept.
    EXPECT_TRUE((etools::meta::has_method_overloaded_v<complex_method_signatures>)); // Checks for overloaded()
    EXPECT_TRUE((etools::meta::has_method_overloaded_v<complex_method_signatures, int>)); // Checks for overloaded(int)
    EXPECT_TRUE((etools::meta::has_method_returns_complex_v<complex_method_signatures>));
    
    // The macro should also be able to detect a variadic template function with an empty parameter list.
    EXPECT_TRUE((etools::meta::has_method_variadic_v<complex_method_signatures>));
}

TEST(GenerateHasMethodTest, ComplexMethodSignatures_Negative) {
    // Check for non-method members and non-existent methods.
    EXPECT_FALSE((etools::meta::has_method_alias_t_v<complex_method_signatures>));
    EXPECT_FALSE((etools::meta::has_method_id_v<complex_method_signatures>));
}

TEST(GenerateHasMethodTest, EdgeCaseStruct_Positive) {
    // This struct has no member functions, so no positive tests.
}

TEST(GenerateHasMethodTest, EdgeCaseStruct_Negative) {
    // Check all members and non-existent methods.
    EXPECT_FALSE((etools::meta::has_method_static_const_array_v<edge_case_struct>));
    EXPECT_FALSE((etools::meta::has_method_buffer_v<edge_case_struct>));
    EXPECT_FALSE((etools::meta::has_method_inner_v<edge_case_struct>));
    EXPECT_FALSE((etools::meta::has_method_deep_field_v<edge_case_struct>));
    EXPECT_FALSE((etools::meta::has_method_foo_v<edge_case_struct>));
}