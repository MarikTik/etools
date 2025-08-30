#include <gtest/gtest.h>
#include <cstdint>
#include <memory>
#include <array>
#include <string>

#include <etools/factories/static_factory.hpp>
#include <etools/meta/typelist.hpp>
#include <etools/memory/slot.hpp>

using namespace etools;
using factories::static_factory;

// ----------------- Base & extractor -----------------

struct Base {
  virtual ~Base() = default;
  virtual const char* tag() const noexcept = 0;
};

template<class T>
struct key_extractor {
  static constexpr auto value = T::key;
};

// RAII cleaner: destroys involved static slots after each test
template<class... Ts>
struct Clean {
  ~Clean() { (memory::slot<Ts>::instance().destroy(), ...); }
};

// ----------------- Derived types -----------------

struct A : Base {
  static constexpr std::uint8_t key = 2;
  int constructed = 0;
  A() noexcept { ++constructed; }
  ~A() override = default;
  const char* tag() const noexcept override { return "A"; }
};

struct B : Base {
  static constexpr std::uint8_t key = 5;
  static inline int ctor_calls = 0;
  static inline int dtor_calls = 0;
  int value = 0;
  explicit B(int v) noexcept : value(v) { ++ctor_calls; }
  ~B() override { ++dtor_calls; }
  const char* tag() const noexcept override { return "B"; }
  static void reset_counts() noexcept { ctor_calls = dtor_calls = 0; }
};

struct C : Base {
  static constexpr std::uint8_t key = 7;
  std::string s;
  bool moved = false;
  C(const std::string& ss) : s(ss), moved(false) {}
  C(std::string&& ss) noexcept : s(std::move(ss)), moved(true) {}
  ~C() override = default;
  const char* tag() const noexcept override { return "C"; }
};

struct D : Base {
  static constexpr std::uint8_t key = 9;
  int payload = -1;
  std::unique_ptr<int> keep;          // owns the resource after move

  explicit D(std::unique_ptr<int>&& p) noexcept
    : payload(p ? *p : -1), keep(std::move(p)) {}
   const char* tag() const noexcept override { return "D"; }
};

struct E : Base {
  static constexpr std::uint8_t key = 0; // boundary
  std::string s;
  bool took_move = false;
  E(const std::string& ss) : s(ss), took_move(false) {}
  E(std::string&& ss) : s(std::move(ss)), took_move(true) {}
  ~E() override = default;
  const char* tag() const noexcept override { return "E"; }
};

struct F : Base {
  static constexpr std::uint8_t key = 255; // boundary
  std::array<int, 64> buf{};
  explicit F(const std::array<int,64>& a) noexcept : buf(a) {}
  ~F() override = default;
  const char* tag() const noexcept override { return "F"; }
};

struct G : Base {
  static constexpr std::uint16_t key = 42;
  G() noexcept = default;
  ~G() override = default;
  const char* tag() const noexcept override { return "G"; }
};

struct H : Base {
  static constexpr std::uint16_t key = 200;
  int a{};
  double b{};
  H(int aa, double bb) noexcept : a(aa), b(bb) {}
  ~H() override = default;
  const char* tag() const noexcept override { return "H"; }
};

struct I : Base {
  static constexpr std::uint16_t key = 11;
  I() = default;
  I(const I&) = delete;
  I(I&&) = delete;
  ~I() override = default;
  const char* tag() const noexcept override { return "I"; }
};

template<std::uint16_t K>
struct TType : Base {
  static constexpr std::uint16_t key = K;
  int v = static_cast<int>(K);
  ~TType() override = default;
  const char* tag() const noexcept override { return "T"; }
};

// ----------------- Factory alias -----------------

template<class... Ts>
using Factory = static_factory<Base, key_extractor, meta::typelist<Ts...>>;

// ----------------- Tests (unique names) -----------------

TEST(StaticFactory, Emplace_DefaultCtor) {
  Clean<A> _c;
  using F = Factory<A>;
  F factory;

  Base* p = factory.emplace(A::key);
  ASSERT_NE(p, nullptr);
  EXPECT_STREQ(p->tag(), "A");
  auto* a = dynamic_cast<A*>(p);
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->constructed, 1);
}

TEST(StaticFactory, Emplace_UnknownKey_ReturnsNull) {
  Clean<A,B> _c;
  using F = Factory<A,B>;
  F factory;

  Base* p = factory.emplace(static_cast<std::uint8_t>(99));
  EXPECT_EQ(p, nullptr);
}

