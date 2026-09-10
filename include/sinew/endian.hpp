#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>

#if defined(_MSC_VER)
#include <stdlib.h>
#endif

namespace sinew {

enum class Endian {
    Little = 0,
    Big = 1
};

constexpr Endian native_endian() noexcept {
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return Endian::Big;
#else
    return Endian::Little;
#endif
}

inline uint16_t bswap16(uint16_t val) noexcept {
#if defined(_MSC_VER)
    return _byteswap_ushort(val);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap16(val);
#else
    return static_cast<uint16_t>((val << 8) | (val >> 8));
#endif
}

inline uint32_t bswap32(uint32_t val) noexcept {
#if defined(_MSC_VER)
    return _byteswap_ulong(val);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(val);
#else
    return ((val & 0xFF000000u) >> 24) |
           ((val & 0x00FF0000u) >> 8)  |
           ((val & 0x0000FF00u) << 8)  |
           ((val & 0x000000FFu) << 24);
#endif
}

inline uint64_t bswap64(uint64_t val) noexcept {
#if defined(_MSC_VER)
    return _byteswap_uint64(val);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap64(val);
#else
    return ((val & 0xFF00000000000000ull) >> 56) |
           ((val & 0x00FF000000000000ull) >> 40) |
           ((val & 0x0000FF0000000000ull) >> 24) |
           ((val & 0x000000FF00000000ull) >> 8)  |
           ((val & 0x00000000FF000000ull) << 8)  |
           ((val & 0x0000000000FF0000ull) << 24) |
           ((val & 0x000000000000FF00ull) << 40) |
           ((val & 0x00000000000000FFull) << 56);
#endif
}

template <typename T>
inline T to_little_endian(T value) noexcept {
    static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
    if constexpr (native_endian() == Endian::Big) {
        if constexpr (sizeof(T) == 2) {
            uint16_t v;
            std::memcpy(&v, &value, 2);
            v = bswap16(v);
            std::memcpy(&value, &v, 2);
        } else if constexpr (sizeof(T) == 4) {
            uint32_t v;
            std::memcpy(&v, &value, 4);
            v = bswap32(v);
            std::memcpy(&value, &v, 4);
        } else if constexpr (sizeof(T) == 8) {
            uint64_t v;
            std::memcpy(&v, &value, 8);
            v = bswap64(v);
            std::memcpy(&value, &v, 8);
        }
    }
    return value;
}

template <typename T>
inline T from_little_endian(T value) noexcept {
    return to_little_endian(value);
}

} // namespace sinew
