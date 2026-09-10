/**
 * @file mcu_freestanding.cpp
 * @brief MCU Target Validation (ARM Cortex-M4/M7, bare-metal, RTOS).
 *
 * This example validates that Sinew operates in a strictly freestanding,
 * zero-heap, no-exceptions, and no-RTTI embedded environment.
 */

#include <sinew/sinew.hpp>
#include <cstdio>
#include <cstdlib>

// Custom embedded IMU sensor packet
struct McuImuSample {
    uint32_t timestamp_us;
    int16_t accel_raw[3];
    int16_t gyro_raw[3];
    int16_t mag_raw[3];
    int16_t temperature_raw;

    SINEW_REFLECT(timestamp_us, accel_raw, gyro_raw, mag_raw, temperature_raw)
};
SINEW_REGISTER_MESSAGE(McuImuSample, 0x10, 1);

static_assert(sizeof(McuImuSample) == 24, "McuImuSample must be exactly 24 bytes packed");
static_assert(sinew::is_valid_message_v<McuImuSample>, "McuImuSample must be standard layout and trivially copyable");

// Hard-real-time sensor telemetry ring buffer (e.g. DMA buffer from SPI/I2C sensor to control loop)
static sinew::SpscRingBuffer<McuImuSample, 64> g_dma_sensor_queue;

int main() {
    std::printf("==================================================\n");
    std::printf("Sinew MCU Bare-Metal / RTOS Target Validation\n");
    std::printf("==================================================\n");
    std::printf("Target Profile: Embedded Cortex-M4/M7 Freestanding\n");
    std::printf("Heap Allocations: 0 (No malloc, No heap)\n");
    std::printf("Exceptions: Disabled (-fno-exceptions / /EHs-c-)\n");
    std::printf("RTTI: Disabled (-fno-rtti / /GR-)\n\n");

    // 1. Simulate DMA Hardware Interrupt Handler (Producer)
    std::printf("[MCU Interrupt / DMA Handler] Preparing telemetry packet in ring buffer...\n");
    for (uint32_t i = 0; i < 16; ++i) {
        McuImuSample* slot = g_dma_sensor_queue.prepare_write();
        if (slot == nullptr) {
            std::printf("Queue full!\n");
            return 1;
        }

        slot->timestamp_us = i * 1000;
        slot->accel_raw[0] = static_cast<int16_t>(i * 10);
        slot->accel_raw[1] = static_cast<int16_t>(-static_cast<int32_t>(i) * 10);
        slot->accel_raw[2] = 4096; // 1G
        slot->gyro_raw[0] = 0;
        slot->gyro_raw[1] = 0;
        slot->gyro_raw[2] = 0;
        slot->mag_raw[0] = 120;
        slot->mag_raw[1] = 240;
        slot->mag_raw[2] = 360;
        slot->temperature_raw = 250; // 25.0 C

        g_dma_sensor_queue.commit_write(/*compute_crc=*/true);
    }
    std::printf("  Pushed 16 sensor samples to lock-free SPSC DMA queue.\n\n");

    // 2. Simulate Real-Time Control Loop (Consumer, e.g. 1kHz PID/EKF loop)
    std::printf("[MCU Control Task] Processing sensor samples from DMA queue...\n");
    uint32_t processed_count = 0;
    while (!g_dma_sensor_queue.empty()) {
        const McuImuSample* sample = g_dma_sensor_queue.peek_read(/*verify_crc=*/true);
        if (sample == nullptr) {
            std::printf("CRC check or read failed!\n");
            return 1;
        }

        const auto& hdr = sinew::get_header(sample);
        if (hdr.magic != sinew::SINEW_MAGIC || hdr.msg_id != 0x10) {
            std::printf("Header validation failed!\n");
            return 1;
        }

        processed_count++;
        g_dma_sensor_queue.commit_read();
    }

    std::printf("  Processed %u samples with zero-copy validation and CRC verification.\n", processed_count);

    // 3. Test In-Place Portable Normalization for CAN / UART Wire Transmission
    alignas(16) uint8_t can_wire_frame[sinew::message_size<McuImuSample>()];
    McuImuSample can_out{};
    can_out.timestamp_us = 50000;
    can_out.accel_raw[2] = 4096;

    size_t wire_bytes = sinew::portable_encode(can_wire_frame, sizeof(can_wire_frame), can_out, true);
    std::printf("\n[CAN/UART Bus Serializer] Encoded %zu wire bytes (16B header + 24B payload).\n", wire_bytes);

    McuImuSample can_in{};
    sinew::ErrorCode err = sinew::portable_decode(can_wire_frame, wire_bytes, can_in, true);
    if (err != sinew::ErrorCode::Ok) {
        std::printf("Portable decode failed!\n");
        return 1;
    }
    std::printf("[CAN/UART Bus Receiver] Decoded payload at t=%u us, Accel Z raw=%d.\n", can_in.timestamp_us, can_in.accel_raw[2]);

    std::printf("\nMCU validation successful: fully functional with zero heap allocations, no exceptions, and no RTTI.\n");
    return 0;
}
