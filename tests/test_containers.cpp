#include "test_harness.hpp"
#include <sinew/sinew.hpp>
#include <type_traits>

struct LidarScan {
    uint64_t timestamp_ns;
    sinew::StaticString<32> sensor_frame;
    sinew::StaticVector<float, 8> ranges;
};
SINEW_REGISTER_MESSAGE(LidarScan, 100, 1);

static_assert(sinew::is_valid_message_v<LidarScan>, "LidarScan must be standard layout and trivially copyable");

TEST_CASE(TestStaticVectorBasic) {
    sinew::StaticVector<int, 4> vec;
    REQUIRE_EQ(vec.size(), 0u);
    REQUIRE(vec.empty());
    REQUIRE(!vec.full());

    REQUIRE(vec.push_back(10));
    REQUIRE(vec.push_back(20));
    REQUIRE(vec.emplace_back(30));
    REQUIRE(vec.push_back(40));

    REQUIRE_EQ(vec.size(), 4u);
    REQUIRE(vec.full());
    REQUIRE(!vec.push_back(50)); // Exceeds capacity

    REQUIRE_EQ(vec[0], 10);
    REQUIRE_EQ(vec[1], 20);
    REQUIRE_EQ(vec[2], 30);
    REQUIRE_EQ(vec[3], 40);
    REQUIRE_EQ(vec.front(), 10);
    REQUIRE_EQ(vec.back(), 40);

    REQUIRE(vec.pop_back());
    REQUIRE_EQ(vec.size(), 3u);
    REQUIRE_EQ(vec.back(), 30);

    int sum = 0;
    for (int val : vec) {
        sum += val;
    }
    REQUIRE_EQ(sum, 60);

    vec.clear();
    REQUIRE(vec.empty());
    REQUIRE_EQ(vec.size(), 0u);
    REQUIRE(!vec.pop_back());
}

TEST_CASE(TestStaticStringBasic) {
    sinew::StaticString<16> str;
    REQUIRE_EQ(str.size(), 0u);
    REQUIRE(str.empty());

    str = "hello";
    REQUIRE_EQ(str.size(), 5u);
    REQUIRE_EQ(str.view(), "hello");
    REQUIRE_EQ(std::string_view(str.c_str()), "hello");

    REQUIRE(str.append(" world"));
    REQUIRE_EQ(str.view(), "hello world");
    REQUIRE_EQ(str.size(), 11u);

    // Test truncation on overflow
    sinew::StaticString<5> small_str("1234567890");
    REQUIRE_EQ(small_str.size(), 5u);
    REQUIRE_EQ(small_str.view(), "12345");
    REQUIRE_EQ(small_str.c_str()[5], '\0');
}

TEST_CASE(TestBoundedContainersWireRoundTrip) {
    alignas(16) uint8_t buffer[sinew::message_size<LidarScan>()];

    LidarScan* out = sinew::prepare<LidarScan>(buffer);
    REQUIRE(out != nullptr);

    out->timestamp_ns = 9988776655ULL;
    out->sensor_frame = "lidar_front_link";
    out->ranges.push_back(1.5f);
    out->ranges.push_back(2.8f);
    out->ranges.push_back(3.14f);

    size_t wire_size = sinew::finalize(out, /*compute_crc=*/true);
    REQUIRE_EQ(wire_size, sizeof(sinew::Header) + sizeof(LidarScan));

    const LidarScan* in = sinew::view<LidarScan>(buffer, wire_size, /*verify_crc=*/true);
    REQUIRE(in != nullptr);
    REQUIRE_EQ(in->timestamp_ns, 9988776655ULL);
    REQUIRE_EQ(in->sensor_frame.view(), "lidar_front_link");
    REQUIRE_EQ(in->ranges.size(), 3u);
    REQUIRE_EQ(in->ranges[0], 1.5f);
    REQUIRE_EQ(in->ranges[1], 2.8f);
    REQUIRE_EQ(in->ranges[2], 3.14f);
}
