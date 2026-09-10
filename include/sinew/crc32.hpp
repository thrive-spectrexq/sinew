#pragma once

#include <cstdint>
#include <cstddef>
#include <array>

namespace sinew {

namespace detail {

constexpr std::array<uint32_t, 256> generate_crc32_table() noexcept {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t crc = i;
        for (uint32_t j = 0; j < 8; ++j) {
            crc = (crc & 1u) ? (0xEDB88320u ^ (crc >> 1)) : (crc >> 1);
        }
        table[i] = crc;
    }
    return table;
}

inline constexpr auto crc32_table = generate_crc32_table();

} // namespace detail

constexpr uint32_t crc32(const void* data, size_t length, uint32_t initial = 0xFFFFFFFFu) noexcept {
    if (data == nullptr || length == 0) {
        return 0;
    }
    const auto* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = initial;
    for (size_t i = 0; i < length; ++i) {
        uint8_t lookup_index = static_cast<uint8_t>(crc ^ bytes[i]);
        crc = (crc >> 8) ^ detail::crc32_table[lookup_index];
    }
    return crc ^ 0xFFFFFFFFu;
}

} // namespace sinew
