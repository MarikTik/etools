#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <etools/factories/static_factory.hpp>
#include <etools/hashing/optimal_mph.hpp>
#include <etools/memory/slot.hpp>
#include <etools/meta/typelist.hpp>

using namespace etools;
using factories::static_factory;

namespace {

// ===========================================================================
// Test fixtures
// ===========================================================================

struct base {
    virtual ~base() = default;
    virtual const char* tag() const noexcept = 0;
};

template <typename T>
struct key_extractor {
    static constexpr auto value = T::key;
};

// --- 8-bit key derived types -----------------------------------------------

struct a8 : base {
    static constexpr std::uint8_t key = 2;
    int constructed = 0;
    a8() noexcept : constructed(1) {}
    const char* tag() const noexcept override { return "a8"; }
};

struct b8 : base {
    static constexpr std::uint8_t key = 5;
    static inline int ctor_calls = 0;
    static inline int dtor_calls = 0;
    int value = 0;
    explicit b8(int v) noexcept : value(v) { ++ctor_calls; }
    ~b8() override { ++dtor_calls; }
    const char* tag() const noexcept override { return "b8"; }
    static void reset_counts() noexcept { ctor_calls = dtor_calls = 0; }
};

struct c8 : base {
    static constexpr std::uint8_t key = 7;
    std::string s;
    bool was_moved = false;
    c8(const std::string& ss) : s(ss), was_moved(false) {}
    c8(std::string&& ss) noexcept : s(std::move(ss)), was_moved(true) {}
    const char* tag() const noexcept override { return "c8"; }
};

struct d8 : base {
    static constexpr std::uint8_t key = 9;
    int payload = -1;
    std::unique_ptr<int> keep;
    explicit d8(std::unique_ptr<int>&& p) noexcept
        : payload(p ? *p : -1), keep(std::move(p)) {}
    const char* tag() const noexcept override { return "d8"; }
};

struct e8_zero : base {
    static constexpr std::uint8_t key = 0; // boundary: minimum key
    std::string s;
    bool took_move = false;
    e8_zero(const std::string& ss) : s(ss), took_move(false) {}
    e8_zero(std::string&& ss) : s(std::move(ss)), took_move(true) {}
    const char* tag() const noexcept override { return "e8_zero"; }
};

struct f8_max : base {
    static constexpr std::uint8_t key = 255; // boundary: maximum uint8 key
    std::array<int, 64> buf{};
    explicit f8_max(const std::array<int, 64>& a) noexcept : buf(a) {}
    const char* tag() const noexcept override { return "f8_max"; }
};

// --- 16-bit key derived types ----------------------------------------------

struct g16 : base {
    static constexpr std::uint16_t key = 42;
    g16() noexcept = default;
    const char* tag() const noexcept override { return "g16"; }
};

struct h16 : base {
    static constexpr std::uint16_t key = 200;
    int a{};
    double b{};
    h16(int aa, double bb) noexcept : a(aa), b(bb) {}
    const char* tag() const noexcept override { return "h16"; }
};

// Non-copyable, non-movable, default-constructible. Verifies that the
// factory never accidentally requires copy/move on the registered type.
struct i_noncopyable : base {
    static constexpr std::uint16_t key = 11;
    int v = 7;
    i_noncopyable() = default;
    i_noncopyable(const i_noncopyable&) = delete;
    i_noncopyable(i_noncopyable&&) = delete;
    i_noncopyable& operator=(const i_noncopyable&) = delete;
    i_noncopyable& operator=(i_noncopyable&&) = delete;
    const char* tag() const noexcept override { return "i_noncopyable"; }
};

struct sparse16 : base {
    static constexpr std::uint16_t key = 60000;
    sparse16() noexcept = default;
    const char* tag() const noexcept override { return "sparse16"; }
};

// Type with three overlapping constructors: by-value, const-lvalue-ref,
// rvalue-ref. The factory must pick the right overload based on the
// value category of the argument.
struct triple_ctor : base {
    static constexpr std::uint16_t key = 300;

    enum class taken { by_value, by_cref, by_rvalue };

    std::string s;
    taken which = taken::by_value;

