#pragma once

#include <sinew/types.hpp>
#include <sinew/crc32.hpp>
#include <sinew/endian.hpp>
#include <sinew/message.hpp>
#include <sinew/containers.hpp>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>
#include <utility>

/**
 * @brief Embed inside any message struct to enable portable endianness-normalization and field reflection.
 *
 * Uses C++17 fold expression expansion to visit every listed field with zero overhead and no macro limits.
 */
#define SINEW_REFLECT(...) \
    template <typename SinewVisitorFunc> \
    void sinew_reflect(SinewVisitorFunc&& _sinew_v) { \
        auto _sinew_helper = [&](auto&&... args) { (_sinew_v(args), ...); }; \
        _sinew_helper(__VA_ARGS__); \
    } \
    template <typename SinewVisitorFunc> \
    void sinew_reflect(SinewVisitorFunc&& _sinew_v) const { \
        auto _sinew_helper = [&](auto&&... args) { (_sinew_v(args), ...); }; \
        _sinew_helper(__VA_ARGS__); \
    }

namespace sinew {

/**
 * @brief Unconditional byte swap for scalar values of 2, 4, or 8 bytes.
 */
template <typename T>
inline T byte_swap(T value) noexcept {
    static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
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
    return value;
}

namespace detail {

struct DummyVisitor {
    template <typename Field>
    void operator()(Field&) const noexcept {}
};

template <typename T, typename = void>
struct has_sinew_reflect : std::false_type {};

template <typename T>
struct has_sinew_reflect<T, std::void_t<decltype(std::declval<T&>().sinew_reflect(std::declval<DummyVisitor&>()))>> : std::true_type {};

template <typename T>
inline constexpr bool has_sinew_reflect_v = has_sinew_reflect<T>::value;

template <typename T>
struct is_static_vector : std::false_type {};

template <typename T, size_t N>
struct is_static_vector<StaticVector<T, N>> : std::true_type {};

template <typename T>
inline constexpr bool is_static_vector_v = is_static_vector<T>::value;

template <typename T>
struct is_static_string : std::false_type {};

template <size_t N>
struct is_static_string<StaticString<N>> : std::true_type {};

template <typename T>
inline constexpr bool is_static_string_v = is_static_string<T>::value;

} // namespace detail

/**
 * @brief In-place endianness byte-swap for any field, container, or reflected message.
 */
template <typename T>
inline void portable_swap(T& val) noexcept {
    if constexpr (detail::has_sinew_reflect_v<T>) {
        val.sinew_reflect([](auto& field) {
            portable_swap(field);
        });
    } else if constexpr (detail::is_static_vector_v<T>) {
        val.size_ = byte_swap(val.size_);
        const uint32_t count = (val.size_ <= T::capacity()) ? val.size_ : T::capacity();
        for (uint32_t i = 0; i < count; ++i) {
            portable_swap(val.data_[i]);
        }
    } else if constexpr (detail::is_static_string_v<T>) {
        val.size_ = byte_swap(val.size_);
    } else if constexpr (std::is_fundamental_v<T> || std::is_enum_v<T>) {
        if constexpr (sizeof(T) > 1) {
            val = byte_swap(val);
        }
    }
}

/**
 * @brief Encode a message struct into a portable, architecture-neutral wire buffer.
 *
 * Automatically normalizes byte order to Little Endian across endian boundaries.
 */
template <typename T>
inline size_t portable_encode(void* dest, size_t dest_capacity, const T& source, bool compute_crc = true) noexcept {
    check_message_type_constraints<T>();

    if (dest == nullptr || dest_capacity < sizeof(Header) + sizeof(T)) {
        return 0;
    }

    auto* hdr = reinterpret_cast<Header*>(dest);
    hdr->magic = to_little_endian(SINEW_MAGIC);
    hdr->msg_id = to_little_endian(MessageTraits<T>::id);
    hdr->version = to_little_endian(MessageTraits<T>::version);
    hdr->payload_len = to_little_endian(static_cast<uint32_t>(sizeof(T)));

    uint8_t* payload_dest = static_cast<uint8_t*>(dest) + sizeof(Header);

    // Copy source payload
    std::memcpy(payload_dest, &source, sizeof(T));

    // If host is big-endian, swap payload fields to little-endian
    if constexpr (native_endian() == Endian::Big) {
        auto* typed_payload = reinterpret_cast<T*>(payload_dest);
        portable_swap(*typed_payload);
    }

    if (compute_crc) {
        uint32_t crc = crc32(payload_dest, sizeof(T));
        hdr->crc = to_little_endian(crc);
    } else {
        hdr->crc = 0;
    }

    return sizeof(Header) + sizeof(T);
}

/**
 * @brief Decode a portable wire buffer into a message struct.
 *
 * Validates header, checksum, and normalizes byte order into native host format.
 */
template <typename T>
inline ErrorCode portable_decode(const void* src, size_t src_len, T& dest, bool verify_crc = true) noexcept {
    check_message_type_constraints<T>();

    if (src == nullptr || src_len < sizeof(Header) + sizeof(T)) {
        return ErrorCode::BufferTooSmall;
    }

    const auto* hdr = reinterpret_cast<const Header*>(src);
    uint32_t magic = from_little_endian(hdr->magic);
    if (magic != SINEW_MAGIC) {
        return ErrorCode::BadMagic;
    }

    uint16_t msg_id = from_little_endian(hdr->msg_id);
    constexpr uint16_t expected_id = MessageTraits<T>::id;
    if (msg_id != expected_id) {
        return ErrorCode::IdMismatch;
    }

    uint16_t ver = from_little_endian(hdr->version);
    constexpr uint16_t expected_ver = MessageTraits<T>::version;
    if (ver != expected_ver) {
        return ErrorCode::VersionMismatch;
    }

    uint32_t plen = from_little_endian(hdr->payload_len);
    if (plen != sizeof(T)) {
        return ErrorCode::BufferTooSmall;
    }

    const uint8_t* payload_src = static_cast<const uint8_t*>(src) + sizeof(Header);

    if (verify_crc && hdr->crc != 0) {
        uint32_t computed = crc32(payload_src, sizeof(T));
        uint32_t expected_crc = from_little_endian(hdr->crc);
        if (computed != expected_crc) {
            return ErrorCode::ChecksumMismatch;
        }
    }

    // Copy to destination struct
    std::memcpy(&dest, payload_src, sizeof(T));

    // If host is big-endian, swap payload fields to native endian
    if constexpr (native_endian() == Endian::Big) {
        portable_swap(dest);
    }

    return ErrorCode::Ok;
}

} // namespace sinew