TEST(StaticFactory, Emplace_IntArg_StoresValue) {
  Clean<B> _c;
  B::reset_counts();
  using F = Factory<B>;
  F factory;

  Base* p = factory.emplace(B::key, 123);
  ASSERT_NE(p, nullptr);
  auto* b = dynamic_cast<B*>(p);
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->value, 123);
  EXPECT_EQ(B::ctor_calls, 1);
  EXPECT_EQ(B::dtor_calls, 0);
}

TEST(StaticFactory, Emplace_StringLvalue_NoMove) {
  Clean<C> _c;
  using F = Factory<C>;
  F factory;

  std::string s = "hello";
  Base* p = factory.emplace(C::key, s);
  ASSERT_NE(p, nullptr);
  auto* c = dynamic_cast<C*>(p);
  ASSERT_NE(c, nullptr);
  EXPECT_FALSE(c->moved);
  EXPECT_EQ(c->s, "hello");
}

TEST(StaticFactory, Emplace_StringRvalue_Moves) {
  Clean<C> _c;
  using F = Factory<C>;
  F factory;

  Base* p = factory.emplace(C::key, std::string("world"));
  ASSERT_NE(p, nullptr);
  auto* c = dynamic_cast<C*>(p);
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->moved);
  EXPECT_EQ(c->s, "world");
}

TEST(StaticFactory, Emplace_MoveOnlyUniquePtr_Works) {
  Clean<D> _c;
  using F = Factory<D>;
  F factory;

  auto up = std::make_unique<int>(7);
  Base* p = factory.emplace(D::key, std::move(up));
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(up.get(), nullptr);
  auto* d = dynamic_cast<D*>(p);
  ASSERT_NE(d, nullptr);
  EXPECT_EQ(d->payload, 7);
}

TEST(StaticFactory, Emplace_SparseKeys_ThreeTypes) {
  Clean<A,C,G> _c;
  using F = Factory<A,C,G>;
  F factory;

  Base* p1 = factory.emplace(A::key);
  Base* p2 = factory.emplace(C::key, std::string("x"));
  Base* p3 = factory.emplace(G::key);
  ASSERT_NE(p1, nullptr);
  ASSERT_NE(p2, nullptr);
  ASSERT_NE(p3, nullptr);
  EXPECT_STREQ(p1->tag(), "A");
  EXPECT_STREQ(p2->tag(), "C");
  EXPECT_STREQ(p3->tag(), "G");
}

TEST(StaticFactory, Emplace_Replacement_DestroysThenConstructs) {
  Clean<B> _c;
  B::reset_counts();
  using F = Factory<B>;
  F factory;

  Base* p1 = factory.emplace(B::key, 1);
  ASSERT_NE(p1, nullptr);
  EXPECT_EQ(B::ctor_calls, 1);
  EXPECT_EQ(B::dtor_calls, 0);

  Base* p2 = factory.emplace(B::key, 2);
  ASSERT_NE(p2, nullptr);
  auto* b = dynamic_cast<B*>(p2);
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->value, 2);
  EXPECT_EQ(B::ctor_calls, 2);
  EXPECT_EQ(B::dtor_calls, 1);
}

TEST(StaticFactory, Emplace_MultipleTypes_Coexist) {
  Clean<A,B,C> _c;
  B::reset_counts();
  using F = Factory<A,B,C>;
  F factory;

  Base* pa = factory.emplace(A::key);
  Base* pb = factory.emplace(B::key, 77);
  Base* pc = factory.emplace(C::key, std::string("z"));

  ASSERT_NE(pa, nullptr);
  ASSERT_NE(pb, nullptr);
  ASSERT_NE(pc, nullptr);

  auto* a = dynamic_cast<A*>(pa);
  auto* b = dynamic_cast<B*>(pb);
  auto* c = dynamic_cast<C*>(pc);
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(b->value, 77);
  EXPECT_EQ(c->s, "z");
}

TEST(StaticFactory, Emplace_BoundaryKey_Zero) {
  Clean<E> _c;
  using F = Factory<E>;
  F factory;

  std::string s = "edge";
  Base* p = factory.emplace(E::key, s);
  ASSERT_NE(p, nullptr);
  auto* e = dynamic_cast<E*>(p);
  ASSERT_NE(e, nullptr);
  EXPECT_FALSE(e->took_move);
  EXPECT_EQ(e->s, "edge");
}

