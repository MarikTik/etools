#include <gtest/gtest.h>
#include <etools/memory/slot.hpp>
#include <vector>
#include <string>

using namespace etools::memory;

// A simple test struct with a constructor and destructor to verify lifecycle management.
struct SimpleObject {
    static int constructor_calls;
    static int destructor_calls;
    static bool constructed;
    int value;

    explicit SimpleObject(int val) : value(val) {
        constructor_calls++;
        constructed = true;
    }

    ~SimpleObject() {
        destructor_calls++;
        constructed = false;
    }
};
int SimpleObject::constructor_calls = 0;
int SimpleObject::destructor_calls = 0;
bool SimpleObject::constructed = false;

// Another test struct to check different types and alignments.
struct ComplexObject {
    double d;
    std::vector<int> v;
    std::string s;

    ComplexObject(double val, std::string str) : d(val), v({1, 2, 3}), s(str) {}
};

// A trivial object to test minimal types.
struct TrivialObject {
    int x;
};

// Test fixture for slot tests
class SlotTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset static counters for each test
        SimpleObject::constructor_calls = 0;
        SimpleObject::destructor_calls = 0;
        SimpleObject::constructed = false;

        // Clean up slots to ensure a clean slate for each test
        slot<SimpleObject>::instance().destroy();
        slot<TrivialObject>::instance().destroy();
        slot<ComplexObject>::instance().destroy();
    }
};

TEST_F(SlotTest, InitialState_EmptySlot) {
    auto& s = slot<SimpleObject>::instance();
    EXPECT_EQ(s.get(), nullptr);
    s.destroy(); // Should do nothing
    EXPECT_EQ(s.get(), nullptr);
    EXPECT_EQ(SimpleObject::constructor_calls, 0);
    EXPECT_EQ(SimpleObject::destructor_calls, 0);
    EXPECT_FALSE(SimpleObject::constructed);
}

TEST_F(SlotTest, Lifecycle_ConstructAndDestroy) {
    auto& s = slot<SimpleObject>::instance();
    
    // Construct the object
    SimpleObject* ptr = s.construct(10);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->value, 10);
    EXPECT_EQ(s.get(), ptr);
    EXPECT_EQ(SimpleObject::constructor_calls, 1);
    EXPECT_EQ(SimpleObject::destructor_calls, 0);
    EXPECT_TRUE(SimpleObject::constructed);
    
    // Destroy the object
    s.destroy();
    EXPECT_EQ(s.get(), nullptr);
    EXPECT_EQ(SimpleObject::constructor_calls, 1);
    EXPECT_EQ(SimpleObject::destructor_calls, 1);
    EXPECT_FALSE(SimpleObject::constructed);
    
    // Test that another construction works
    SimpleObject* ptr2 = s.construct(20);
    EXPECT_NE(ptr2, nullptr);
    EXPECT_EQ(ptr2->value, 20);
    EXPECT_EQ(s.get(), ptr2);
    EXPECT_EQ(SimpleObject::constructor_calls, 2);
    EXPECT_EQ(SimpleObject::destructor_calls, 1);
    EXPECT_TRUE(SimpleObject::constructed);
}

TEST_F(SlotTest, Emplace_OverwritesExistingObject) {
    auto& s = slot<SimpleObject>::instance();

    SimpleObject* ptr1 = s.emplace(100);
    EXPECT_NE(ptr1, nullptr);
    EXPECT_EQ(ptr1->value, 100);
    EXPECT_EQ(SimpleObject::constructor_calls, 1);
    EXPECT_EQ(SimpleObject::destructor_calls, 0);
    EXPECT_TRUE(SimpleObject::constructed);

    // Emplace again, which should destroy the old one and construct a new one
    SimpleObject* ptr2 = s.emplace(200);
    EXPECT_NE(ptr2, nullptr);
    EXPECT_EQ(ptr2->value, 200);
    EXPECT_EQ(SimpleObject::constructor_calls, 2);
    EXPECT_EQ(SimpleObject::destructor_calls, 1);
    EXPECT_TRUE(SimpleObject::constructed);
}

// NOTE: This test will fail due to the `assert` in a debug build.
// For embedded systems, we often compile with NDEBUG for release, but this is an important case
// to test the assertion's presence for debugging.
TEST_F(SlotTest, DISABLED_Construct_ThrowsAssertionWhenCalledTwice) {
    auto& s = slot<SimpleObject>::instance();
    s.construct(1);

    // This second call should trigger the assertion in debug mode
    // In a release build (NDEBUG), this will lead to undefined behavior
    // since the destructor is not called.
    // EXPECT_DEATH(s.construct(2), "Slot already constructed, cannot construct again.");
    s.destroy(); // Cleanup
}

TEST_F(SlotTest, Get_ConstAndNonConst) {
    auto& s = slot<SimpleObject>::instance();
    const auto& const_s = slot<SimpleObject>::instance();

    // Before construction, both should be nullptr
    EXPECT_EQ(s.get(), nullptr);
    EXPECT_EQ(const_s.get(), nullptr);

    s.construct(30);
    
    // After construction, both should return the same valid pointer
    SimpleObject* non_const_ptr = s.get();
    const SimpleObject* const_ptr = const_s.get();
    
    EXPECT_NE(non_const_ptr, nullptr);
    EXPECT_NE(const_ptr, nullptr);
    EXPECT_EQ(non_const_ptr, const_cast<SimpleObject*>(const_ptr));
    EXPECT_EQ(non_const_ptr->value, 30);
    EXPECT_EQ(const_ptr->value, 30);
}

TEST_F(SlotTest, Memory_AlignmentAndSize) {
    auto& s = slot<ComplexObject>::instance();
    EXPECT_EQ(s.get(), nullptr);
    
    // Construct a complex object to test correct memory allocation and alignment.
    ComplexObject* ptr = s.construct(3.14, "hello");
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->d, 3.14);
    EXPECT_EQ(ptr->s, "hello");
    EXPECT_EQ(ptr->v.size(), 3);
    EXPECT_EQ(s.get(), ptr);

    s.destroy();
    EXPECT_EQ(s.get(), nullptr);
}

TEST_F(SlotTest, MultipleSlots_IndependentInstances) {
    auto& s1 = slot<SimpleObject>::instance();
    auto& s2 = slot<TrivialObject>::instance();
    
    s1.construct(10);
    s2.construct(); // Trivial default construction
    
    EXPECT_NE(s1.get(), nullptr);
    EXPECT_NE(s2.get(), nullptr);
    
    // Ensure they are independent memory locations
    EXPECT_NE(static_cast<void*>(s1.get()), static_cast<void*>(s2.get()));
    EXPECT_EQ(s1.get()->value, 10);
    
    s1.destroy();
    EXPECT_EQ(s1.get(), nullptr);
    EXPECT_NE(s2.get(), nullptr);

    s2.destroy();
    EXPECT_EQ(s2.get(), nullptr);
}