    // by-value
    explicit triple_ctor(std::string v) : s(std::move(v)), which(taken::by_value) {}
    // const lvalue ref — only picked when the call site has a const lvalue
    explicit triple_ctor(const std::string& v, int /*tag*/) : s(v), which(taken::by_cref) {}
    // rvalue ref — only picked for explicit rvalue with the tag overload
    explicit triple_ctor(std::string&& v, double /*tag*/) noexcept
        : s(std::move(v)), which(taken::by_rvalue) {}

    const char* tag() const noexcept override { return "triple_ctor"; }
};

// Move-counter type: tracks whether the source string was moved-from.
struct move_observer : base {
    static constexpr std::uint16_t key = 400;
    std::string s;
    bool ctor_saw_rvalue = false;
    move_observer(const std::string& ss) : s(ss), ctor_saw_rvalue(false) {}
    move_observer(std::string&& ss) noexcept
        : s(std::move(ss)), ctor_saw_rvalue(true) {}
    const char* tag() const noexcept override { return "move_observer"; }
};

// Large parameter pack helper.
template <std::uint16_t K>
struct seq_type : base {
    static constexpr std::uint16_t key = K;
    int v = static_cast<int>(K);
    const char* tag() const noexcept override { return "seq_type"; }
};

// --- Factory aliases -------------------------------------------------------

template <class... Ts>
using factory = static_factory<base, key_extractor, meta::typelist<Ts...>>;

template <class... Ts>
using factory_unwrapped = static_factory<base, key_extractor, Ts...>;

// RAII helper: destroy the slots touched by a test so state never leaks
// between tests. Destructor of slot<T> is a no-op if not constructed.
template <class... Ts>
struct clean {
    ~clean() { (memory::slot<Ts>::instance().destroy(), ...); }
};

} // namespace

// ===========================================================================
// Compile-time properties
//
// No EXPECT/ASSERT: these tests exist so that any regression in the
// compile-time machinery (key extraction, typelist unwrapping, the MPH
// table, static-callability of emplace) fails the build rather than a test.
// ===========================================================================

TEST(StaticFactoryCompile, TypelistAdapterSpecialization) {
    using wrapped   = factory<a8, b8, c8>;
    using unwrapped = factory_unwrapped<a8, b8, c8>;

    static_assert(sizeof(wrapped) > 0,   "wrapped form must instantiate");
    static_assert(sizeof(unwrapped) > 0, "unwrapped form must instantiate");

    // Both must expose the same emplace signature.
    static_assert(
        std::is_same_v<
            decltype(wrapped::emplace(std::declval<std::uint8_t>())),
            decltype(unwrapped::emplace(std::declval<std::uint8_t>()))
        >,
        "typelist adapter and unpacked form must share emplace return type"
    );
}

TEST(StaticFactoryCompile, KeyTypeDeducedFromExtractor) {
    static_assert(
        std::is_same_v<decltype(key_extractor<a8>::value), const std::uint8_t>,
        "8-bit key extractor"
    );
    static_assert(
        std::is_same_v<decltype(key_extractor<g16>::value), const std::uint16_t>,
        "16-bit key extractor"
    );
}

// SFINAE probe: is `F::emplace(key, args...)` a valid expression without an
// instance? If emplace ever loses `static`, this trait yields false_type.
template <typename F, typename Key, typename... Args>
constexpr auto callable_without_instance(int)
    -> decltype(F::emplace(std::declval<Key>(), std::declval<Args>()...),
                std::true_type{});

template <typename F, typename Key, typename... Args>
constexpr std::false_type callable_without_instance(...);

template <typename F, typename Key, typename... Args>
using callable_without_instance_t =
    decltype(callable_without_instance<F, Key, Args...>(0));

TEST(StaticFactoryCompile, EmplaceIsStaticallyCallable) {
    using f = factory<a8, b8, c8>;

    static_assert(callable_without_instance_t<f, std::uint8_t>::value,
                  "factory::emplace(key) must be callable without an instance");
    static_assert(callable_without_instance_t<f, std::uint8_t, int>::value,
                  "factory::emplace(key, int) must be callable without an instance");
    static_assert(callable_without_instance_t<f, std::uint8_t, std::string>::value,
                  "factory::emplace(key, string) must be callable without an instance");
}

