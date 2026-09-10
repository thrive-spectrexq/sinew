#pragma once

#include <sinew/types.hpp>
#include <sinew/wire.hpp>
#include <sinew/message.hpp>
#include <sinew/dispatch.hpp>
#include <sinew/shm.hpp>
#include <sinew/ipc_ring_buffer.hpp>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <string_view>
#include <utility>

namespace sinew {

inline constexpr uint32_t IPC_ENVELOPE_MAGIC = 0x45564D53u; // 'SMVE' (Sinew Message Variable Envelope)

/**
 * @brief Heterogeneous Inter-Process Communication (IPC) Ring Buffer.
 *
 * Allows producers to send arbitrary registered Sinew message types (up to MaxPayloadSize)
 * across processes over a single shared-memory bus. Consumers can inspect incoming packets
 * and dispatch them using type-safe visitor pattern matching with zero heap allocation.
 *
 * @tparam MaxPayloadSize Maximum payload size in bytes for any message on this bus.
 * @tparam Capacity Number of ring buffer slots (must be a power of two).
 */
template <size_t MaxPayloadSize = 256, size_t Capacity = 1024>
class IpcEnvelopeRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");
    static_assert(Capacity >= 2, "Capacity must be at least 2");
    static_assert(MaxPayloadSize > 0, "MaxPayloadSize must be greater than zero");

    static constexpr size_t kSlotAlign = 64;
    static constexpr size_t kRawSlotSize = sizeof(Header) + MaxPayloadSize;
    static constexpr size_t kSlotSize = ((kRawSlotSize + kSlotAlign - 1) / kSlotAlign) * kSlotAlign;
    static constexpr size_t kTotalShmSize = sizeof(IpcRingHeader) + (Capacity * kSlotSize);
    static constexpr size_t kIndexMask = Capacity - 1;

public:
    IpcEnvelopeRingBuffer() noexcept = default;
    ~IpcEnvelopeRingBuffer() = default;

    IpcEnvelopeRingBuffer(const IpcEnvelopeRingBuffer&) = delete;
    IpcEnvelopeRingBuffer& operator=(const IpcEnvelopeRingBuffer&) = delete;

    IpcEnvelopeRingBuffer(IpcEnvelopeRingBuffer&&) noexcept = default;
    IpcEnvelopeRingBuffer& operator=(IpcEnvelopeRingBuffer&&) noexcept = default;

    /**
     * @brief Create a new shared-memory bus segment and initialize as Producer.
     */
    static IpcEnvelopeRingBuffer create(std::string_view name) {
        SharedMemoryRegion shm = SharedMemoryRegion::create(name, kTotalShmSize);
        if (!shm.is_valid()) {
            return IpcEnvelopeRingBuffer{};
        }

        auto* hdr = reinterpret_cast<IpcRingHeader*>(shm.data());
        hdr->magic = IPC_ENVELOPE_MAGIC;
        hdr->version = 1;
        hdr->capacity = static_cast<uint32_t>(Capacity);
        hdr->slot_size = static_cast<uint32_t>(kSlotSize);
        hdr->payload_len = static_cast<uint32_t>(MaxPayloadSize);
        hdr->msg_id = 0; // Heterogeneous bus carries multiple message IDs
        hdr->reserved = 0;
        hdr->head.store(0, std::memory_order_relaxed);
        hdr->tail.store(0, std::memory_order_relaxed);

        IpcEnvelopeRingBuffer rb;
        rb.shm_ = std::move(shm);
        rb.hdr_ = hdr;
        rb.slots_base_ = static_cast<uint8_t*>(rb.shm_.data()) + sizeof(IpcRingHeader);
        rb.is_producer_ = true;
        return rb;
    }

    /**
     * @brief Attach to an existing shared-memory bus segment as Consumer.
     */
    static IpcEnvelopeRingBuffer open(std::string_view name) {
        SharedMemoryRegion shm = SharedMemoryRegion::open(name, kTotalShmSize);
        if (!shm.is_valid()) {
            return IpcEnvelopeRingBuffer{};
        }

        auto* hdr = reinterpret_cast<IpcRingHeader*>(shm.data());
        if (hdr->magic != IPC_ENVELOPE_MAGIC ||
            hdr->capacity != Capacity ||
            hdr->slot_size != kSlotSize ||
            hdr->payload_len != MaxPayloadSize) {
            return IpcEnvelopeRingBuffer{};
        }

        IpcEnvelopeRingBuffer rb;
        rb.shm_ = std::move(shm);
        rb.hdr_ = hdr;
        rb.slots_base_ = static_cast<uint8_t*>(rb.shm_.data()) + sizeof(IpcRingHeader);
        rb.is_producer_ = false;
        return rb;
    }

    bool is_valid() const noexcept {
        return hdr_ != nullptr && slots_base_ != nullptr;
    }

    explicit operator bool() const noexcept {
        return is_valid();
    }

    static constexpr size_t capacity() noexcept { return Capacity; }
    static constexpr size_t max_payload_size() noexcept { return MaxPayloadSize; }
    static constexpr size_t slot_size() noexcept { return kSlotSize; }
    static constexpr size_t required_shm_size() noexcept { return kTotalShmSize; }

    size_t size() const noexcept {
        if (!is_valid()) return 0;
        const uint64_t head = hdr_->head.load(std::memory_order_relaxed);
        const uint64_t tail = hdr_->tail.load(std::memory_order_relaxed);
        return (head >= tail) ? static_cast<size_t>(head - tail) : 0;
    }

