#include <gtest/gtest.h>
#include <etools/memory/buffer.hpp>
#include <eser/flat/serializer.hpp>
#include <memory>
#include <utility>

struct Message {
    int id;
    float value;
    bool operator==(const Message& other) const {
        return id == other.id && value == other.value;
    }
};
static_assert(std::is_trivially_copyable_v<Message>);
 

TEST(BufferTest, DefaultConstructs_EmptyState) {
    etools::memory::buffer<> b;
    EXPECT_EQ(b.data(), nullptr);
    EXPECT_EQ(b.size(), 0u);
    EXPECT_EQ(b.capacity(), 0u);
}

TEST(BufferTest, ParameterizedConstructor_Positive) {
    const size_t capacity = 16;
    auto data = std::make_unique<std::byte[]>(capacity);
    etools::memory::buffer e(std::move(data), capacity);
    EXPECT_NE(e.data(), nullptr);
    EXPECT_EQ(e.capacity(), capacity);
    EXPECT_EQ(e.size(), 0);
}

TEST(BufferTest, Pack_ReturnsBytesWritten) {
    etools::memory::buffer b{std::make_unique<std::byte[]>(16), 16};
    std::size_t n = b.pack(int{7}, char{'A'});
    EXPECT_EQ(n, sizeof(int) + sizeof(char));
    EXPECT_EQ(n, b.size());
}

TEST(BufferTest, Pack_TooSmall_ReturnsZeroAndWritesNothing) {
    // Capacity 2 cannot hold an int (4 bytes); the serializer writes nothing and
    // returns 0. (Release/NDEBUG behavior — this TU is compiled with NDEBUG.)
    etools::memory::buffer b{std::make_unique<std::byte[]>(2), 2};
    std::size_t n = b.pack(int{123456});
    EXPECT_EQ(n, 0u);
    EXPECT_EQ(b.size(), 0u);
}

TEST(BufferTest, MoveConstructor_Positive) {
    const size_t capacity = 8;
    auto data = std::make_unique<std::byte[]>(capacity);
    auto raw_ptr = data.get();
    etools::memory::buffer e1(std::move(data), capacity);

    etools::memory::buffer e2 = std::move(e1);
    
    EXPECT_EQ(e1.data(), nullptr);
    EXPECT_EQ(e1.size(), 0);
    EXPECT_EQ(e1.capacity(), 0);

    EXPECT_EQ(e2.data(), raw_ptr);
    EXPECT_EQ(e2.size(), 0);
    EXPECT_EQ(e2.capacity(), capacity);
}

TEST(BufferTest, MoveAssignment_Positive) {
    const size_t capacity = 8;
    auto data = std::make_unique<std::byte[]>(capacity);
    auto raw_ptr = data.get();
    etools::memory::buffer e1(std::move(data), capacity);
    etools::memory::buffer e2 = std::move(e1);
    
    EXPECT_EQ(e1.data(), nullptr);
    EXPECT_EQ(e1.size(), 0);
    EXPECT_EQ(e1.capacity(), 0);

    EXPECT_EQ(e2.data(), raw_ptr);
    EXPECT_EQ(e2.size(), 0);
    EXPECT_EQ(e2.capacity(), capacity);
}

 
TEST(BufferTest, PackUnpackStruct_Positive) {
    etools::memory::buffer e{std::make_unique<std::byte[]>(16), 16};
    Message m = {123, 99.9f};

    e.pack(m);

    EXPECT_NE(e.data(), nullptr);
    EXPECT_EQ(e.capacity(), 16);
    EXPECT_EQ(e.size(), sizeof(Message));

    auto unpacked = e.unpack<Message>();
    ASSERT_TRUE(unpacked.has_value());
    auto& [msg] = *unpacked;

    EXPECT_EQ(msg, m);
}

