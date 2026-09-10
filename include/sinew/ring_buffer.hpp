#pragma once

#include <sinew/types.hpp>
#include <sinew/wire.hpp>
#include <sinew/message.hpp>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <new>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

namespace sinew {

// Single-Producer Single-Consumer (SPSC) Lock-Free Zero-Copy Ring Buffer
// Capacity must be a power of two.
template <typename T, size_t Capacity = 1024>
class SpscRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");
    static_assert(Capacity >= 2, "Capacity must be at least 2");
    static_assert(is_valid_message_v<T>, "Sinew message types must be standard layout and trivially copyable");

    static constexpr size_t kSlotSize = sizeof(Header) + sizeof(T);
    static constexpr size_t kSlotAlign = (alignof(T) > alignof(Header)) ? alignof(T) : alignof(Header);

    struct alignas(kSlotAlign) Slot {
        uint8_t buffer[kSlotSize];
    };

    static constexpr size_t kIndexMask = Capacity - 1;

public:
    SpscRingBuffer() noexcept 
        : head_(0), tail_(0) {
    }

    // Zero-allocation, non-copyable for concurrency safety
    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;

    // Capacity query
    static constexpr size_t capacity() noexcept {
        return Capacity;
    }

    // Approximate size (producers/consumers can read without locking)
    size_t size() const noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t tail = tail_.load(std::memory_order_relaxed);
        return (head >= tail) ? (head - tail) : (Capacity + head - tail);
    }

    bool empty() const noexcept {
        return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed);
    }

    bool full() const noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t tail = tail_.load(std::memory_order_relaxed);
        return (head - tail) >= Capacity;
    }

    // PRODUCER API: Zero-copy write directly into ring buffer slot
    T* prepare_write() noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t tail = tail_.load(std::memory_order_acquire);

        if ((head - tail) >= Capacity) {
            return nullptr; // Buffer is full
        }

        Slot& slot = slots_[head & kIndexMask];
        return sinew::prepare<T>(slot.buffer, kSlotSize);
    }

    // PRODUCER API: Finalize message and commit slot into ring buffer
    void commit_write(bool compute_crc = false) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        Slot& slot = slots_[head & kIndexMask];
        auto* payload = reinterpret_cast<T*>(slot.buffer + sizeof(Header));
        sinew::finalize(payload, compute_crc);

        head_.store(head + 1, std::memory_order_release);
    }

    // Producer convenience: Push an existing object (single memcpy)
    bool push(const T& value, bool compute_crc = false) noexcept {
        T* slot = prepare_write();
        if (slot == nullptr) {
            return false;
        }
        *slot = value;
        commit_write(compute_crc);
        return true;
    }

    // CONSUMER API: Zero-copy read directly from current slot
    const T* peek_read(bool verify_crc = false) noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t head = head_.load(std::memory_order_acquire);

        if (tail == head) {
            return nullptr; // Buffer is empty
        }

        const Slot& slot = slots_[tail & kIndexMask];
        return sinew::view<T>(slot.buffer, kSlotSize, verify_crc);
    }

    // CONSUMER API: Advance read pointer after consuming slot
    void commit_read() noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        tail_.store(tail + 1, std::memory_order_release);
    }

    // Consumer convenience: Pop into value
    bool pop(T& out_value, bool verify_crc = false) noexcept {
        const T* item = peek_read(verify_crc);
        if (item == nullptr) {
            return false;
        }
        out_value = *item;
        commit_read();
        return true;
    }

private:
    // Align head and tail to distinct 64-byte cache lines to eliminate false sharing
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};

    // Buffer slots
    alignas(64) Slot slots_[Capacity];
};

} // namespace sinew

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
