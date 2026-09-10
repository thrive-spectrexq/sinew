#include "test_harness.hpp"
#include <sinew/types.hpp>

TEST_CASE(TestHeaderLayout) {
    REQUIRE_EQ(sizeof(sinew::Header), 16);
    REQUIRE_EQ(sinew::SINEW_MAGIC, 0x574E4953u);

    sinew::Header hdr{};
    hdr.magic = sinew::SINEW_MAGIC;
    hdr.msg_id = 42;
    hdr.version = 1;
    hdr.crc = 0x12345678;
    hdr.payload_len = 64;

    const auto* bytes = reinterpret_cast<const uint8_t*>(&hdr);
    // Magic: 'S', 'I', 'N', 'W' in Little Endian
    REQUIRE_EQ(bytes[0], 'S');
    REQUIRE_EQ(bytes[1], 'I');
    REQUIRE_EQ(bytes[2], 'N');
    REQUIRE_EQ(bytes[3], 'W');
}

TEST_CASE(TestErrorStrings) {
    REQUIRE_NE(sinew::error_string(sinew::ErrorCode::Ok).length(), 0);
    REQUIRE_NE(sinew::error_string(sinew::ErrorCode::BufferTooSmall).length(), 0);
    REQUIRE_NE(sinew::error_string(sinew::ErrorCode::BadMagic).length(), 0);
    REQUIRE_NE(sinew::error_string(sinew::ErrorCode::IdMismatch).length(), 0);
    REQUIRE_NE(sinew::error_string(sinew::ErrorCode::VersionMismatch).length(), 0);
    REQUIRE_NE(sinew::error_string(sinew::ErrorCode::ChecksumMismatch).length(), 0);
    REQUIRE_NE(sinew::error_string(sinew::ErrorCode::Misaligned).length(), 0);
}
