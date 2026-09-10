#pragma once

#include <sinew/types.hpp>
#include <sinew/message.hpp>
#include <sinew/wire.hpp>
#include <cstdint>
#include <cstddef>
#include <utility>

namespace sinew {

/**
 * @brief Lightweight view of an untyped Sinew wire packet.
 */
struct PacketView {
    Header header{};
    const uint8_t* payload{nullptr};
    size_t payload_len{0};

    constexpr bool is_valid() const noexcept {
        return header.magic == SINEW_MAGIC && payload != nullptr;
    }
};

/**
 * @brief Inspect an unparsed buffer to validate the header and extract payload boundaries.
 */
inline ErrorCode inspect_packet(const void* buffer, size_t size, PacketView& out_view) noexcept {
    if (buffer == nullptr || size < sizeof(Header)) {
        return ErrorCode::BufferTooSmall;
    }

    const auto* hdr = reinterpret_cast<const Header*>(buffer);
    if (hdr->magic != SINEW_MAGIC) {
        return ErrorCode::BadMagic;
    }

    if (size < sizeof(Header) + hdr->payload_len) {
        return ErrorCode::BufferTooSmall;
    }

    out_view.header = *hdr;
    out_view.payload = static_cast<const uint8_t*>(buffer) + sizeof(Header);
    out_view.payload_len = hdr->payload_len;
    return ErrorCode::Ok;
}

namespace detail {

template <typename Msg, typename Visitor>
inline bool try_dispatch_single(uint16_t msg_id, const void* buffer, size_t size, Visitor&& visitor, bool verify_crc) {
    if (MessageTraits<Msg>::id == msg_id) {
        const Msg* msg = view<Msg>(buffer, size, verify_crc);
        if (msg != nullptr) {
            visitor(*msg);
            return true;
        }
    }
    return false;
}

} // namespace detail

/**
 * @brief Type-safe pattern-matching dispatcher for heterogeneous Sinew message streams.
 *
 * Checks the packet's message ID against the provided message types and dispatches
 * to the visitor function/lambda matching that type.
 *
 * @tparam MsgTypes Variadic list of Sinew message types to match against.
 * @param buffer Raw buffer containing a Sinew packet.
 * @param size Buffer size in bytes.
 * @param visitor A callable / overloaded visitor accepting (const MsgType&).
 * @param verify_crc Whether to verify CRC32 before invoking the visitor.
 * @return true if a message type matched and visitor was called; false otherwise.
 */
template <typename... MsgTypes, typename Visitor>
inline bool dispatch(const void* buffer, size_t size, Visitor&& visitor, bool verify_crc = false) {
    if (buffer == nullptr || size < sizeof(Header)) {
        return false;
    }

    const auto* hdr = reinterpret_cast<const Header*>(buffer);
    if (hdr->magic != SINEW_MAGIC) {
        return false;
    }

    return (detail::try_dispatch_single<MsgTypes>(
        hdr->msg_id, buffer, size, std::forward<Visitor>(visitor), verify_crc
    ) || ...);
}

} // namespace sinew
