#include "test_harness.hpp"
#include <sinew/sinew.hpp>
#include <cstring>
#include <vector>

struct TestTelemetry {
    uint64_t timestamp_ns;
    float temperature;
    float pressure;
    int32_t status_flags;
};
SINEW_REGISTER_MESSAGE(TestTelemetry, 201, 2);

TEST_CASE(TestWireRoundTrip) {
    uint8_t wire_buffer[sinew::message_size<TestTelemetry>()];

    // Prepare in-place
    TestTelemetry* out = sinew::prepare<TestTelemetry>(wire_buffer);
    REQUIRE(out != nullptr);
    out->timestamp_ns = 1700000000123456789ULL;
    out->temperature = 36.6f;
    out->pressure = 1013.25f;
    out->status_flags = 0x07;

    // Finalize stamps CRC and header
    size_t total_len = sinew::finalize(out, /*compute_crc=*/true);
    REQUIRE_EQ(total_len, sizeof(wire_buffer));

    // View in-place with zero-copy
    const TestTelemetry* in = sinew::view<TestTelemetry>(wire_buffer, sizeof(wire_buffer), /*verify_crc=*/true);
    REQUIRE(in != nullptr);

    // Pointer must be identical to wire_buffer + 16 (zero copy)
    REQUIRE_EQ(reinterpret_cast<const uint8_t*>(in), wire_buffer + sizeof(sinew::Header));

    REQUIRE_EQ(in->timestamp_ns, 1700000000123456789ULL);
    REQUIRE_EQ(in->temperature, 36.6f);
    REQUIRE_EQ(in->pressure, 1013.25f);
    REQUIRE_EQ(in->status_flags, 0x07);

    // Header inspection
    const auto& hdr = sinew::get_header(in);
    REQUIRE_EQ(hdr.magic, sinew::SINEW_MAGIC);
    REQUIRE_EQ(hdr.msg_id, 201);
    REQUIRE_EQ(hdr.version, 2);
    REQUIRE_NE(hdr.crc, 0u);
    REQUIRE_EQ(hdr.payload_len, sizeof(TestTelemetry));
}

TEST_CASE(TestValidationErrors) {
    uint8_t wire_buffer[sinew::message_size<TestTelemetry>()];
    TestTelemetry* out = sinew::prepare<TestTelemetry>(wire_buffer);
    REQUIRE(out != nullptr);
    out->timestamp_ns = 1000;
    out->temperature = 25.0f;
    out->pressure = 1000.0f;
    out->status_flags = 1;
    sinew::finalize(out, true);

    // 1. Buffer too small
    REQUIRE_EQ(sinew::validate<TestTelemetry>(wire_buffer, sizeof(sinew::Header)), 
               sinew::ErrorCode::BufferTooSmall);
    REQUIRE(sinew::view<TestTelemetry>(wire_buffer, sizeof(sinew::Header)) == nullptr);

    // 2. Bad Magic
    auto* hdr = reinterpret_cast<sinew::Header*>(wire_buffer);
    uint32_t real_magic = hdr->magic;
    hdr->magic = 0xDEADBEEF;
    REQUIRE_EQ(sinew::validate<TestTelemetry>(wire_buffer, sizeof(wire_buffer)), 
               sinew::ErrorCode::BadMagic);
    hdr->magic = real_magic;

    // 3. Bad Message ID
    uint16_t real_id = hdr->msg_id;
    hdr->msg_id = 999;
    REQUIRE_EQ(sinew::validate<TestTelemetry>(wire_buffer, sizeof(wire_buffer)), 
               sinew::ErrorCode::IdMismatch);
    hdr->msg_id = real_id;

    // 4. Bad Version
    uint16_t real_ver = hdr->version;
    hdr->version = 99;
    REQUIRE_EQ(sinew::validate<TestTelemetry>(wire_buffer, sizeof(wire_buffer)), 
               sinew::ErrorCode::VersionMismatch);
    hdr->version = real_ver;

    // 5. CRC Checksum mismatch on altered payload
    out->temperature = 999.0f; // Mutate payload without re-finalizing
    REQUIRE_EQ(sinew::validate<TestTelemetry>(wire_buffer, sizeof(wire_buffer), /*verify_crc=*/true), 
               sinew::ErrorCode::ChecksumMismatch);
}
