#include <gtest/gtest.h>
#include <etools/memory/envelope.hpp>

struct Message {
    int id;
    float value;
    bool operator==(const Message& other) const {
        return id == other.id && value == other.value;
    }
};
static_assert(std::is_trivially_copyable_v<Message>);
 

TEST(EnvelopeTest, ParameterizedConstructor_Positive) {
    const size_t capacity = 16;
    auto data = std::make_unique<std::byte[]>(capacity);
    etools::memory::envelope e(std::move(data), capacity);
    EXPECT_NE(e.data(), nullptr);
    EXPECT_EQ(e.capacity(), capacity);
    EXPECT_EQ(e.size(), 0);
}

TEST(EnvelopeTest, MoveConstructor_Positive) {
    const size_t capacity = 8;
    auto data = std::make_unique<std::byte[]>(capacity);
    auto raw_ptr = data.get();
    etools::memory::envelope e1(std::move(data), capacity);

    etools::memory::envelope e2 = std::move(e1);
    
    EXPECT_EQ(e1.data(), nullptr);
    EXPECT_EQ(e1.size(), 0);
    EXPECT_EQ(e1.capacity(), 0);

    EXPECT_EQ(e2.data(), raw_ptr);
    EXPECT_EQ(e2.size(), 0);
    EXPECT_EQ(e2.capacity(), capacity);
}

TEST(EnvelopeTest, MoveAssignment_Positive) {
    const size_t capacity = 8;
    auto data = std::make_unique<std::byte[]>(capacity);
    auto raw_ptr = data.get();
    etools::memory::envelope e1(std::move(data), capacity);
    etools::memory::envelope e2 = std::move(e1);
    
    EXPECT_EQ(e1.data(), nullptr);
    EXPECT_EQ(e1.size(), 0);
    EXPECT_EQ(e1.capacity(), 0);

    EXPECT_EQ(e2.data(), raw_ptr);
    EXPECT_EQ(e2.size(), 0);
    EXPECT_EQ(e2.capacity(), capacity);
}

 
TEST(EnvelopeTest, PackUnpackStruct_Positive) {
    etools::memory::envelope e{std::make_unique<std::byte[]>(16), 16};
    Message m = {123, 99.9f};

    e.pack(m);

    EXPECT_NE(e.data(), nullptr);
    EXPECT_EQ(e.capacity(), 16);
    EXPECT_EQ(e.size(), sizeof(Message));

    auto [msg] = e.unpack<Message>();
 
    EXPECT_EQ(msg, m);
}

TEST(EnvelopeTest, Repack_Positive) {
    etools::memory::envelope e{std::make_unique<std::byte[]>(16), 16};
    
    // Initial pack with two floats
    e.pack(1.1f, 2.2f);
    EXPECT_EQ(e.size(), sizeof(float) * 2);

    // Repack with different data and types
    e.pack(100);
    EXPECT_EQ(e.size(), sizeof(int));
    
    auto [unpacked_int] = e.unpack<int>();
    EXPECT_EQ(unpacked_int, 100);
}

TEST(EnvelopeTest, DataSizeAccessors_Positive) {
    etools::memory::envelope e{std::make_unique<std::byte[]>(16), 16};
    int a = 1, b = 2, c = 3;

    e.pack(a, b, c);

    EXPECT_NE(e.data(), nullptr);
    EXPECT_EQ(e.size(), sizeof(int) * 3);
}


TEST(EnvelopeTest, ConstructWithPrepopulatedSize)
{
    constexpr std::size_t cap = 32;

    // Serialize manually into a buffer
    auto raw = std::make_unique<std::byte[]>(cap);
    std::size_t used = eser::binary::serialize(123, 'X').to(raw.get(), cap);

    // Construct envelope with pre-filled data
    etools::memory::envelope env(std::move(raw), cap, used);

    auto [i, c] = env.unpack<int, char>();
    EXPECT_EQ(i, 123);
    EXPECT_EQ(c, 'X');
    EXPECT_EQ(env.size(), used);
    EXPECT_EQ(env.capacity(), cap);
}

TEST(EnvelopeTest, ConstructWithCustomNoopDeleter)
{
    constexpr std::size_t cap = 64;
    std::byte static_mem[cap];

    using NoDelete = void(*)(std::byte*);
    NoDelete noop = [](std::byte*) {}; // Do nothing

    // Wrap static buffer with no-op deleter
    std::unique_ptr<std::byte[], NoDelete> ptr(static_mem, noop);
    etools::memory::envelope<NoDelete> env(std::move(ptr), cap);

    env.pack(3.14, 'Z');
    auto [d, ch] = env.unpack<double, char>();
    EXPECT_DOUBLE_EQ(d, 3.14);
    EXPECT_EQ(ch, 'Z');
    EXPECT_LE(env.size(), env.capacity());
}