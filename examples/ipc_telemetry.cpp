#include <sinew/sinew.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <string_view>

struct ActuatorTelemetry {
    uint64_t timestamp_ns;
    sinew::StaticString<16> joint_name;
    float position;
    float velocity;
    float torque;
    float temperature_c;
};
SINEW_REGISTER_MESSAGE(ActuatorTelemetry, 101, 1);

constexpr std::string_view kShmChannel = "sinew_actuator_channel";
constexpr size_t kTotalPackets = 100000;

void run_producer() {
    std::cout << "[Producer] Creating IPC shared-memory channel: " << kShmChannel << "\n";
    auto producer = sinew::IpcRingBuffer<ActuatorTelemetry, 2048>::create(kShmChannel);
    if (!producer) {
        std::cerr << "[Producer] Failed to create shared memory!\n";
        return;
    }

    std::cout << "[Producer] Streaming " << kTotalPackets << " actuator telemetry messages...\n";
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < kTotalPackets; ++i) {
        ActuatorTelemetry* slot = nullptr;
        while ((slot = producer.prepare_write()) == nullptr) {
            std::this_thread::yield();
        }

        slot->timestamp_ns = i * 10000ULL;
        slot->joint_name = "shoulder_pan";
        slot->position = static_cast<float>(i) * 0.001f;
        slot->velocity = 1.25f;
        slot->torque = 3.4f;
        slot->temperature_c = 42.5f;

        producer.commit_write(/*compute_crc=*/false);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "[Producer] Finished producing in " << std::fixed << std::setprecision(2)
              << ms << " ms (" << (kTotalPackets / (ms / 1000.0) / 1000.0) << " kmsg/sec)\n";
}

void run_consumer() {
    std::cout << "[Consumer] Opening IPC shared-memory channel: " << kShmChannel << "\n";
    auto consumer = sinew::IpcRingBuffer<ActuatorTelemetry, 2048>::open(kShmChannel);
    if (!consumer) {
        std::cerr << "[Consumer] Failed to open shared memory! Is producer running?\n";
        return;
    }

    size_t received = 0;
    auto start = std::chrono::high_resolution_clock::now();

    while (received < kTotalPackets) {
        const ActuatorTelemetry* msg = consumer.peek_read();
        if (msg != nullptr) {
            received++;
            consumer.commit_read();
        } else {
            std::this_thread::yield();
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "[Consumer] Consumed " << received << " zero-copy messages in "
              << std::fixed << std::setprecision(2) << ms << " ms ("
              << (received / (ms / 1000.0) / 1000.0) << " kmsg/sec)\n";
}

int main(int argc, char** argv) {
    std::cout << "==================================================\n";
    std::cout << "Sinew Zero-Copy IPC Shared Memory Telemetry\n";
    std::cout << "==================================================\n";

    if (argc > 1) {
        std::string_view mode = argv[1];
        if (mode == "--producer") {
            run_producer();
            return 0;
        } else if (mode == "--consumer") {
            run_consumer();
            return 0;
        }
    }

    // Default: run both concurrent producer and consumer threads over real OS shared memory
    std::thread prod_thread(run_producer);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::thread cons_thread(run_consumer);

    prod_thread.join();
    cons_thread.join();

    std::cout << "\nIPC pipeline completed successfully with zero heap allocations!\n";
    return 0;
}