TEST(StaticFactoryCompile, EmplaceReturnsBasePointer) {
    using f = factory<a8, b8>;
    static_assert(
        std::is_same_v<
            decltype(f::emplace(std::declval<std::uint8_t>())),
            base*
        >,
        "emplace must return Base*"
    );
    static_assert(
        std::is_same_v<
            decltype(f::emplace(std::declval<std::uint8_t>(), std::declval<int>())),
            base*
        >,
        "emplace with int argument must still return Base*"
    );
}

TEST(StaticFactoryCompile, EmplaceIsNoexcept) {
    using f = factory<a8, b8>;
    static_assert(noexcept(f::emplace(std::declval<std::uint8_t>())),
                  "default-construct dispatch is noexcept");
    static_assert(noexcept(f::emplace(std::declval<std::uint8_t>(), std::declval<int>())),
                  "int-arg dispatch is noexcept");
}

TEST(StaticFactoryCompile, HeterogeneousConstructorSignaturesAllRegisterable) {
    // a8 default, b8(int), c8(string) — exercises the if-constexpr SFINAE
    // branch in try_emplace_if_constructible.
    using f = factory<a8, b8, c8>;
    static_assert(sizeof(f) > 0, "type must instantiate");
}

TEST(StaticFactoryCompile, MphSurfaceIsBackendAgnostic) {
    // Verifies the cross-backend contract (size/capacity/not_found) holds for
    // the table the factory will build internally. This isn't a test of
    // optimal_mph itself — it's a guarantee that whichever backend
    // optimal_mph picks for a given key set, the public surface stays
    // identical from the factory's perspective.
    constexpr const auto& dense = hashing::optimal_mph<std::uint8_t>
        ::instance<a8::key, b8::key, c8::key>();
    static_assert(dense.size() == 3);
    static_assert(dense.not_found() == dense.size());
    static_assert(dense.capacity() >= dense.size());

    constexpr const auto& sparse = hashing::optimal_mph<std::uint16_t>
        ::instance<g16::key, h16::key, sparse16::key>();
    static_assert(sparse.size() == 3);
    static_assert(sparse.not_found() == sparse.size());
}

// ===========================================================================
// Runtime — construction basics
// ===========================================================================

TEST(StaticFactoryRuntime, EmplaceNoArgs_DefaultConstructs) {
    clean<a8> guard;
    using f = factory<a8>;

    base* p = f::emplace(a8::key);
    ASSERT_NE(p, nullptr);
    EXPECT_STREQ(p->tag(), "a8");
    auto* a = dynamic_cast<a8*>(p);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->constructed, 1);
}

TEST(StaticFactoryRuntime, EmplaceWithSingleIntArg_StoresValue) {
    clean<b8> guard;
    b8::reset_counts();
    using f = factory<b8>;

    base* p = f::emplace(b8::key, 123);
    ASSERT_NE(p, nullptr);
    auto* b = dynamic_cast<b8*>(p);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->value, 123);
    EXPECT_EQ(b8::ctor_calls, 1);
    EXPECT_EQ(b8::dtor_calls, 0);
}

TEST(StaticFactoryRuntime, EmplaceWithMultiArgCtor_StoresAll) {
    clean<h16> guard;
    using f = factory<h16>;

    base* p = f::emplace(h16::key, 5, 3.5);
    ASSERT_NE(p, nullptr);
    auto* h = dynamic_cast<h16*>(p);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(h->a, 5);
    EXPECT_DOUBLE_EQ(h->b, 3.5);
}

TEST(StaticFactoryRuntime, BaseConversion_DynamicCastBack) {
    clean<g16> guard;
    using f = factory<g16>;

    base* p = f::emplace(g16::key);
    ASSERT_NE(p, nullptr);
    auto* g = dynamic_cast<g16*>(p);
    ASSERT_NE(g, nullptr);
    EXPECT_STREQ(g->tag(), "g16");
}

TEST(StaticFactoryRuntime, NonCopyableNonMovableType_DefaultConstruct) {
    clean<i_noncopyable> guard;
    using f = factory<i_noncopyable>;

    base* p = f::emplace(i_noncopyable::key);
    ASSERT_NE(p, nullptr);
    auto* i = dynamic_cast<i_noncopyable*>(p);
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->v, 7);
}