TEST(StaticFactory, Emplace_BoundaryKey_MaxUint8) {
  Clean<F> _c;
  using Fct = Factory<F>;
  Fct factory;

  std::array<int,64> a{};
  for (int i = 0; i < 64; ++i) a[i] = i * i;
  Base* p = factory.emplace(F::key, a);
  ASSERT_NE(p, nullptr);
  auto* f = dynamic_cast<F*>(p);
  ASSERT_NE(f, nullptr);
  EXPECT_EQ(f->buf[10], 100);
  EXPECT_EQ(f->buf[63], 3969);
}

TEST(StaticFactory, Emplace_Overload_LvalueVsRvalue) {
  Clean<E> _c;
  using Fct = Factory<E>;
  Fct factory;

  std::string lv = "lvalue";
  Base* p1 = factory.emplace(E::key, lv);
  Base* p2 = factory.emplace(E::key, std::string("rvalue"));
  ASSERT_NE(p1, nullptr);
  ASSERT_NE(p2, nullptr);
  auto* e2 = dynamic_cast<E*>(p2);
  ASSERT_NE(e2, nullptr);
  EXPECT_TRUE(e2->took_move);
  EXPECT_EQ(e2->s, "rvalue");
}

TEST(StaticFactory, Emplace_BasePtrConvertible_AndDynamicCast) {
  Clean<G> _c;
  using Fct = Factory<G>;
  Fct factory;

  Base* p = factory.emplace(G::key);
  ASSERT_NE(p, nullptr);
  auto* g = dynamic_cast<G*>(p);
  ASSERT_NE(g, nullptr);
  EXPECT_STREQ(g->tag(), "G");
}

TEST(StaticFactory, Emplace_MultiArgCtor_StoresBoth) {
  Clean<H> _c;
  using Fct = Factory<H>;
  Fct factory;

  Base* p = factory.emplace(H::key, 5, 3.5);
  ASSERT_NE(p, nullptr);
  auto* h = dynamic_cast<H*>(p);
  ASSERT_NE(h, nullptr);
  EXPECT_EQ(h->a, 5);
  EXPECT_DOUBLE_EQ(h->b, 3.5);
}

TEST(StaticFactory, Emplace_NonCopyableTypeItself_DefaultConstruct) {
  Clean<I> _c;
  using Fct = Factory<I>;
  Fct factory;

  Base* p = factory.emplace(I::key);
  ASSERT_NE(p, nullptr);
  auto* i = dynamic_cast<I*>(p);
  ASSERT_NE(i, nullptr);
  EXPECT_STREQ(i->tag(), "I");
}

TEST(StaticFactory, Emplace_LargerTypelist_SparseKeys) {
  using Fct = Factory<
    TType<1>,  TType<17>, TType<33>, TType<49>,
    TType<65>, TType<81>, TType<97>, TType<113>,
    TType<129>,TType<145>,TType<161>,TType<177>,
    TType<193>,TType<209>,TType<225>,TType<241>
  >;
  Clean<
    TType<1>,  TType<17>, TType<33>, TType<49>,
    TType<65>, TType<81>, TType<97>, TType<113>,
    TType<129>,TType<145>,TType<161>,TType<177>,
    TType<193>,TType<209>,TType<225>,TType<241>
  > _c;

  Fct factory;
  for (std::uint16_t k : {1, 97, 145, 241}) {
    Base* p = factory.emplace(k);
    ASSERT_NE(p, nullptr) << "key=" << k;
    EXPECT_STREQ(p->tag(), "T");
  }
}

TEST(StaticFactory, Emplace_UnknownKey_InLargeSet_IsNull) {
  using Fct = Factory<
    TType<2>,  TType<18>, TType<34>, TType<50>,
    TType<66>, TType<82>, TType<98>, TType<114>
  >;
  Clean<
    TType<2>,  TType<18>, TType<34>, TType<50>,
    TType<66>, TType<82>, TType<98>, TType<114>
  > _c;

  Fct factory;
  Base* p = factory.emplace(static_cast<std::uint16_t>(999));
  EXPECT_EQ(p, nullptr);
}

TEST(StaticFactory, Emplace_ReplaceRepeatedly_Counts) {
  Clean<B> _c;
  B::reset_counts();
  using F = Factory<B>;
  F factory;

  ASSERT_NE(factory.emplace(B::key, 10), nullptr);
  ASSERT_NE(factory.emplace(B::key, 20), nullptr);
  ASSERT_NE(factory.emplace(B::key, 30), nullptr);
  auto* b = dynamic_cast<B*>(factory.emplace(B::key, 40));
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->value, 40);
  EXPECT_EQ(B::ctor_calls, 4);
  EXPECT_EQ(B::dtor_calls, 3);
}