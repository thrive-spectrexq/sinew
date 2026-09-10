#pragma once

#include <sinew/types.hpp>
#include <sinew/crc32.hpp>
#include <sinew/message.hpp>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <new>
#include <cstring>

namespace sinew {

template <typename T>
constexpr size_t message_size() noexcept {
    check_message_type_constraints<T>();
    return sizeof(Header) + sizeof(T);
}

template <typename T>
ErrorCode validate(const void* buffer, size_t size, bool verify_crc = false) noexcept {
    check_message_type_constraints<T>();

    if (buffer == nullptr || size < sizeof(Header) + sizeof(T)) {
        return ErrorCode::BufferTooSmall;
    }

    const auto* byte_ptr = static_cast<const uint8_t*>(buffer);
    const auto* payload_ptr = byte_ptr + sizeof(Header);

    if (reinterpret_cast<uintptr_t>(payload_ptr) % alignof(T) != 0) {
        return ErrorCode::Misaligned;
    }

    const auto* hdr = reinterpret_cast<const Header*>(buffer);
    if (hdr->magic != SINEW_MAGIC) {
        return ErrorCode::BadMagic;
    }

    if (hdr->payload_len != sizeof(T)) {
        return ErrorCode::BufferTooSmall;
    }

    constexpr uint16_t expected_id = MessageTraits<T>::id;
    if (hdr->msg_id != expected_id) {
        return ErrorCode::IdMismatch;
    }

    constexpr uint16_t expected_ver = MessageTraits<T>::version;
    if (hdr->version != expected_ver) {
        return ErrorCode::VersionMismatch;
    }

    if (verify_crc && hdr->crc != 0) {
        uint32_t computed = crc32(payload_ptr, sizeof(T));
        if (computed != hdr->crc) {
            return ErrorCode::ChecksumMismatch;
        }
    }

    return ErrorCode::Ok;
}

template <typename T>
struct ViewResult {
    const T* value{nullptr};
    ErrorCode error{ErrorCode::Ok};

    constexpr bool ok() const noexcept { return error == ErrorCode::Ok && value != nullptr; }
    constexpr bool has_value() const noexcept { return value != nullptr; }
    constexpr explicit operator bool() const noexcept { return ok(); }
    constexpr const T* operator->() const noexcept { return value; }
    constexpr const T& operator*() const noexcept { return *value; }
};

template <typename T>
inline ViewResult<T> view_result(const void* buffer, size_t size, bool verify_crc = false) noexcept {
    ErrorCode err = validate<T>(buffer, size, verify_crc);
    if (err != ErrorCode::Ok) {
        return ViewResult<T>{nullptr, err};
    }
    const auto* byte_ptr = static_cast<const uint8_t*>(buffer);
    return ViewResult<T>{reinterpret_cast<const T*>(byte_ptr + sizeof(Header)), ErrorCode::Ok};
}

template <typename T>
inline const T* view(const void* buffer, size_t size, bool verify_crc = false) noexcept {
    if (validate<T>(buffer, size, verify_crc) != ErrorCode::Ok) {
        return nullptr;
    }
    const auto* byte_ptr = static_cast<const uint8_t*>(buffer);
    return reinterpret_cast<const T*>(byte_ptr + sizeof(Header));
}

template <typename T, size_t N>
inline const T* view(const uint8_t (&buffer)[N]) noexcept {
    return view<T>(static_cast<const void*>(buffer), N, false);
}

template <typename T, size_t N, typename BoolType,
          typename = std::enable_if_t<std::is_same_v<BoolType, bool>>>
inline const T* view(const uint8_t (&buffer)[N], BoolType verify_crc) noexcept {
    return view<T>(static_cast<const void*>(buffer), N, verify_crc);
}

template <typename T>
inline T* prepare(void* buffer, size_t capacity) noexcept {
    check_message_type_constraints<T>();

    if (buffer == nullptr || capacity < sizeof(Header) + sizeof(T)) {
        return nullptr;
    }

    auto* byte_ptr = static_cast<uint8_t*>(buffer);
    auto* payload_ptr = byte_ptr + sizeof(Header);

    if (reinterpret_cast<uintptr_t>(payload_ptr) % alignof(T) != 0) {
        return nullptr;
    }

    auto* hdr = reinterpret_cast<Header*>(buffer);
    hdr->magic = SINEW_MAGIC;
    hdr->msg_id = MessageTraits<T>::id;
    hdr->version = MessageTraits<T>::version;
    hdr->crc = 0;
    hdr->payload_len = static_cast<uint32_t>(sizeof(T));

    // Explicitly zero the payload memory to guarantee all inter-field alignment padding
    // bytes are deterministic for reproducible CRC32 checksums.
    std::memset(payload_ptr, 0, sizeof(T));
    return new (payload_ptr) T;
}

template <typename T, size_t N>
inline T* prepare(uint8_t (&buffer)[N]) noexcept {
    return prepare<T>(static_cast<void*>(buffer), N);
}

template <typename T>
inline size_t finalize(T* payload, bool compute_crc = true) noexcept {
    check_message_type_constraints<T>();
    if (payload == nullptr) {
        return 0;
    }

    auto* byte_ptr = reinterpret_cast<uint8_t*>(payload);
    auto* hdr = reinterpret_cast<Header*>(byte_ptr - sizeof(Header));

    if (compute_crc) {
        hdr->crc = crc32(payload, sizeof(T));
    } else {
        hdr->crc = 0;
    }

    return sizeof(Header) + sizeof(T);
}

template <typename T>
inline const Header& get_header(const T* payload) noexcept {
    const auto* byte_ptr = reinterpret_cast<const uint8_t*>(payload);
    return *reinterpret_cast<const Header*>(byte_ptr - sizeof(Header));
}

template <typename T>
inline Header& get_header(T* payload) noexcept {
    auto* byte_ptr = reinterpret_cast<uint8_t*>(payload);
    return *reinterpret_cast<Header*>(byte_ptr - sizeof(Header));
}

} // namespace sinew