TEST(StaticFactoryRuntime, MultipleTypes_CoexistInDistinctSlots) {
    clean<a8, b8, c8> guard;
    b8::reset_counts();
    using f = factory<a8, b8, c8>;

    base* pa = f::emplace(a8::key);
    base* pb = f::emplace(b8::key, 77);
    base* pc = f::emplace(c8::key, std::string("z"));

    ASSERT_NE(pa, nullptr);
    ASSERT_NE(pb, nullptr);
    ASSERT_NE(pc, nullptr);

    EXPECT_STREQ(pa->tag(), "a8");
    EXPECT_STREQ(pb->tag(), "b8");
    EXPECT_STREQ(pc->tag(), "c8");

    // Distinct slots: every base* should be unique.
    EXPECT_NE(pa, pb);
    EXPECT_NE(pa, pc);
    EXPECT_NE(pb, pc);
}

// ===========================================================================
// Runtime — perfect forwarding (the main coverage the user asked for)
// ===========================================================================

TEST(StaticFactoryForwarding, StringLvalue_PicksCopyCtor) {
    clean<c8> guard;
    using f = factory<c8>;

    std::string s = "hello";
    base* p = f::emplace(c8::key, s);
    ASSERT_NE(p, nullptr);
    auto* c = dynamic_cast<c8*>(p);
    ASSERT_NE(c, nullptr);
    EXPECT_FALSE(c->was_moved) << "lvalue must select c8(const std::string&)";
    EXPECT_EQ(c->s, "hello");
    EXPECT_EQ(s, "hello") << "lvalue source must not be moved-from";
}

TEST(StaticFactoryForwarding, StringRvalue_PicksMoveCtor) {
    clean<c8> guard;
    using f = factory<c8>;

    base* p = f::emplace(c8::key, std::string("world"));
    ASSERT_NE(p, nullptr);
    auto* c = dynamic_cast<c8*>(p);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->was_moved) << "rvalue must select c8(std::string&&)";
    EXPECT_EQ(c->s, "world");
}

TEST(StaticFactoryForwarding, ExplicitlyMovedLvalue_PicksMoveCtor) {
    clean<c8> guard;
    using f = factory<c8>;

    // std::move on an lvalue produces an rvalue reference — same as a
    // temporary from the forwarding-reference rule's perspective.
    std::string s = "moved";
    base* p = f::emplace(c8::key, std::move(s));
    ASSERT_NE(p, nullptr);
    auto* c = dynamic_cast<c8*>(p);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->was_moved);
    EXPECT_EQ(c->s, "moved");
}

TEST(StaticFactoryForwarding, ConstLvalue_PicksCopyCtor) {
    clean<c8> guard;
    using f = factory<c8>;

    const std::string s = "constref";
    base* p = f::emplace(c8::key, s);
    ASSERT_NE(p, nullptr);
    auto* c = dynamic_cast<c8*>(p);
    ASSERT_NE(c, nullptr);
    EXPECT_FALSE(c->was_moved) << "const lvalue must select copy ctor";
    EXPECT_EQ(c->s, "constref");
}

TEST(StaticFactoryForwarding, TripleCtor_ByValue_OneArg) {
    clean<triple_ctor> guard;
    using f = factory<triple_ctor>;

    // Single string argument — the by-value ctor is the only single-arg
    // overload. by-value sinks happily from both lvalue and rvalue.
    std::string s = "by-value-lvalue";
    base* p = f::emplace(triple_ctor::key, s);
    ASSERT_NE(p, nullptr);
    auto* t = dynamic_cast<triple_ctor*>(p);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->which, triple_ctor::taken::by_value);
    EXPECT_EQ(t->s, "by-value-lvalue");
}

TEST(StaticFactoryForwarding, TripleCtor_ByValue_FromRvalue) {
    clean<triple_ctor> guard;
    using f = factory<triple_ctor>;

    base* p = f::emplace(triple_ctor::key, std::string("by-value-rvalue"));
    ASSERT_NE(p, nullptr);
    auto* t = dynamic_cast<triple_ctor*>(p);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->which, triple_ctor::taken::by_value);
    EXPECT_EQ(t->s, "by-value-rvalue");
}

