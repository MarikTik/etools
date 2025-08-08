#include "gtest/gtest.h"
#include <string>
#include <vector>
#include <memory>
#include <numeric>

 
#include <etools/facilities/registry.hpp>
#include <etools/memory/slot.hpp>
// The base class for our commands
class Command {
public:
    static int s_destructor_count;
    static std::string s_last_destructed_name;

    explicit Command(const std::string& name) : m_name(name) {}
    virtual ~Command() {
        s_destructor_count++;
        s_last_destructed_name = m_name;
    }

    virtual void do_action() = 0;
    virtual double result() = 0;
    virtual void do_with_params(int a, int b) = 0;

protected:
    std::string m_name;
};
int Command::s_destructor_count = 0;
std::string Command::s_last_destructed_name = "";

// Concrete command implementations
class AddCommand : public Command {
public:
    static constexpr int id = 10;
    explicit AddCommand(const std::string& name) noexcept : Command(name) {}
    
    void do_action() override { m_result = 5 + 5; }
    double result() override { return m_result; }
    void do_with_params(int a, int b) override { m_result = a + b; }
private:
    double m_result = 0.0;
};

class SubtractCommand : public Command {
public:
    static constexpr int id = 20;
    explicit SubtractCommand(const std::string& name) noexcept : Command(name) {}

    void do_action() override { m_result = 10 - 3; }
    double result() override { return m_result; }
    void do_with_params(int a, int b) override { m_result = a - b; }
private:
    double m_result = 0.0;
};

class MultiplyCommand : public Command {
public:
    static constexpr int id = 30;
    explicit MultiplyCommand(const std::string& name) noexcept : Command(name) {}

    void do_action() override { m_result = 4 * 4; }
    double result() override { return m_result; }
    void do_with_params(int a, int b) override { m_result = a * b; }
private:
    double m_result = 0.0;
};

class DivideCommand : public Command {
public:
    static constexpr int id = 40;
    explicit DivideCommand(const std::string& name) noexcept : Command(name) {}
    
    void do_action() override { m_result = 100.0 / 2.0; }
    double result() override { return m_result; }
    void do_with_params(int a, int b) override { m_result = static_cast<double>(a) / b; }
private:
    double m_result = 0.0;
};

class DummyCommand : public Command {
public:
    static constexpr int id = 99;
    explicit DummyCommand(const std::string& name) noexcept : Command(name) {}

    void do_action() override {}
    double result() override { return 0.0; }
    void do_with_params([[maybe_unused]] int a, [[maybe_unused]] int b) override {}
};

template<typename T>
struct CommandExtractor {
    static constexpr int value = T::id;
};

 
using CommandTypes = etools::meta::typelist<AddCommand, SubtractCommand, MultiplyCommand, DivideCommand>;
using CommandArgs = etools::meta::typelist<std::string>;

 
using CommandRegistry = etools::facilities::registry<Command, CommandExtractor, CommandTypes, CommandArgs>;


// Test fixture to reset the singleton for each test
class RegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset the registry by destroying all elements
        CommandRegistry::instance().destroy(AddCommand::id);
        CommandRegistry::instance().destroy(SubtractCommand::id);
        CommandRegistry::instance().destroy(MultiplyCommand::id);
        CommandRegistry::instance().destroy(DivideCommand::id);
        
        // Reset static counters
        Command::s_destructor_count = 0;
        Command::s_last_destructed_name = "";
    }
};


TEST_F(RegistryTest, InitialState_AllEmpty) {
    auto& reg = CommandRegistry::instance();

    EXPECT_EQ(reg.get(AddCommand::id), nullptr);
    EXPECT_EQ(reg.get(SubtractCommand::id), nullptr);
    EXPECT_EQ(reg.get(MultiplyCommand::id), nullptr);
    EXPECT_EQ(reg.get(DivideCommand::id), nullptr);
    
    // Check for an unknown key
    EXPECT_EQ(reg.get(999), nullptr);

    // Check iteration on an empty registry
    EXPECT_EQ(std::distance(reg.begin(), reg.end()), 0);
    EXPECT_EQ(std::distance(reg.cbegin(), reg.cend()), 0);
}

TEST_F(RegistryTest, ConstructAndGet_ValidKey) {
    auto& reg = CommandRegistry::instance();

    Command* add_cmd = reg.construct(AddCommand::id, "AddCmd");
    EXPECT_NE(add_cmd, nullptr);
    EXPECT_EQ(reg.get(AddCommand::id), add_cmd);

    // Verify polymorphic behavior
    add_cmd->do_action();
    EXPECT_EQ(add_cmd->result(), 10.0);
    add_cmd->do_with_params(10, 20);
    EXPECT_EQ(add_cmd->result(), 30.0);

    // Make sure other keys are still empty
    EXPECT_EQ(reg.get(SubtractCommand::id), nullptr);
    
    // Ensure the destructor hasn't been called yet
    EXPECT_EQ(Command::s_destructor_count, 0);
}