TEST(BufferTest, Repack_Positive) {
    etools::memory::buffer e{std::make_unique<std::byte[]>(16), 16};
    
    // Initial pack with two floats
    e.pack(1.1f, 2.2f);
    EXPECT_EQ(e.size(), sizeof(float) * 2);

    // Repack with different data and types
    e.pack(100);
    EXPECT_EQ(e.size(), sizeof(int));
    
    auto unpacked = e.unpack<int>();
    ASSERT_TRUE(unpacked.has_value());
    auto& [unpacked_int] = *unpacked;
    EXPECT_EQ(unpacked_int, 100);
}

TEST(BufferTest, DataSizeAccessors_Positive) {
    etools::memory::buffer e{std::make_unique<std::byte[]>(16), 16};
    int a = 1, b = 2, c = 3;

    e.pack(a, b, c);

    EXPECT_NE(e.data(), nullptr);
    EXPECT_EQ(e.size(), sizeof(int) * 3);
}


TEST(BufferTest, ConstructWithPrepopulatedSize)
{
    constexpr std::size_t cap = 32;

    // Serialize manually into a buffer
    auto raw = std::make_unique<std::byte[]>(cap);
    std::size_t used = eser::flat::serialize(123, 'X').to(raw.get(), cap);

    // Construct buffer with pre-filled data
    etools::memory::buffer env(std::move(raw), cap, used);

    auto unpacked = env.unpack<int, char>();
    ASSERT_TRUE(unpacked.has_value());
    auto& [i, c] = *unpacked;
    EXPECT_EQ(i, 123);
    EXPECT_EQ(c, 'X');
    EXPECT_EQ(env.size(), used);
    EXPECT_EQ(env.capacity(), cap);
}

TEST(BufferTest, ConstructWithCustomNoopDeleter)
{
    constexpr std::size_t cap = 64;
    std::byte static_mem[cap];

    using NoDelete = void(*)(std::byte*);
    NoDelete noop = [](std::byte*) {}; // Do nothing

    // Wrap static buffer with no-op deleter
    std::unique_ptr<std::byte[], NoDelete> ptr(static_mem, noop);
    etools::memory::buffer<NoDelete> env(std::move(ptr), cap);

    env.pack(3.14, 'Z');
    auto unpacked = env.unpack<double, char>();
    ASSERT_TRUE(unpacked.has_value());
    auto& [d, ch] = *unpacked;
    EXPECT_DOUBLE_EQ(d, 3.14);
    EXPECT_EQ(ch, 'Z');
    EXPECT_LE(env.size(), env.capacity());
}

// --- Custom-deleter invocation -------------------------------------------
//
// The headline feature of buffer is that the Deleter template parameter
// lets users wire in pool/static/heap memory uniformly. Up to now nothing
// in the suite verified that the deleter *actually runs* at destruction.

namespace {
    struct counting_deleter {
        // shared so the lambda captured by std::function-like deleter can
        // tick a counter we can read from the test body.
        static inline int invocations = 0;
        static inline std::byte* last_pointer = nullptr;
        void operator()(std::byte* p) const noexcept {
            ++invocations;
            last_pointer = p;
            delete[] p;
        }
        static void reset() noexcept { invocations = 0; last_pointer = nullptr; }
    };
}

TEST(BufferTest, Deleter_InvokedOnDestruction) {
    counting_deleter::reset();
    constexpr std::size_t cap = 16;
    std::byte* raw = new std::byte[cap];
    {
        std::unique_ptr<std::byte[], counting_deleter> ptr(raw, counting_deleter{});
        etools::memory::buffer<counting_deleter> env(std::move(ptr), cap);
        EXPECT_EQ(counting_deleter::invocations, 0) << "deleter must not run while buffer is alive";
    }
    EXPECT_EQ(counting_deleter::invocations, 1) << "deleter must run exactly once at destruction";
    EXPECT_EQ(counting_deleter::last_pointer, raw) << "deleter must receive the buffer pointer";
}