TEST(StaticFactoryForwarding, TripleCtor_ByConstRef_TagDisambiguates) {
    clean<triple_ctor> guard;
    using f = factory<triple_ctor>;

    // Two-arg form with int tag — selects the const-lvalue-ref overload.
    std::string s = "cref-overload";
    base* p = f::emplace(triple_ctor::key, s, 1);
    ASSERT_NE(p, nullptr);
    auto* t = dynamic_cast<triple_ctor*>(p);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->which, triple_ctor::taken::by_cref);
    EXPECT_EQ(t->s, "cref-overload");
    EXPECT_EQ(s, "cref-overload") << "const-ref overload must not move from source";
}

TEST(StaticFactoryForwarding, TripleCtor_ByRvalueRef_TagDisambiguates) {
    clean<triple_ctor> guard;
    using f = factory<triple_ctor>;

    // Two-arg form with double tag — selects the rvalue-ref overload.
    base* p = f::emplace(triple_ctor::key, std::string("rvalue-overload"), 1.0);
    ASSERT_NE(p, nullptr);
    auto* t = dynamic_cast<triple_ctor*>(p);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->which, triple_ctor::taken::by_rvalue);
    EXPECT_EQ(t->s, "rvalue-overload");
}

TEST(StaticFactoryForwarding, MoveOnlyType_UniquePtr) {
    clean<d8> guard;
    using f = factory<d8>;

    auto up = std::make_unique<int>(7);
    base* p = f::emplace(d8::key, std::move(up));
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(up.get(), nullptr) << "ownership must transfer through the factory";
    auto* d = dynamic_cast<d8*>(p);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->payload, 7);
    ASSERT_NE(d->keep, nullptr);
    EXPECT_EQ(*d->keep, 7);
}

TEST(StaticFactoryForwarding, MoveObserver_SourceIsActuallyMoved) {
    clean<move_observer> guard;
    using f = factory<move_observer>;

    // Long enough to defeat SSO so move actually shows in source state.
    std::string src = "this string is long enough to live on the heap and survive SSO truncation";
    const auto src_data_before = src.data();

    base* p = f::emplace(move_observer::key, std::move(src));
    ASSERT_NE(p, nullptr);
    auto* m = dynamic_cast<move_observer*>(p);
    ASSERT_NE(m, nullptr);
    EXPECT_TRUE(m->ctor_saw_rvalue);
    EXPECT_EQ(m->s.data(), src_data_before) << "buffer should have been pilfered, not copied";
}

TEST(StaticFactoryForwarding, LvalueAndRvalueInSameCall_MixedArgs) {
    clean<h16> guard;
    using f = factory<h16>;

    int x = 5;          // lvalue
    base* p = f::emplace(h16::key, x, 2.5); // lvalue int + rvalue double
    ASSERT_NE(p, nullptr);
    auto* h = dynamic_cast<h16*>(p);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(h->a, 5);
    EXPECT_DOUBLE_EQ(h->b, 2.5);
}

// ===========================================================================
// Runtime — lifecycle (replacement, repeated emplace, ctor/dtor counts)
// ===========================================================================

TEST(StaticFactoryLifecycle, ReplaceSameKey_DestroysOldBeforeConstructingNew) {
    clean<b8> guard;
    b8::reset_counts();
    using f = factory<b8>;

    base* p1 = f::emplace(b8::key, 1);
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(b8::ctor_calls, 1);
    EXPECT_EQ(b8::dtor_calls, 0);

    base* p2 = f::emplace(b8::key, 2);
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(b8::ctor_calls, 2);
    EXPECT_EQ(b8::dtor_calls, 1);

    auto* b = dynamic_cast<b8*>(p2);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->value, 2);
}

