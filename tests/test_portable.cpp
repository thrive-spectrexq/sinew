#include "test_harness.hpp"
#include <sinew/sinew.hpp>
#include <cmath>

struct GpsWaypoint {
    uint64_t timestamp_ns;
    double latitude;
    double longitude;
    float altitude_m;
    uint32_t satellites;

    SINEW_REFLECT(timestamp_ns, latitude, longitude, altitude_m, satellites)
};
SINEW_REGISTER_MESSAGE(GpsWaypoint, 201, 1);

static_assert(sinew::is_valid_message_v<GpsWaypoint>, "GpsWaypoint must be valid message");

struct ComplexTelemetry {
    uint32_t seq;
    sinew::StaticString<16> device_id;
    sinew::StaticVector<float, 4> voltages;

    SINEW_REFLECT(seq, device_id, voltages)
};
SINEW_REGISTER_MESSAGE(ComplexTelemetry, 202, 1);

TEST_CASE(TestByteSwapScalars) {
    uint16_t v16 = 0x1234;
    REQUIRE_EQ(sinew::byte_swap(v16), 0x3412);

    uint32_t v32 = 0x12345678;
    REQUIRE_EQ(sinew::byte_swap(v32), 0x78563412u);

    uint64_t v64 = 0x0102030405060708ull;
    REQUIRE_EQ(sinew::byte_swap(v64), 0x0807060504030201ull);
}

TEST_CASE(TestPortableSwapReflectionInversion) {
    GpsWaypoint orig{
        123456789012345ULL,
        37.774929,
        -122.419416,
        15.5f,
        14
    };

    GpsWaypoint swapped = orig;
    sinew::portable_swap(swapped);

    // After swapping, multi-byte fields should NOT match original
    REQUIRE_NE(swapped.timestamp_ns, orig.timestamp_ns);
    REQUIRE_NE(swapped.satellites, orig.satellites);

    // Swapping a second time must restore exact original values
    sinew::portable_swap(swapped);
    REQUIRE_EQ(swapped.timestamp_ns, orig.timestamp_ns);
    REQUIRE_EQ(swapped.latitude, orig.latitude);
    REQUIRE_EQ(swapped.longitude, orig.longitude);
    REQUIRE_EQ(swapped.altitude_m, orig.altitude_m);
    REQUIRE_EQ(swapped.satellites, orig.satellites);
}

TEST_CASE(TestPortableSwapContainersInversion) {
    ComplexTelemetry orig{};
    orig.seq = 42;
    orig.device_id = "sensor_alpha";
    orig.voltages.push_back(3.3f);
    orig.voltages.push_back(5.0f);
    orig.voltages.push_back(12.0f);

    ComplexTelemetry swapped = orig;
    sinew::portable_swap(swapped);

    REQUIRE_NE(swapped.seq, orig.seq);

    // Swap back
    sinew::portable_swap(swapped);
    REQUIRE_EQ(swapped.seq, orig.seq);
    REQUIRE_EQ(swapped.device_id.view(), "sensor_alpha");
    REQUIRE_EQ(swapped.voltages.size(), 3u);
    REQUIRE_EQ(swapped.voltages[0], 3.3f);
    REQUIRE_EQ(swapped.voltages[1], 5.0f);
    REQUIRE_EQ(swapped.voltages[2], 12.0f);
}

TEST_CASE(TestPortableEncodeDecodeRoundTrip) {
    GpsWaypoint out_waypoint{
        987654321000ULL,
        40.712776,
        -74.005974,
        10.25f,
        18
    };

    alignas(16) uint8_t wire_buf[sinew::message_size<GpsWaypoint>()];
    size_t encoded_bytes = sinew::portable_encode(wire_buf, sizeof(wire_buf), out_waypoint, true);
    REQUIRE_EQ(encoded_bytes, sizeof(sinew::Header) + sizeof(GpsWaypoint));

    GpsWaypoint in_waypoint{};
    sinew::ErrorCode err = sinew::portable_decode(wire_buf, encoded_bytes, in_waypoint, true);
    REQUIRE_EQ(err, sinew::ErrorCode::Ok);

    REQUIRE_EQ(in_waypoint.timestamp_ns, 987654321000ULL);
    REQUIRE_EQ(in_waypoint.latitude, 40.712776);
    REQUIRE_EQ(in_waypoint.longitude, -74.005974);
    REQUIRE_EQ(in_waypoint.altitude_m, 10.25f);
    REQUIRE_EQ(in_waypoint.satellites, 18u);

    // Corrupt the CRC and verify detection
    auto* hdr = reinterpret_cast<sinew::Header*>(wire_buf);
    hdr->crc ^= 0xFF;
    sinew::ErrorCode err_corrupt = sinew::portable_decode(wire_buf, encoded_bytes, in_waypoint, true);
    REQUIRE_EQ(err_corrupt, sinew::ErrorCode::ChecksumMismatch);
}

TEST_CASE(TestStaticVectorPortableSwap) {
    sinew::StaticVector<uint32_t, 8> vec;
    vec.push_back(0x12345678);
    vec.push_back(0xAABBCCDD);

    REQUIRE_EQ(vec.size(), 2u);

    // Call portable_swap directly
    sinew::portable_swap(vec);

    // Size should be byte swapped
    REQUIRE_EQ(vec.size(), sinew::byte_swap(2u));
    // Populated elements should be byte swapped
    REQUIRE_EQ(vec.data_[0], 0x78563412u);
    REQUIRE_EQ(vec.data_[1], 0xDDCCBBAAu);

    // Swap back
    sinew::portable_swap(vec);
    REQUIRE_EQ(vec.size(), 2u);
    REQUIRE_EQ(vec[0], 0x12345678u);
    REQUIRE_EQ(vec[1], 0xAABBCCDDu);
}