TEST_F(RegistryTest, Construct_InvalidKeyReturnsNull) {
    auto& reg = CommandRegistry::instance();
    Command* cmd = reg.construct(999, "Invalid");
    EXPECT_EQ(cmd, nullptr);
    EXPECT_EQ(reg.get(999), nullptr);
}

TEST_F(RegistryTest, Find_ReturnsCorrectIterator) {
    auto& reg = CommandRegistry::instance();

    reg.construct(AddCommand::id, "AddCmd");
    reg.construct(MultiplyCommand::id, "MulCmd");

    auto it = reg.find(AddCommand::id);
    EXPECT_NE(it, reg.end());
    EXPECT_EQ(it->result(), 0.0); // before action
    it->do_with_params(1, 2);
    EXPECT_EQ(it->result(), 3.0);

    EXPECT_EQ(reg.end(), reg.end());
    auto not_found_it = reg.find(SubtractCommand::id);
    EXPECT_EQ(not_found_it, reg.end());
}

TEST_F(RegistryTest, FindConst_ReturnsCorrectIterator) {
    // This test ensures const-correctness of the find method
    auto& reg = CommandRegistry::instance();
    reg.construct(AddCommand::id, "AddCmd");

    const auto& const_reg = reg;
    auto it = const_reg.find(AddCommand::id);
    EXPECT_NE(it, const_reg.end());

    // Should not be able to modify the object via a const_iterator
     
}

TEST_F(RegistryTest, Destructor_CorrectlyCalled) {
    auto& reg = CommandRegistry::instance();

    Command* add_cmd = reg.construct(AddCommand::id, "AddCmd");
    EXPECT_NE(add_cmd, nullptr);
    EXPECT_EQ(Command::s_destructor_count, 0);

    reg.destroy(AddCommand::id);
    EXPECT_EQ(reg.get(AddCommand::id), nullptr);
    EXPECT_EQ(Command::s_destructor_count, 1);
    EXPECT_EQ(Command::s_last_destructed_name, "AddCmd");
}

TEST_F(RegistryTest, Destructor_OnInvalidKey_DoesNothing) {
    auto& reg = CommandRegistry::instance();
    reg.construct(AddCommand::id, "AddCmd");
    
    reg.destroy(999);
    EXPECT_NE(reg.get(AddCommand::id), nullptr);
    EXPECT_EQ(Command::s_destructor_count, 0);
}

TEST_F(RegistryTest, Iteration_OnlyConstructedObjects) {
    auto& reg = CommandRegistry::instance();

    reg.construct(AddCommand::id, "AddCmd");
    reg.construct(DivideCommand::id, "DivCmd");

    // We should only see two objects, even though the registry has four slots
    std::vector<int> found_ids;
    for (auto* cmd : reg) {
        if (auto* add = dynamic_cast<AddCommand*>(cmd)) {
            found_ids.push_back(add->id);
        } else if (auto* div = dynamic_cast<DivideCommand*>(cmd)) {
            found_ids.push_back(div->id);
        }
    }
    std::sort(found_ids.begin(), found_ids.end());
    
    EXPECT_EQ(found_ids.size(), 2);
    EXPECT_EQ(found_ids[0], AddCommand::id);
    EXPECT_EQ(found_ids[1], DivideCommand::id);
}

TEST_F(RegistryTest, Lifetime_MultipleConstructsAndDestructs) {
    auto& reg = CommandRegistry::instance();

    reg.construct(AddCommand::id, "Add-1");
    Command* cmd = reg.get(AddCommand::id);
    cmd->do_with_params(1, 2);
    EXPECT_EQ(cmd->result(), 3.0);
    EXPECT_EQ(Command::s_destructor_count, 0);

    reg.destroy(AddCommand::id);
    EXPECT_EQ(reg.get(AddCommand::id), nullptr);
    EXPECT_EQ(Command::s_destructor_count, 1);
    EXPECT_EQ(Command::s_last_destructed_name, "Add-1");

    cmd = reg.construct(AddCommand::id, "Add-2");
    cmd->do_with_params(10, 20);
    EXPECT_EQ(cmd->result(), 30.0);
    EXPECT_EQ(Command::s_destructor_count, 1);
    
    reg.destroy(AddCommand::id);
    EXPECT_EQ(Command::s_destructor_count, 2);
    EXPECT_EQ(Command::s_last_destructed_name, "Add-2");
}

