#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <ostream>

namespace sinew {

// 4-byte ASCII magic identifier: 'SNEW' (0x574E4953 in Little Endian)
inline constexpr uint32_t SINEW_MAGIC = 0x574E4953u;

#pragma pack(push, 1)
struct Header {
    uint32_t magic;       // Must equal SINEW_MAGIC (0x574E4953)
    uint16_t msg_id;      // Unique message type identifier
    uint16_t version;     // Schema layout version
    uint32_t crc;         // Optional CRC-32 (IEEE 802.3) of payload
    uint32_t payload_len; // Size of payload in bytes
};
#pragma pack(pop)

static_assert(sizeof(Header) == 16, "Sinew Header must be exactly 16 bytes");
static_assert(alignof(Header) == 1, "Sinew Header must have 1-byte alignment for wire safety");

enum class ErrorCode : uint8_t {
    Ok = 0,
    BufferTooSmall,
    BadMagic,
    IdMismatch,
    VersionMismatch,
    ChecksumMismatch,
    Misaligned
};

constexpr std::string_view error_string(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::Ok: return "Ok";
        case ErrorCode::BufferTooSmall: return "Buffer too small for header and payload";
        case ErrorCode::BadMagic: return "Invalid Sinew magic identifier";
        case ErrorCode::IdMismatch: return "Message ID mismatch";
        case ErrorCode::VersionMismatch: return "Message schema version mismatch";
        case ErrorCode::ChecksumMismatch: return "CRC32 checksum mismatch";
        case ErrorCode::Misaligned: return "Buffer address is not properly aligned for payload type";
        default: return "Unknown error";
    }
}

inline std::ostream& operator<<(std::ostream& os, ErrorCode code) {
    return os << error_string(code);
}

} // namespace sinew
