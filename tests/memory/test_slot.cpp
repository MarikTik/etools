#include <gtest/gtest.h>
#include <etools/memory/slot.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

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
        // Order matters: destroy any leftover object from a previous test
        // FIRST, so the destructor's increment of destructor_calls lands on
        // the *previous* test's counters. Only then do we zero out the
        // counters for the current test.
        slot<SimpleObject>::instance().destroy();
        slot<TrivialObject>::instance().destroy();
        slot<ComplexObject>::instance().destroy();

        SimpleObject::constructor_calls = 0;
        SimpleObject::destructor_calls = 0;
        SimpleObject::constructed = false;
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

// --- Singleton identity --------------------------------------------------
//
// Every call to instance() must return the same object. This is the
// post-condition of slot::instance() declared in the header.

TEST_F(SlotTest, Instance_ReturnsSameObjectAcrossCalls) {
    auto& a = slot<SimpleObject>::instance();
    auto& b = slot<SimpleObject>::instance();
    EXPECT_EQ(&a, &b) << "slot::instance() must return a stable reference";
}

TEST_F(SlotTest, Instance_AlignmentMatchesT) {
    // The buffer is alignas(T); the pointer returned by get() must therefore
    // be properly aligned for T. ComplexObject contains std::string and
    // std::vector, which usually require alignment > 1.
    auto& s = slot<ComplexObject>::instance();
    ComplexObject* p = s.construct(1.5, "aligned");
    auto addr = reinterpret_cast<std::uintptr_t>(p);
    EXPECT_EQ(addr % alignof(ComplexObject), 0u)
        << "slot<T>::get() must return an alignof(T)-aligned pointer";
}

// --- emplace() lifecycle accounting --------------------------------------
//
// Lifecycle_ConstructAndDestroy already exercises construct + destroy.
// This test pins down emplace()'s exact dtor/ctor sequence over several
// overwrites — important because emplace() runs destroy() internally, and
// any future refactor of that path must keep the counts honest.

TEST_F(SlotTest, Emplace_RepeatedlyOverwrites_CountsBalance) {
    auto& s = slot<SimpleObject>::instance();

    s.emplace(1);
    s.emplace(2);
    s.emplace(3);
    s.emplace(4);

    EXPECT_EQ(SimpleObject::constructor_calls, 4);
    EXPECT_EQ(SimpleObject::destructor_calls, 3) << "every overwrite destroys the previous object exactly once";
    EXPECT_EQ(s.get()->value, 4);
}

TEST_F(SlotTest, Destroy_OnEmptySlot_DoesNotCallDestructor) {
    // The header documents destroy() as idempotent — calling it on a
    // not-constructed slot must NOT call ~T().
    auto& s = slot<SimpleObject>::instance();
    EXPECT_EQ(s.get(), nullptr);
    EXPECT_EQ(SimpleObject::destructor_calls, 0);

    s.destroy();
    s.destroy();
    s.destroy();

    EXPECT_EQ(SimpleObject::destructor_calls, 0)
        << "destroy() on empty slot must not invoke ~T()";
}

// --- Move-only payload through emplace -----------------------------------

namespace {
    struct move_only_payload {
        std::unique_ptr<int> p;
        explicit move_only_payload(int v) : p(std::make_unique<int>(v)) {}
        move_only_payload(move_only_payload&&) = default;
        move_only_payload& operator=(move_only_payload&&) = default;
        move_only_payload(const move_only_payload&) = delete;
        move_only_payload& operator=(const move_only_payload&) = delete;
    };
}

TEST_F(SlotTest, Emplace_PerfectForwarding_MoveOnlyType) {
    // Verifies that arguments propagate by value-category through emplace's
    // forwarding-reference parameter pack. If forwarding were broken the
    // unique_ptr move would fail to compile.
    auto& s = slot<move_only_payload>::instance();

    auto src = std::make_unique<int>(42);
    s.construct(std::move(*src)); // emplace from an rvalue int
    // The constructor copied the int into a new unique_ptr internally.
    ASSERT_NE(s.get(), nullptr);
    EXPECT_NE(s.get()->p, nullptr);
    EXPECT_EQ(*s.get()->p, 42);

    s.destroy();
}

// --- Compile-time properties of slot -------------------------------------

namespace {
    struct throwing_dtor {
        ~throwing_dtor() noexcept(false) {}
    };
    struct nothrow_dtor {
        ~nothrow_dtor() = default;
    };
}

// slot<throwing_dtor> would fail the class-level static_assert and is
// therefore impossible to instantiate. We assert the *positive* case
// here: well-formed types pass the destructor-noexcept gate.
static_assert(std::is_nothrow_destructible_v<nothrow_dtor>,
    "sanity: nothrow_dtor must qualify for slot<>");
static_assert(!std::is_nothrow_destructible_v<throwing_dtor>,
    "sanity: throwing_dtor must NOT qualify for slot<>");

TEST(SlotCompile, GetReturnsPointerToT) {
    static_assert(std::is_same_v<
        decltype(std::declval<slot<SimpleObject>&>().get()),
        SimpleObject*>,
        "non-const get() must return SimpleObject*");
    static_assert(std::is_same_v<
        decltype(std::declval<const slot<SimpleObject>&>().get()),
        const SimpleObject*>,
        "const get() must return const SimpleObject*");
}

TEST(SlotCompile, InstanceReturnsSlotReference) {
    static_assert(std::is_same_v<
        decltype(slot<SimpleObject>::instance()),
        slot<SimpleObject>&>,
        "instance() must return slot<T>&");
}
