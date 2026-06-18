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

// Test fixture: reset SimpleObject's static counters between tests. Slots are now
// value types owned as locals in each test, so there is no shared state to clean up.
class SlotTest : public ::testing::Test {
protected:
    void SetUp() override {
        SimpleObject::constructor_calls = 0;
        SimpleObject::destructor_calls = 0;
        SimpleObject::constructed = false;
    }
};

// --- Construction / destruction lifecycle --------------------------------

TEST_F(SlotTest, InitialState_EmptySlot) {
    slot<SimpleObject> s;
    EXPECT_FALSE(s.has_value());
    EXPECT_FALSE(static_cast<bool>(s));
    EXPECT_EQ(s.get(), nullptr);
    EXPECT_EQ(SimpleObject::constructor_calls, 0);
    EXPECT_EQ(SimpleObject::destructor_calls, 0);
}

TEST_F(SlotTest, Lifecycle_ConstructAndDestroy) {
    slot<SimpleObject> s;

    SimpleObject* ptr = s.construct(10);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->value, 10);
    EXPECT_TRUE(s.has_value());
    EXPECT_EQ(s.get(), ptr);
    EXPECT_EQ(SimpleObject::constructor_calls, 1);
    EXPECT_EQ(SimpleObject::destructor_calls, 0);

    s.destroy();
    EXPECT_FALSE(s.has_value());
    EXPECT_EQ(s.get(), nullptr);
    EXPECT_EQ(SimpleObject::destructor_calls, 1);

    // Reconstruction after destroy works.
    SimpleObject* ptr2 = s.construct(20);
    EXPECT_NE(ptr2, nullptr);
    EXPECT_EQ(ptr2->value, 20);
    EXPECT_EQ(SimpleObject::constructor_calls, 2);
    EXPECT_EQ(SimpleObject::destructor_calls, 1);
}

TEST_F(SlotTest, RaiiDestructor_DestroysContainedObject) {
    {
        slot<SimpleObject> s;
        s.construct(5);
        EXPECT_EQ(SimpleObject::destructor_calls, 0);
    } // s goes out of scope here
    EXPECT_EQ(SimpleObject::destructor_calls, 1)
        << "~slot must destroy the contained object";
}

TEST_F(SlotTest, RaiiDestructor_EmptySlot_NoDestructorCall) {
    {
        slot<SimpleObject> s;
        // never constructed
    }
    EXPECT_EQ(SimpleObject::destructor_calls, 0);
}

// --- emplace / overwrite semantics ---------------------------------------

TEST_F(SlotTest, Emplace_OverwritesExistingObject) {
    slot<SimpleObject> s;

    SimpleObject* ptr1 = s.emplace(100);
    EXPECT_EQ(ptr1->value, 100);
    EXPECT_EQ(SimpleObject::constructor_calls, 1);
    EXPECT_EQ(SimpleObject::destructor_calls, 0);

    SimpleObject* ptr2 = s.emplace(200);
    EXPECT_EQ(ptr2->value, 200);
    EXPECT_EQ(SimpleObject::constructor_calls, 2);
    EXPECT_EQ(SimpleObject::destructor_calls, 1)
        << "emplace must destroy the previous object exactly once";
}

TEST_F(SlotTest, Emplace_RepeatedlyOverwrites_CountsBalance) {
    slot<SimpleObject> s;
    s.emplace(1);
    s.emplace(2);
    s.emplace(3);
    s.emplace(4);

    EXPECT_EQ(SimpleObject::constructor_calls, 4);
    EXPECT_EQ(SimpleObject::destructor_calls, 3);
    EXPECT_EQ(s.get()->value, 4);
}

TEST_F(SlotTest, Destroy_OnEmptySlot_DoesNotCallDestructor) {
    slot<SimpleObject> s;
    s.destroy();
    s.destroy();
    s.destroy();
    EXPECT_EQ(SimpleObject::destructor_calls, 0);
}

// --- Access surface: get / operator* / operator-> / operator bool ---------

TEST_F(SlotTest, Get_ConstAndNonConst) {
    slot<SimpleObject> s;
    const slot<SimpleObject>& cs = s;

    EXPECT_EQ(s.get(), nullptr);
    EXPECT_EQ(cs.get(), nullptr);

    s.construct(30);

    SimpleObject* non_const_ptr = s.get();
    const SimpleObject* const_ptr = cs.get();
    EXPECT_NE(non_const_ptr, nullptr);
    EXPECT_EQ(non_const_ptr, const_cast<SimpleObject*>(const_ptr));
    EXPECT_EQ(non_const_ptr->value, 30);
}

TEST_F(SlotTest, DereferenceOperators_AccessContainedObject) {
    slot<SimpleObject> s;
    s.construct(77);

    EXPECT_EQ((*s).value, 77);
    EXPECT_EQ(s->value, 77);

    s->value = 88;
    EXPECT_EQ((*s).value, 88);

    const slot<SimpleObject>& cs = s;
    EXPECT_EQ((*cs).value, 88);
    EXPECT_EQ(cs->value, 88);
}

TEST_F(SlotTest, OperatorBool_ReflectsState) {
    slot<SimpleObject> s;
    EXPECT_FALSE(static_cast<bool>(s));
    s.construct(1);
    EXPECT_TRUE(static_cast<bool>(s));
    s.destroy();
    EXPECT_FALSE(static_cast<bool>(s));
}

// --- Independence of separate slot instances -----------------------------

