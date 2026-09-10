#pragma once

#include <sinew/types.hpp>
#include <sinew/wire.hpp>
#include <sinew/message.hpp>
#include <sinew/shm.hpp>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <string_view>
#include <utility>

namespace sinew {

inline constexpr uint32_t IPC_RING_MAGIC = 0x524D4853u; // 'SHMR'

/**
 * @brief 192-byte cache-aligned header for inter-process shared memory ring buffers.
 *
 * head and tail atomics are isolated on separate 64-byte cache lines to eliminate
 * false sharing between writer and reader processes.
 */
struct alignas(64) IpcRingHeader {
    uint32_t magic{0};
    uint32_t version{0};
    uint32_t capacity{0};
    uint32_t slot_size{0};
    uint32_t payload_len{0};
    uint16_t msg_id{0};
    uint16_t reserved{0};
    uint8_t  pad0[40]{};

    alignas(64) std::atomic<uint64_t> head{0};
    uint8_t  pad1[56]{};

    alignas(64) std::atomic<uint64_t> tail{0};
    uint8_t  pad2[56]{};
};

static_assert(sizeof(IpcRingHeader) == 192, "IpcRingHeader must be exactly 192 bytes (3 cache lines)");
static_assert(offsetof(IpcRingHeader, head) == 64, "head must be at offset 64");
static_assert(offsetof(IpcRingHeader, tail) == 128, "tail must be at offset 128");

/**
 * @brief Inter-Process Communication (IPC) Zero-Copy Lock-Free SPSC Ring Buffer.
 *
 * Operates on a named SharedMemoryRegion mapped across separate OS processes.
 */
template <typename T, size_t Capacity = 1024>
class IpcRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");
    static_assert(Capacity >= 2, "Capacity must be at least 2");
    static_assert(is_valid_message_v<T>, "Sinew message types must be standard layout and trivially copyable");

    static constexpr size_t kSlotAlign = (alignof(T) > 16) ? alignof(T) : 16;
    static constexpr size_t kRawSlotSize = sizeof(Header) + sizeof(T);
    static constexpr size_t kSlotSize = ((kRawSlotSize + kSlotAlign - 1) / kSlotAlign) * kSlotAlign;
    static constexpr size_t kTotalShmSize = sizeof(IpcRingHeader) + (Capacity * kSlotSize);
    static constexpr size_t kIndexMask = Capacity - 1;

public:
    IpcRingBuffer() noexcept = default;

    ~IpcRingBuffer() = default;

    IpcRingBuffer(const IpcRingBuffer&) = delete;
    IpcRingBuffer& operator=(const IpcRingBuffer&) = delete;

    IpcRingBuffer(IpcRingBuffer&&) noexcept = default;
    IpcRingBuffer& operator=(IpcRingBuffer&&) noexcept = default;

    /**
     * @brief Create a new shared-memory segment and initialize the ring buffer as Producer.
     */
    static IpcRingBuffer create(std::string_view name) {
        SharedMemoryRegion shm = SharedMemoryRegion::create(name, kTotalShmSize);
        if (!shm.is_valid()) {
            return IpcRingBuffer{};
        }

        auto* hdr = reinterpret_cast<IpcRingHeader*>(shm.data());
        hdr->magic = IPC_RING_MAGIC;
        hdr->version = 1;
        hdr->capacity = static_cast<uint32_t>(Capacity);
        hdr->slot_size = static_cast<uint32_t>(kSlotSize);
        hdr->payload_len = static_cast<uint32_t>(sizeof(T));
        hdr->msg_id = MessageTraits<T>::id;
        hdr->reserved = 0;
        hdr->head.store(0, std::memory_order_relaxed);
        hdr->tail.store(0, std::memory_order_relaxed);

        IpcRingBuffer rb;
        rb.shm_ = std::move(shm);
        rb.hdr_ = hdr;
        rb.slots_base_ = static_cast<uint8_t*>(rb.shm_.data()) + sizeof(IpcRingHeader);
        rb.is_producer_ = true;
        return rb;
    }

    /**
     * @brief Attach to an existing shared-memory segment as Consumer.
     */
    static IpcRingBuffer open(std::string_view name) {
        SharedMemoryRegion shm = SharedMemoryRegion::open(name, kTotalShmSize);
        if (!shm.is_valid()) {
            return IpcRingBuffer{};
        }

        auto* hdr = reinterpret_cast<IpcRingHeader*>(shm.data());
        if (hdr->magic != IPC_RING_MAGIC ||
            hdr->capacity != Capacity ||
            hdr->slot_size != kSlotSize ||
            hdr->payload_len != sizeof(T)) {
            return IpcRingBuffer{};
        }

        IpcRingBuffer rb;
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

    static constexpr size_t capacity() noexcept {
        return Capacity;
    }

    static constexpr size_t required_shm_size() noexcept {
        return kTotalShmSize;
    }

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

    T* prepare_write() noexcept {
        if (!is_valid()) return nullptr;

        const uint64_t head = hdr_->head.load(std::memory_order_relaxed);
        const uint64_t tail = hdr_->tail.load(std::memory_order_acquire);

        if ((head - tail) >= Capacity) {
            return nullptr; // Full
        }

        uint8_t* slot = slots_base_ + ((head & kIndexMask) * kSlotSize);
        return sinew::prepare<T>(slot, kSlotSize);
    }

    void commit_write(bool compute_crc = false) noexcept {
        if (!is_valid()) return;

        const uint64_t head = hdr_->head.load(std::memory_order_relaxed);
        uint8_t* slot = slots_base_ + ((head & kIndexMask) * kSlotSize);
        auto* payload = reinterpret_cast<T*>(slot + sizeof(Header));
        sinew::finalize(payload, compute_crc);

        hdr_->head.store(head + 1, std::memory_order_release);
    }

    bool push(const T& value, bool compute_crc = false) noexcept {
        T* slot = prepare_write();
        if (slot == nullptr) return false;
        *slot = value;
        commit_write(compute_crc);
        return true;
    }

    // --- CONSUMER API ---

    const T* peek_read(bool verify_crc = false) noexcept {
        if (!is_valid()) return nullptr;

        const uint64_t tail = hdr_->tail.load(std::memory_order_relaxed);
        const uint64_t head = hdr_->head.load(std::memory_order_acquire);

        if (tail == head) {
            return nullptr; // Empty
        }

        const uint8_t* slot = slots_base_ + ((tail & kIndexMask) * kSlotSize);
        return sinew::view<T>(slot, kSlotSize, verify_crc);
    }

    void commit_read() noexcept {
        if (!is_valid()) return;

        const uint64_t tail = hdr_->tail.load(std::memory_order_relaxed);
        hdr_->tail.store(tail + 1, std::memory_order_release);
    }

    bool pop(T& out_value, bool verify_crc = false) noexcept {
        const T* item = peek_read(verify_crc);
        if (item == nullptr) return false;
        out_value = *item;
        commit_read();
        return true;
    }

private:
    SharedMemoryRegion shm_{};
    IpcRingHeader* hdr_{nullptr};
    uint8_t* slots_base_{nullptr};
    bool is_producer_{false};
};

} // namespace sinew