    bool empty() const noexcept {
        if (!is_valid()) return true;
        return hdr_->head.load(std::memory_order_relaxed) == hdr_->tail.load(std::memory_order_relaxed);
    }

    bool full() const noexcept {
        if (!is_valid()) return true;
        const uint64_t head = hdr_->head.load(std::memory_order_relaxed);
        const uint64_t tail = hdr_->tail.load(std::memory_order_relaxed);
        return (head - tail) >= Capacity;
    }

    // --- PRODUCER API ---

    /**
     * @brief Prepare a typed message directly inside the next ring buffer slot.
     */
    template <typename Msg>
    Msg* prepare_write() noexcept {
        static_assert(is_valid_message_v<Msg>, "Sinew message types must be standard layout and trivially copyable");
        static_assert(sizeof(Msg) <= MaxPayloadSize, "Message size exceeds MaxPayloadSize of this bus");

        if (!is_valid()) return nullptr;

        const uint64_t head = hdr_->head.load(std::memory_order_relaxed);
        const uint64_t tail = hdr_->tail.load(std::memory_order_acquire);

        if ((head - tail) >= Capacity) {
            return nullptr; // Buffer full
        }

        uint8_t* slot = slots_base_ + ((head & kIndexMask) * kSlotSize);
        return sinew::prepare<Msg>(slot, kSlotSize);
    }

    /**
     * @brief Finalize and commit the current message to the bus.
     */
    void commit_write(bool compute_crc = false) noexcept {
        if (!is_valid()) return;

        const uint64_t head = hdr_->head.load(std::memory_order_relaxed);
        uint8_t* slot = slots_base_ + ((head & kIndexMask) * kSlotSize);
        auto* hdr = reinterpret_cast<Header*>(slot);
        auto* payload = slot + sizeof(Header);

        if (compute_crc && hdr->payload_len > 0) {
            hdr->crc = crc32(payload, hdr->payload_len);
        } else {
            hdr->crc = 0;
        }

        hdr_->head.store(head + 1, std::memory_order_release);
    }

    /**
     * @brief Push an existing message value to the bus.
     */
    template <typename Msg>
    bool push(const Msg& value, bool compute_crc = false) noexcept {
        Msg* slot = prepare_write<Msg>();
        if (slot == nullptr) return false;
        *slot = value;
        commit_write(compute_crc);
        return true;
    }

    // --- CONSUMER API ---

    /**
     * @brief Peek at the raw packet envelope at the current read cursor.
     */
    PacketView peek_packet(bool verify_crc = false) noexcept {
        if (!is_valid()) return PacketView{};

        const uint64_t tail = hdr_->tail.load(std::memory_order_relaxed);
        const uint64_t head = hdr_->head.load(std::memory_order_acquire);

        if (tail == head) {
            return PacketView{}; // Empty
        }

        const uint8_t* slot = slots_base_ + ((tail & kIndexMask) * kSlotSize);
        PacketView pv;
        if (inspect_packet(slot, kSlotSize, pv) != ErrorCode::Ok) {
            return PacketView{};
        }

        if (verify_crc && pv.header.crc != 0) {
            if (crc32(pv.payload, pv.payload_len) != pv.header.crc) {
                return PacketView{}; // Checksum failure
            }
        }

        return pv;
    }

    /**
     * @brief Peek at a typed message if the next packet matches Msg.
     */
    template <typename Msg>
    const Msg* peek_msg(bool verify_crc = false) noexcept {
        static_assert(is_valid_message_v<Msg>, "Sinew message types must be standard layout and trivially copyable");
        if (!is_valid()) return nullptr;

        const uint64_t tail = hdr_->tail.load(std::memory_order_relaxed);
        const uint64_t head = hdr_->head.load(std::memory_order_acquire);

        if (tail == head) return nullptr;

        const uint8_t* slot = slots_base_ + ((tail & kIndexMask) * kSlotSize);
        return sinew::view<Msg>(slot, kSlotSize, verify_crc);
    }

    /**
     * @brief Dispatch the current packet to a matching visitor lambda/function.
     */
    template <typename... MsgTypes, typename Visitor>
    bool dispatch(Visitor&& visitor, bool verify_crc = false) {
        if (!is_valid()) return false;

        const uint64_t tail = hdr_->tail.load(std::memory_order_relaxed);
        const uint64_t head = hdr_->head.load(std::memory_order_acquire);

        if (tail == head) return false;

        const uint8_t* slot = slots_base_ + ((tail & kIndexMask) * kSlotSize);
        return sinew::dispatch<MsgTypes...>(slot, kSlotSize, std::forward<Visitor>(visitor), verify_crc);
    }

    /**
     * @brief Dispatch the current packet and automatically advance the read cursor if matched.
     */
    template <typename... MsgTypes, typename Visitor>
    bool dispatch_and_commit(Visitor&& visitor, bool verify_crc = false) {
        if (dispatch<MsgTypes...>(std::forward<Visitor>(visitor), verify_crc)) {
            commit_read();
            return true;
        }
        return false;
    }

    /**
     * @brief Advance the read cursor.
     */
    void commit_read() noexcept {
        if (!is_valid()) return;

        const uint64_t tail = hdr_->tail.load(std::memory_order_relaxed);
        hdr_->tail.store(tail + 1, std::memory_order_release);
    }

private:
    SharedMemoryRegion shm_{};
    IpcRingHeader* hdr_{nullptr};
    uint8_t* slots_base_{nullptr};
    bool is_producer_{false};
};

} // namespace sinew