TEST_F(SlotTest, SeparateInstances_AreIndependent) {
    slot<SimpleObject> a;
    slot<SimpleObject> b;

    a.construct(1);
    b.construct(2);

    // Distinct storage, distinct values — the whole point of the value-type rework.
    EXPECT_NE(static_cast<void*>(a.get()), static_cast<void*>(b.get()));
    EXPECT_EQ(a.get()->value, 1);
    EXPECT_EQ(b.get()->value, 2);

    a.destroy();
    EXPECT_FALSE(a.has_value());
    EXPECT_TRUE(b.has_value()) << "destroying one slot must not affect another";
    EXPECT_EQ(b.get()->value, 2);
}

TEST_F(SlotTest, DifferentTypes_IndependentStorage) {
    slot<SimpleObject> s1;
    slot<TrivialObject> s2;

    s1.construct(10);
    s2.emplace();

    EXPECT_NE(static_cast<void*>(s1.get()), static_cast<void*>(s2.get()));
    EXPECT_EQ(s1.get()->value, 10);
}

TEST_F(SlotTest, Alignment_GetReturnsAlignedPointer) {
    slot<ComplexObject> s;
    ComplexObject* p = s.construct(1.5, "aligned");
    auto addr = reinterpret_cast<std::uintptr_t>(p);
    EXPECT_EQ(addr % alignof(ComplexObject), 0u);
}

// --- Move semantics ------------------------------------------------------

TEST_F(SlotTest, MoveConstruct_RelocatesObject_SourceEmptied) {
    slot<SimpleObject> a;
    a.construct(42);

    slot<SimpleObject> b = std::move(a);

    EXPECT_TRUE(b.has_value());
    EXPECT_EQ(b.get()->value, 42) << "value must survive relocation";
    EXPECT_FALSE(a.has_value()) << "moved-from slot must be empty";
    // SimpleObject only counts its int-ctor, not the implicit copy/move used
    // to relocate; so constructor_calls stays at 1. The source object IS
    // destroyed during the move, hence one destructor call.
    EXPECT_EQ(SimpleObject::constructor_calls, 1);
    EXPECT_EQ(SimpleObject::destructor_calls, 1);
}

TEST_F(SlotTest, MoveConstruct_FromEmpty_StaysEmpty) {
    slot<SimpleObject> a;
    slot<SimpleObject> b = std::move(a);
    EXPECT_FALSE(b.has_value());
    EXPECT_FALSE(a.has_value());
    EXPECT_EQ(SimpleObject::constructor_calls, 0);
}

TEST_F(SlotTest, MoveAssign_RelocatesObject_SourceEmptied) {
    slot<SimpleObject> a;
    a.construct(7);

    slot<SimpleObject> b;
    b.construct(99);  // b already holds something; move-assign must destroy it first

    b = std::move(a);

    EXPECT_TRUE(b.has_value());
    EXPECT_EQ(b.get()->value, 7);
    EXPECT_FALSE(a.has_value());
}

TEST_F(SlotTest, MoveAssign_MoveOnlyPayload) {
    slot<std::unique_ptr<int>> a;
    a.construct(std::make_unique<int>(123));

    slot<std::unique_ptr<int>> b;
    b = std::move(a);

    ASSERT_TRUE(b.has_value());
    ASSERT_NE(b.get()->get(), nullptr);
    EXPECT_EQ(**b.get(), 123);
    EXPECT_FALSE(a.has_value());
}

// --- Perfect forwarding into construct -----------------------------------

TEST_F(SlotTest, Construct_PerfectForwarding_StringMove) {
    slot<std::string> s;
    std::string src = "a string long enough to live on the heap and avoid SSO";
    const auto* src_data = src.data();

    s.construct(std::move(src));

    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->data(), src_data) << "buffer should be pilfered, not copied";
}

// --- Compile-time properties ---------------------------------------------

namespace {
    struct nothrow_dtor { ~nothrow_dtor() = default; };
    struct throwing_dtor { ~throwing_dtor() noexcept(false) {} };

    struct nonmovable {
        nonmovable() = default;
        nonmovable(const nonmovable&) = delete;
        nonmovable(nonmovable&&) = delete;
    };
}

static_assert(std::is_nothrow_destructible_v<nothrow_dtor>,
    "sanity: nothrow_dtor qualifies for slot<>");
static_assert(!std::is_nothrow_destructible_v<throwing_dtor>,
    "sanity: throwing_dtor does NOT qualify for slot<>");

TEST(SlotCompile, ValueTypeTraits) {
    // Copy is always deleted.
    static_assert(!std::is_copy_constructible_v<slot<SimpleObject>>,
        "slot is never copyable");
    static_assert(!std::is_copy_assignable_v<slot<SimpleObject>>,
        "slot is never copy-assignable");
}

TEST(SlotCompile, ConditionalMovability_HonestTraits) {
    // Movable T -> movable slot.
    static_assert(std::is_move_constructible_v<slot<std::string>>,
        "slot<movable T> must be move-constructible");
    static_assert(std::is_move_constructible_v<slot<std::unique_ptr<int>>>,
        "slot<move-only T> must be move-constructible");

    // Non-movable T -> non-movable slot (the base-class technique makes the
    // trait report the truth, not just fail at the point of use).
    static_assert(!std::is_move_constructible_v<slot<nonmovable>>,
        "slot<non-movable T> must NOT be move-constructible");
}

TEST(SlotCompile, AccessorReturnTypes) {
    static_assert(std::is_same_v<
        decltype(std::declval<slot<SimpleObject>&>().get()), SimpleObject*>);
    static_assert(std::is_same_v<
        decltype(std::declval<const slot<SimpleObject>&>().get()), const SimpleObject*>);
    static_assert(std::is_same_v<
        decltype(*std::declval<slot<SimpleObject>&>()), SimpleObject&>);
    static_assert(std::is_same_v<
        slot<SimpleObject>::value_type, SimpleObject>);
}
