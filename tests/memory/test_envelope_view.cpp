#include <gtest/gtest.h>
#include <eser/binary/serializer.hpp>
#include <etools/memory/envelope_view.hpp>
#include <type_traits>
struct Message {
    int id;
    float value;
    bool operator==(const Message& other) const {
        return id == other.id && value == other.value;
    }
};
static_assert(std::is_trivially_copyable_v<Message>);


// --- Google Test suite for envelope_view ---

TEST(EnvelopeViewTest, Constructor_Positive) {
    // Manually create a buffer and serialize data into it.
    std::byte buffer[100]{};
    auto serialized_data = eser::binary::serialize(42, 3.14f);
    size_t data_size = serialized_data.to(buffer);

    // Create a view from the data.
    etools::memory::envelope_view view(buffer, data_size);

    // Verify the view's data and capacity are correctly set.
    EXPECT_EQ(view.data(), buffer);
    EXPECT_EQ(view.capacity(), data_size);
}

TEST(EnvelopeViewTest, UnpackScalar_Positive) {
    // Manually create a buffer and serialize data into it.
    std::byte buffer[100]{};
    int a = 100;
    float b = 50.5f;
    auto serialized_data = eser::binary::serialize(a, b);
    size_t data_size = serialized_data.to(buffer);

    // Create a view from the serialized data.
    etools::memory::envelope_view view(buffer, data_size);

    // Unpack the data from the view.
    auto [unpacked_a, unpacked_b] = view.unpack<int, float>();

    // Verify the unpacked values are correct.
    EXPECT_EQ(unpacked_a, a);
    EXPECT_FLOAT_EQ(unpacked_b, b);
}

TEST(EnvelopeViewTest, UnpackStruct_Positive) {
    // Manually create a buffer and serialize data into it.
    std::byte buffer[100]{};
    Message m = {123, 99.9f};
    auto serialized_data = eser::binary::serialize(m);
    size_t data_size = serialized_data.to(buffer);

    // Create a view from the serialized data.
    etools::memory::envelope_view view(buffer, data_size);
    
    // Unpack the data from the view.
    auto unpacked_tuple = view.unpack<Message>();
    Message unpacked_m = std::get<0>(unpacked_tuple);

    // Verify the struct's contents.
    EXPECT_EQ(unpacked_m, m);
}

TEST(EnvelopeViewTest, CopySemantics_Positive) {
    std::byte buffer[100]{};
    auto serialized_data = eser::binary::serialize(1, 2, 3);
    size_t data_size = serialized_data.to(buffer);
    
    etools::memory::envelope_view view1(buffer, data_size);
    const std::byte* original_data_ptr = view1.data();

    // Test copy constructor (trivial shallow copy).
    etools::memory::envelope_view view2 = view1;
    EXPECT_EQ(view2.data(), original_data_ptr);
    EXPECT_EQ(view2.capacity(), data_size);

    // Test copy assignment.
    etools::memory::envelope_view view3(nullptr, 0);
    view3 = view1;
    EXPECT_EQ(view3.data(), original_data_ptr);
    EXPECT_EQ(view3.capacity(), data_size);
}

TEST(EnvelopeViewTest, MoveSemantics_Positive) {
    std::byte buffer[100]{};
    auto serialized_data = eser::binary::serialize(1.0, 2.0, 3.0);
    size_t data_size = serialized_data.to(buffer);
    
    etools::memory::envelope_view view1(buffer, data_size);
    const std::byte* original_data_ptr = view1.data();
    
    // Test move constructor (should be a shallow copy, not a move).
    etools::memory::envelope_view view2 = std::move(view1);
    EXPECT_EQ(view2.data(), original_data_ptr);
    EXPECT_EQ(view2.capacity(), data_size);
    
    // Test move assignment.
    etools::memory::envelope_view view3(nullptr, 0);
    view3 = std::move(view2);
    EXPECT_EQ(view3.data(), original_data_ptr);
    EXPECT_EQ(view3.capacity(), data_size);
}