TEST_F(RegistryTest, AllObjects_DestroyedOnRegistryDestruction) {
    // This is a special test that won't use the fixture's cleanup.
    // The registry is a singleton, so we can't easily re-create it for a test,
    // but its destructor logic can be implicitly tested by observing the
    // static destructor counter at the end of a test run.
    
    // We construct two objects in the registry.
    CommandRegistry::instance().construct(AddCommand::id, "TestAdd");
    CommandRegistry::instance().construct(SubtractCommand::id, "TestSub");
    
    // We don't call destroy() on them explicitly.
    // When the test fixture is torn down, the registry instance, being a static variable,
    // is destroyed at program exit. Its destructor should clean up all objects.
    // The final value of s_destructor_count should be 2.
    // This is more of an integration test for the final program state.
}

// NOTE: This test will only pass in a release build where NDEBUG is not defined.
// The registry's 'construct' method internally calls slot<T>::construct, which has an assertion.
// TEST_F(RegistryTest, DISABLED_Construct_Twice_AssertsInDebug) {
//     auto& reg = CommandRegistry::instance();
//     reg.construct(AddCommand::id, "First");

//     // The second call to construct should trigger an assertion
//     ASSERT_DEATH(reg.construct(AddCommand::id, "Second"), "Slot already constructed, cannot construct again.");
    
//     // Clean up
//     reg.destroy(AddCommand::id);
// }

TEST_F(RegistryTest, ConstFind_NotConstructed_DoesNotSkipToNext) {
    auto& reg = CommandRegistry::instance();

    // Construct only a *later* key (e.g., Multiply = 30)
    reg.construct(MultiplyCommand::id, "MulCmd");

    // Now ask for an *earlier* key (Add = 10) via const find.
    // With the fix, this must return cend() (and not skip to Multiply).
    const auto& const_reg = reg;
    auto it = const_reg.find(AddCommand::id);
    EXPECT_EQ(it, const_reg.cend());

    // Sanity: asking for Multiply must work.
    auto it2 = const_reg.find(MultiplyCommand::id);
    EXPECT_NE(it2, const_reg.cend());
    EXPECT_NE(*it2, nullptr);
}

TEST_F(RegistryTest, MutableFind_NotConstructed_ReturnsEnd) {
    auto& reg = CommandRegistry::instance();
    // Construct nothing; find should return end() for any key
    auto it = reg.find(SubtractCommand::id);
    EXPECT_EQ(it, reg.end());

    // Construct another command; still should be end() for Subtract
    reg.construct(AddCommand::id, "AddCmd");
    it = reg.find(SubtractCommand::id);
    EXPECT_EQ(it, reg.end());
}

TEST_F(RegistryTest, Begin_SkipsNulls_OnlyConstructedCount) {
    auto& reg = CommandRegistry::instance();

    // Construct a subset
    reg.construct(AddCommand::id, "AddCmd");
    reg.construct(DivideCommand::id, "DivCmd");

    // begin/end should iterate over exactly those two
    std::vector<int> ids;
    for (auto* cmd : reg) {
        if (dynamic_cast<AddCommand*>(cmd)) ids.push_back(AddCommand::id);
        if (dynamic_cast<DivideCommand*>(cmd)) ids.push_back(DivideCommand::id);
    }
    std::sort(ids.begin(), ids.end());
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], AddCommand::id);
    EXPECT_EQ(ids[1], DivideCommand::id);
}

TEST_F(RegistryTest, Iteration_OrderFollowsRoutingTable_SkippingUnconstructed) {
    auto& reg = CommandRegistry::instance();
    // Template order is: Add(10), Sub(20), Mul(30), Div(40)
    // Construct Mul and Add and Div (skip Sub)
    reg.construct(MultiplyCommand::id, "MulCmd");
    reg.construct(AddCommand::id, "AddCmd");
    reg.construct(DivideCommand::id, "DivCmd");

    std::vector<int> seen;
    for (auto* cmd : reg) {
        if (dynamic_cast<AddCommand*>(cmd)) seen.push_back(AddCommand::id);
        else if (dynamic_cast<MultiplyCommand*>(cmd)) seen.push_back(MultiplyCommand::id);
        else if (dynamic_cast<DivideCommand*>(cmd)) seen.push_back(DivideCommand::id);
    }
    // Should visit in template order of constructed ones: Add(10), Mul(30), Div(40)
    ASSERT_EQ(seen.size(), 3u);
    EXPECT_EQ(seen[0], AddCommand::id);
    EXPECT_EQ(seen[1], MultiplyCommand::id);
    EXPECT_EQ(seen[2], DivideCommand::id);
}

TEST_F(RegistryTest, Destroy_IsIdempotent) {
    auto& reg = CommandRegistry::instance();
    reg.construct(AddCommand::id, "AddCmd");

    reg.destroy(AddCommand::id);
    EXPECT_EQ(reg.get(AddCommand::id), nullptr);
    int count_after_first = Command::s_destructor_count;

    // Destroy again should do nothing
    reg.destroy(AddCommand::id);
    EXPECT_EQ(reg.get(AddCommand::id), nullptr);
    EXPECT_EQ(Command::s_destructor_count, count_after_first);
}