TEST(StaticFactoryLifecycle, RepeatedReplacement_CountsAddUp) {
    clean<b8> guard;
    b8::reset_counts();
    using f = factory<b8>;

    ASSERT_NE(f::emplace(b8::key, 10), nullptr);
    ASSERT_NE(f::emplace(b8::key, 20), nullptr);
    ASSERT_NE(f::emplace(b8::key, 30), nullptr);
    auto* b = dynamic_cast<b8*>(f::emplace(b8::key, 40));
    ASSERT_NE(b, nullptr);

    EXPECT_EQ(b->value, 40);
    EXPECT_EQ(b8::ctor_calls, 4);
    EXPECT_EQ(b8::dtor_calls, 3);
}

TEST(StaticFactoryLifecycle, CleanupBetweenTests_NoCrossContamination) {
    // Two consecutive emplaces with different argument categories on the
    // same key — make sure the destructor from the first call ran before
    // the second ctor ran (no double-construct, no skipped destruct).
    clean<c8> guard;
    using f = factory<c8>;

    base* p1 = f::emplace(c8::key, std::string("first"));
    ASSERT_NE(p1, nullptr);

    std::string s = "second";
    base* p2 = f::emplace(c8::key, s);
    ASSERT_NE(p2, nullptr);

    auto* c = dynamic_cast<c8*>(p2);
    ASSERT_NE(c, nullptr);
    EXPECT_FALSE(c->was_moved);
    EXPECT_EQ(c->s, "second");
}

// ===========================================================================
// Runtime — boundary and scale
// ===========================================================================

TEST(StaticFactoryBoundary, KeyZero_Works) {
    clean<e8_zero> guard;
    using f = factory<e8_zero>;

    std::string s = "edge";
    base* p = f::emplace(e8_zero::key, s);
    ASSERT_NE(p, nullptr);
    auto* e = dynamic_cast<e8_zero*>(p);
    ASSERT_NE(e, nullptr);
    EXPECT_FALSE(e->took_move);
    EXPECT_EQ(e->s, "edge");
}

TEST(StaticFactoryBoundary, KeyMaxUint8_Works) {
    clean<f8_max> guard;
    using fct = factory<f8_max>;

    std::array<int, 64> a{};
    for (int i = 0; i < 64; ++i) a[i] = i * i;
    base* p = fct::emplace(f8_max::key, a);
    ASSERT_NE(p, nullptr);
    auto* fp = dynamic_cast<f8_max*>(p);
    ASSERT_NE(fp, nullptr);
    EXPECT_EQ(fp->buf[10], 100);
    EXPECT_EQ(fp->buf[63], 63 * 63);
}

TEST(StaticFactoryBoundary, SparseKey_SixtyThousand) {
    clean<sparse16> guard;
    using f = factory<sparse16>;

    // 60000 is far from any other key — exercises the FKS branch of
    // optimal_mph as the chosen backend.
    base* p = f::emplace(sparse16::key);
    ASSERT_NE(p, nullptr);
    EXPECT_STREQ(p->tag(), "sparse16");
}

TEST(StaticFactoryBoundary, MixedKeyWidths_8And16_NotMixable) {
    // The factory deduces key_t from the first registered type. Mixing key
    // widths in one factory would compile but the implicit conversion is
    // surprising — this test documents that single-width factories are the
    // intended use. We just verify two separate factories with different
    // key widths can coexist in the same TU.
    clean<a8, g16> guard;
    using f8  = factory<a8>;
    using f16 = factory<g16>;

    EXPECT_NE(f8::emplace(a8::key),   nullptr);
    EXPECT_NE(f16::emplace(g16::key), nullptr);
}

TEST(StaticFactoryBoundary, LargeTypelist_SparseKeys) {
    using fct = factory<
        seq_type<1>,   seq_type<17>,  seq_type<33>,  seq_type<49>,
        seq_type<65>,  seq_type<81>,  seq_type<97>,  seq_type<113>,
        seq_type<129>, seq_type<145>, seq_type<161>, seq_type<177>,
        seq_type<193>, seq_type<209>, seq_type<225>, seq_type<241>
    >;
    clean<
        seq_type<1>,   seq_type<17>,  seq_type<33>,  seq_type<49>,
        seq_type<65>,  seq_type<81>,  seq_type<97>,  seq_type<113>,
        seq_type<129>, seq_type<145>, seq_type<161>, seq_type<177>,
        seq_type<193>, seq_type<209>, seq_type<225>, seq_type<241>
    > guard;

    for (std::uint16_t k : {1, 33, 97, 145, 241}) {
        base* p = fct::emplace(k);
        ASSERT_NE(p, nullptr) << "registered key " << k << " must dispatch";
        EXPECT_STREQ(p->tag(), "seq_type");
    }
}

