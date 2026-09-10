#include <sinew/sinew.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <memory>
#include <atomic>

// Define telemetry message
struct ImuSample {
    uint64_t timestamp_ns;
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
};

// Register message with type id 42, version 1
SINEW_REGISTER_MESSAGE(ImuSample, 42, 1);

int main() {
    std::cout << "==================================================\n";
    std::cout << "Sinew Robotics IMU Telemetry Pipeline Example\n";
    std::cout << "==================================================\n\n";

    // 1. Direct Stack / Network Buffer Zero-Copy Wire Demo
    alignas(alignof(ImuSample)) uint8_t wire_packet[sinew::message_size<ImuSample>()];

    std::cout << "[Step 1] Preparing message directly in buffer...\n";
    ImuSample* sample = sinew::prepare<ImuSample>(wire_packet);
    if (!sample) {
        std::cerr << "Failed to prepare wire buffer!\n";
        return 1;
    }

    sample->timestamp_ns = 1700000000000ULL;
    sample->accel_x = 0.012f;
    sample->accel_y = -0.045f;
    sample->accel_z = 9.806f;
    sample->gyro_x = 0.001f;
    sample->gyro_y = -0.002f;
    sample->gyro_z = 0.0005f;

    size_t packet_size = sinew::finalize(sample, /*compute_crc=*/true);
    std::cout << "Message finalized: " << packet_size << " wire bytes (16B header + "
              << sizeof(ImuSample) << "B payload)\n\n";

    std::cout << "[Step 2] Zero-copy view from wire buffer...\n";
    const ImuSample* view = sinew::view<ImuSample>(wire_packet, packet_size, /*verify_crc=*/true);
    if (!view) {
        std::cerr << "Validation failed!\n";
        return 1;
    }

    const auto& hdr = sinew::get_header(view);
    std::cout << std::hex
              << "  Magic: 0x" << hdr.magic << " (valid)\n"
              << std::dec
              << "  Msg ID: " << hdr.msg_id << "\n"
              << "  Version: " << hdr.version << "\n"
              << std::hex << "  CRC32: 0x" << hdr.crc << "\n" << std::dec
              << "  Accel: [" << view->accel_x << ", " << view->accel_y << ", " << view->accel_z << "] m/s^2\n"
              << "  Gyro:  [" << view->gyro_x << ", " << view->gyro_y << ", " << view->gyro_z << "] rad/s\n\n";

    // 2. High-Frequency Real-Time SPSC Ring Buffer Pipeline
    std::cout << "[Step 3] Running 10,000 Hz real-time sensor loop simulation over SPSC ring buffer...\n";
    auto ring_buffer = std::make_unique<sinew::SpscRingBuffer<ImuSample, 4096>>();
    constexpr size_t kSamplesToSend = 50000;

    // Consumer (e.g. Attitude Estimator / State Filter)
    std::thread filter_thread([&]() {
        size_t samples_received = 0;
        double sum_az = 0.0;
        while (samples_received < kSamplesToSend) {
            const ImuSample* in = ring_buffer->peek_read();
            if (in != nullptr) {
                sum_az += in->accel_z;
                samples_received++;
                ring_buffer->commit_read();
            } else {
                std::this_thread::yield();
            }
        }
        std::cout << "  Consumer received: " << samples_received << " samples.\n";
        std::cout << "  Average Accel Z: " << (sum_az / samples_received) << " m/s^2\n";
    });

    // Producer (e.g. Hardware Sensor SPI/I2C DMA Driver)
    auto start_time = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < kSamplesToSend; ++i) {
        ImuSample* slot = nullptr;
        while ((slot = ring_buffer->prepare_write()) == nullptr) {
            std::this_thread::yield();
        }
        slot->timestamp_ns = i * 100000ULL;
        slot->accel_x = 0.0f;
        slot->accel_y = 0.0f;
        slot->accel_z = 9.81f;
        slot->gyro_x = 0.0f;
        slot->gyro_y = 0.0f;
        slot->gyro_z = 0.0f;
        ring_buffer->commit_write(/*compute_crc=*/false); // ultra-fast local transport
    }

    filter_thread.join();
    auto end_time = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    std::cout << "  Processed " << kSamplesToSend << " packets in " << std::fixed << std::setprecision(2)
              << total_ms << " ms ("
              << (kSamplesToSend / (total_ms / 1000.0) / 1000.0) << " thousand msg/sec)\n";
    std::cout << "\nPipeline completed successfully! Zero heap allocations on the hot path.\n";
    return 0;
}