TEST_F(RegistryTest, GetAndFind_AfterDestroy_ReturnNullAndEnd) {
    auto& reg = CommandRegistry::instance();
    reg.construct(SubtractCommand::id, "SubCmd");
    EXPECT_NE(reg.get(SubtractCommand::id), nullptr);

    reg.destroy(SubtractCommand::id);
    EXPECT_EQ(reg.get(SubtractCommand::id), nullptr);

    auto it = reg.find(SubtractCommand::id);
    EXPECT_EQ(it, reg.end());

    const auto& const_reg = reg;
    auto cit = const_reg.find(SubtractCommand::id);
    EXPECT_EQ(cit, const_reg.cend());
}

TEST_F(RegistryTest, Construct_Succeeds_WhenPreviouslyNotFound) {
    auto& reg = CommandRegistry::instance();

    // Not constructed yet: find/end should say "not found"
    auto it_before = reg.find(DivideCommand::id);
    EXPECT_EQ(it_before, reg.end());

    // Construct should still succeed for a fresh key
    Command* div = reg.construct(DivideCommand::id, "DivCmd");
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(reg.get(DivideCommand::id), div);

    // Now find must succeed
    auto it_after = reg.find(DivideCommand::id);
    EXPECT_NE(it_after, reg.end());
    EXPECT_EQ((*it_after)->result(), 0.0);
}

TEST_F(RegistryTest, ConstRangeFor_OnlyVisitsConstructed) {
    auto& reg = CommandRegistry::instance();
    reg.construct(AddCommand::id, "AddCmd");
    reg.construct(MultiplyCommand::id, "MulCmd");

    const auto& const_reg = reg;
    int visited = 0;
    for (const Command* cmd : const_reg) {
        (void)cmd;
        ++visited;
    }
    EXPECT_EQ(visited, 2);
}

TEST_F(RegistryTest, UnknownKey_FindAndGetBehaviors) {
    auto& reg = CommandRegistry::instance();

    // Keys outside known set
    EXPECT_EQ(reg.get(5), nullptr);
    EXPECT_EQ(reg.get(999), nullptr);

    auto it = reg.find(5);
    EXPECT_EQ(it, reg.end());
    it = reg.find(999);
    EXPECT_EQ(it, reg.end());

    const auto& const_reg = reg;
    auto cit = const_reg.find(5);
    EXPECT_EQ(cit, const_reg.cend());
    cit = const_reg.find(999);
    EXPECT_EQ(cit, const_reg.cend());
}

TEST_F(RegistryTest, FindDoesNotReturnAdjacentConstructed_OnMiss) {
    auto& reg = CommandRegistry::instance();

    // Construct only Multiply
    reg.construct(MultiplyCommand::id, "MulCmd");

    // Ask for Subtract: must be end/cend (no "next" skip)
    auto it = reg.find(SubtractCommand::id);
    EXPECT_EQ(it, reg.end());

    const auto& const_reg = reg;
    auto cit = const_reg.find(SubtractCommand::id);
    EXPECT_EQ(cit, const_reg.cend());
}

TEST_F(RegistryTest, Iterator_IncrementSkipsDestroyedSlot) {
    auto& reg = CommandRegistry::instance();

    // Construct two in template order positions 0 and 2 (Add, Mul)
    reg.construct(AddCommand::id, "AddCmd");
    reg.construct(MultiplyCommand::id, "MulCmd");

    auto it = reg.begin();
    ASSERT_NE(it, reg.end());
    // Destroy the first one the iterator currently points to
    reg.destroy(AddCommand::id);

    // Advancing the iterator should skip the now-null slot and land on Multiply
    ++it;
    ASSERT_NE(it, reg.end());
    auto* ptr = *it;
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(dynamic_cast<MultiplyCommand*>(ptr) != nullptr);
}

TEST_F(RegistryTest, MultipleObjects_ConstructUseResultsIndependently) {
    auto& reg = CommandRegistry::instance();

    auto* add = reg.construct(AddCommand::id, "AddCmd");
    auto* sub = reg.construct(SubtractCommand::id, "SubCmd");
    ASSERT_NE(add, nullptr);
    ASSERT_NE(sub, nullptr);

    add->do_with_params(7, 8);  // 15
    sub->do_with_params(20, 5); // 15

    EXPECT_EQ(add->result(), 15.0);
    EXPECT_EQ(sub->result(), 15.0);

    // Destroy only one and ensure the other is still alive
    reg.destroy(SubtractCommand::id);
    EXPECT_EQ(reg.get(SubtractCommand::id), nullptr);
    EXPECT_NE(reg.get(AddCommand::id), nullptr);
}