TEST(BufferTest, Deleter_NotInvokedAfterMoveFromObjectDies) {
    counting_deleter::reset();
    constexpr std::size_t cap = 16;
    auto ptr = std::unique_ptr<std::byte[], counting_deleter>(new std::byte[cap], counting_deleter{});
    {
        etools::memory::buffer<counting_deleter> src(std::move(ptr), cap);
        {
            etools::memory::buffer<counting_deleter> dst = std::move(src);
            EXPECT_EQ(counting_deleter::invocations, 0);
        }
        // dst went out of scope and ran the deleter; src is now moved-from
        // and must NOT run it again when it dies.
        EXPECT_EQ(counting_deleter::invocations, 1);
    }
    EXPECT_EQ(counting_deleter::invocations, 1) << "moved-from buffer must not double-free";
}

// --- Move semantics: moved-from invariant --------------------------------

TEST(BufferTest, MoveAssign_LeavesSourceInDocumentedEmptyState) {
    constexpr std::size_t cap = 32;
    etools::memory::buffer src(std::make_unique<std::byte[]>(cap), cap);
    src.pack(42, 'A');
    const auto src_size_before = src.size();

    etools::memory::buffer dst(std::make_unique<std::byte[]>(8), 8);
    dst = std::move(src);

    EXPECT_EQ(src.data(),     nullptr);
    EXPECT_EQ(src.size(),     0u);
    EXPECT_EQ(src.capacity(), 0u);

    EXPECT_NE(dst.data(),     nullptr);
    EXPECT_EQ(dst.size(),     src_size_before);
    EXPECT_EQ(dst.capacity(), cap);
}

TEST(BufferTest, MoveAssign_SelfMoveIsNoop) {
    constexpr std::size_t cap = 16;
    etools::memory::buffer env(std::make_unique<std::byte[]>(cap), cap);
    env.pack(int{123});
    const auto* data_before = env.data();
    const auto  size_before = env.size();
    const auto  cap_before  = env.capacity();

    // Self-move via reference indirection to defeat -Wself-move.
    auto& alias = env;
    env = std::move(alias);

    EXPECT_EQ(env.data(),     data_before);
    EXPECT_EQ(env.size(),     size_before);
    EXPECT_EQ(env.capacity(), cap_before);

    auto unpacked = env.unpack<int>();
    ASSERT_TRUE(unpacked.has_value());
    auto& [i] = *unpacked;
    EXPECT_EQ(i, 123) << "self-move must not corrupt the payload";
}

// --- Constructor with pre-populated size: precondition holds -------------

TEST(BufferTest, ConstructWithPrepopulatedSize_AtCapacityIsLegal) {
    // The (data, capacity, size) ctor's precondition is `size <= capacity`.
    // size == capacity is the boundary case — it must not assert.
    constexpr std::size_t cap = 8;
    auto buf = std::make_unique<std::byte[]>(cap);
    etools::memory::buffer env(std::move(buf), cap, cap);
    EXPECT_EQ(env.size(),     cap);
    EXPECT_EQ(env.capacity(), cap);
}

TEST(BufferTest, ConstructWithPrepopulatedSize_ZeroSizeIsLegal) {
    // size == 0 with capacity > 0 is the other boundary — equivalent to
    // the two-arg constructor and must not assert.
    constexpr std::size_t cap = 8;
    auto buf = std::make_unique<std::byte[]>(cap);
    etools::memory::buffer env(std::move(buf), cap, 0);
    EXPECT_EQ(env.size(),     0u);
    EXPECT_EQ(env.capacity(), cap);
}

// --- Pack + unpack round-trip for heterogeneous types --------------------

TEST(BufferTest, PackUnpack_HeterogeneousTypes_PreservesValues) {
    constexpr std::size_t cap = 64;
    etools::memory::buffer env(std::make_unique<std::byte[]>(cap), cap);

    env.pack(int{-7}, double{2.71828}, char{'Q'});
    EXPECT_LE(env.size(), env.capacity());

    auto unpacked = env.unpack<int, double, char>();
    ASSERT_TRUE(unpacked.has_value());
    auto& [i, d, c] = *unpacked;
    EXPECT_EQ(i, -7);
    EXPECT_DOUBLE_EQ(d, 2.71828);
    EXPECT_EQ(c, 'Q');
}