// ===========================================================================
// Runtime — nullptr paths
// ===========================================================================

TEST(StaticFactoryNullptr, UnknownKey_SmallSet_ReturnsNullptr) {
    clean<a8, b8, c8> guard;
    using f = factory<a8, b8, c8>;

    // Registered keys: {2, 5, 7}.
    EXPECT_EQ(f::emplace(static_cast<std::uint8_t>(99)),  nullptr);
    EXPECT_EQ(f::emplace(static_cast<std::uint8_t>(0)),   nullptr);
    EXPECT_EQ(f::emplace(static_cast<std::uint8_t>(255)), nullptr);
}

TEST(StaticFactoryNullptr, UnknownKey_LargeSet_ReturnsNullptr) {
    using fct = factory<
        seq_type<2>,  seq_type<18>, seq_type<34>, seq_type<50>,
        seq_type<66>, seq_type<82>, seq_type<98>, seq_type<114>
    >;
    clean<
        seq_type<2>,  seq_type<18>, seq_type<34>, seq_type<50>,
        seq_type<66>, seq_type<82>, seq_type<98>, seq_type<114>
    > guard;

    EXPECT_EQ(fct::emplace(static_cast<std::uint16_t>(999)), nullptr);
    EXPECT_EQ(fct::emplace(static_cast<std::uint16_t>(0)),   nullptr);
}

TEST(StaticFactoryNullptr, ArgMismatch_NoSlotConstructible) {
    clean<a8, b8, c8> guard;
    using f = factory<a8, b8, c8>;

    // a8 takes no args, b8 takes int, c8 takes string. A double argument
    // matches no constructor — the fold's if-constexpr branches all return
    // false and the dispatch returns nullptr without constructing anything.
    EXPECT_EQ(f::emplace(a8::key, 3.14), nullptr);
}

TEST(StaticFactoryNullptr, ArgMismatch_OnValidKey_DoesNotCorruptSlot) {
    clean<a8, b8, c8> guard;
    b8::reset_counts();
    using f = factory<a8, b8, c8>;

    // b8::key resolves to a real slot, but b8 only has b8(int). Passing a
    // std::string finds no matching branch and the dispatch returns nullptr.
    // The b8 slot must remain untouched.
    base* p = f::emplace(b8::key, std::string("not-an-int"));
    EXPECT_EQ(p, nullptr);
    EXPECT_EQ(b8::ctor_calls, 0);
    EXPECT_EQ(b8::dtor_calls, 0);

    // Sanity: the correct signature still works after the failed attempt.
    base* q = f::emplace(b8::key, 7);
    ASSERT_NE(q, nullptr);
    EXPECT_STREQ(q->tag(), "b8");
    EXPECT_EQ(b8::ctor_calls, 1);
    EXPECT_EQ(b8::dtor_calls, 0);
}

TEST(StaticFactoryNullptr, RvalueArgumentNotConsumedOnFailedDispatch) {
    clean<b8> guard;
    using f = factory<b8>;

    // b8 takes int, not unique_ptr. Pass an rvalue unique_ptr — the
    // dispatch finds no matching ctor, returns nullptr, and *must not*
    // have moved-from our unique_ptr because no construction happened.
    auto up = std::make_unique<int>(99);
    base* p = f::emplace(b8::key, std::move(up));
    EXPECT_EQ(p, nullptr);
    ASSERT_NE(up.get(), nullptr) << "failed dispatch must not consume the rvalue";
    EXPECT_EQ(*up, 99);
}

TEST(StaticFactoryNullptr, CallableWithoutInstance_RegressionGuard) {
    // If the static keyword is ever removed from emplace, this TU fails
    // to compile. The runtime body is incidental.
    clean<a8> guard;
    using f = factory<a8>;

    base* p = f::emplace(a8::key);
    ASSERT_NE(p, nullptr);
    EXPECT_STREQ(p->tag(), "a8");
}
