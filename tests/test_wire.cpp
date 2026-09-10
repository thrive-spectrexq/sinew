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

// Structs defined via convenience macros to verify unique non-zero IDs and type confusion prevention
SINEW_MESSAGE(MacroMsgA,
    uint32_t a;
    uint32_t b;
);

SINEW_MESSAGE(MacroMsgB,
    uint32_t x;
    uint32_t y;
);

TEST_CASE(TestConvenienceMacroUniqueIds) {
    // Both types have the exact same size, but must have distinct, non-zero IDs
    static_assert(sizeof(MacroMsgA) == sizeof(MacroMsgB), "Payload sizes should match for collision check");
    REQUIRE_NE(sinew::MessageTraits<MacroMsgA>::id, 0u);
    REQUIRE_NE(sinew::MessageTraits<MacroMsgB>::id, 0u);
    REQUIRE_NE(sinew::MessageTraits<MacroMsgA>::id, sinew::MessageTraits<MacroMsgB>::id);

    uint8_t buffer[sinew::message_size<MacroMsgA>()];
    MacroMsgA* msg_a = sinew::prepare<MacroMsgA>(buffer);
    msg_a->a = 42;
    msg_a->b = 100;
    sinew::finalize(msg_a, true);

    // Decoding buffer as MacroMsgA must succeed
    REQUIRE(sinew::view<MacroMsgA>(buffer, sizeof(buffer)) != nullptr);

    // Decoding buffer as MacroMsgB must fail with IdMismatch (no type confusion despite matching sizeof)
    auto res_b = sinew::view_result<MacroMsgB>(buffer, sizeof(buffer));
    REQUIRE(!res_b.ok());
    REQUIRE_EQ(res_b.error, sinew::ErrorCode::IdMismatch);
}

struct StructWithPadding {
    uint8_t  flag;       // 1 byte
    // 7 bytes alignment padding
    uint64_t timestamp;  // 8 bytes
    uint8_t  flag2;      // 1 byte
    // 7 bytes alignment padding
    double   value;      // 8 bytes
};
SINEW_REGISTER_MESSAGE(StructWithPadding, 777, 1);

TEST_CASE(TestPaddingZeroingDeterminism) {
    // Fill two wire buffers with completely different dirty patterns
    alignas(16) uint8_t buf1[sinew::message_size<StructWithPadding>()];
    alignas(16) uint8_t buf2[sinew::message_size<StructWithPadding>()];
    std::memset(buf1, 0xAA, sizeof(buf1));
    std::memset(buf2, 0x55, sizeof(buf2));

    // Prepare both payloads (sinew::prepare zeroes padding bytes)
    StructWithPadding* s1 = sinew::prepare<StructWithPadding>(buf1);
    StructWithPadding* s2 = sinew::prepare<StructWithPadding>(buf2);

    s1->flag = 1;
    s1->timestamp = 12345678ULL;
    s1->flag2 = 2;
    s1->value = 3.14159;

    s2->flag = 1;
    s2->timestamp = 12345678ULL;
    s2->flag2 = 2;
    s2->value = 3.14159;

    sinew::finalize(s1, /*compute_crc=*/true);
    sinew::finalize(s2, /*compute_crc=*/true);

    const auto& hdr1 = sinew::get_header(s1);
    const auto& hdr2 = sinew::get_header(s2);

    // CRC must be completely identical despite dirty initial buffers because prepare zeroed the padding
    REQUIRE_EQ(hdr1.crc, hdr2.crc);
    REQUIRE_EQ(std::memcmp(buf1, buf2, sizeof(buf1)), 0);
}

TEST_CASE(TestViewResult) {
    uint8_t wire_buffer[sinew::message_size<TestTelemetry>()];
    TestTelemetry* out = sinew::prepare<TestTelemetry>(wire_buffer);
    out->timestamp_ns = 123;
    out->temperature = 20.0f;
    out->pressure = 1000.0f;
    out->status_flags = 0;
    sinew::finalize(out, true);

    // Successful view_result
    auto res = sinew::view_result<TestTelemetry>(wire_buffer, sizeof(wire_buffer), true);
    REQUIRE(res.ok());
    REQUIRE(res.has_value());
    REQUIRE_EQ(res.error, sinew::ErrorCode::Ok);
    REQUIRE(res.value != nullptr);
    REQUIRE_EQ(res.value->timestamp_ns, 123ULL);

    // Corrupt buffer length
    auto res_short = sinew::view_result<TestTelemetry>(wire_buffer, sizeof(sinew::Header));
    REQUIRE(!res_short.ok());
    REQUIRE_EQ(res_short.error, sinew::ErrorCode::BufferTooSmall);
    REQUIRE(res_short.value == nullptr);
}

TEST_CASE(TestPrepareResult) {
    uint8_t wire_buffer[sinew::message_size<TestTelemetry>()];

    // 1. Successful prepare_result
    auto res = sinew::prepare_result<TestTelemetry>(wire_buffer, sizeof(wire_buffer));
    REQUIRE(res.ok());
    REQUIRE(res.has_value());
    REQUIRE_EQ(res.error, sinew::ErrorCode::Ok);
    REQUIRE(res.value != nullptr);

    // Operator* and operator-> ergonomics
    res->timestamp_ns = 9999;
    REQUIRE_EQ((*res).timestamp_ns, 9999ULL);

    // 2. Buffer too small
    auto res_small = sinew::prepare_result<TestTelemetry>(wire_buffer, sizeof(wire_buffer) - 1);
    REQUIRE(!res_small.ok());
    REQUIRE_EQ(res_small.error, sinew::ErrorCode::BufferTooSmall);
    REQUIRE(res_small.value == nullptr);

    // 3. Null buffer
    auto res_null = sinew::prepare_result<TestTelemetry>(nullptr, sizeof(wire_buffer));
    REQUIRE(!res_null.ok());
    REQUIRE_EQ(res_null.error, sinew::ErrorCode::BufferTooSmall);
    REQUIRE(res_null.value == nullptr);

    // 4. Misaligned buffer
    alignas(16) uint8_t aligned_buf[sinew::message_size<TestTelemetry>() + 16];
    // Offset by 1 byte so that payload (offset by 16 bytes) is misaligned for alignof(TestTelemetry) (which is 8)
    uint8_t* misaligned_ptr = aligned_buf + 1;
    auto res_misalign = sinew::prepare_result<TestTelemetry>(misaligned_ptr, sizeof(aligned_buf) - 1);
    REQUIRE(!res_misalign.ok());
    REQUIRE_EQ(res_misalign.error, sinew::ErrorCode::Misaligned);
    REQUIRE(res_misalign.value == nullptr);
}

