#include <sinew/sinew.hpp>
#include <iostream>
#include <chrono>
#include <vector>
#include <numeric>
#include <iomanip>

struct BenchmarkTelemetry {
    uint64_t timestamp;
    double lat;
    double lon;
    float alt;
    float velocity_x;
    float velocity_y;
    float velocity_z;
};
SINEW_REGISTER_MESSAGE(BenchmarkTelemetry, 99, 1);

int main() {
    std::cout << "==================================================\n";
    std::cout << "Sinew Performance Micro-Benchmarks\n";
    std::cout << "==================================================\n\n";

    constexpr size_t kIterations = 2000000;
    alignas(BenchmarkTelemetry) uint8_t buffer[sinew::message_size<BenchmarkTelemetry>()];

    // Benchmark 1: Encode (prepare + write + finalize without CRC)
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < kIterations; ++i) {
            BenchmarkTelemetry* out = sinew::prepare<BenchmarkTelemetry>(buffer);
            out->timestamp = i;
            out->lat = 37.7749;
            out->lon = -122.4194;
            out->alt = 100.0f;
            out->velocity_x = 1.0f;
            out->velocity_y = 2.0f;
            out->velocity_z = 0.5f;
            sinew::finalize(out, /*compute_crc=*/false);
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ns_total = std::chrono::duration<double, std::nano>(end - start).count();
        double ns_per_op = ns_total / kIterations;
        double ops_per_sec = (kIterations / (ns_total / 1e9)) / 1e6;

        std::cout << "[Encode (No CRC)]       "
                  << std::fixed << std::setprecision(2)
                  << ns_per_op << " ns/msg  |  "
                  << ops_per_sec << " Million msgs/sec\n";
    }

    // Benchmark 2: Encode (prepare + write + finalize with CRC-32)
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < kIterations; ++i) {
            BenchmarkTelemetry* out = sinew::prepare<BenchmarkTelemetry>(buffer);
            out->timestamp = i;
            out->lat = 37.7749;
            out->lon = -122.4194;
            out->alt = 100.0f;
            out->velocity_x = 1.0f;
            out->velocity_y = 2.0f;
            out->velocity_z = 0.5f;
            sinew::finalize(out, /*compute_crc=*/true);
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ns_total = std::chrono::duration<double, std::nano>(end - start).count();
        double ns_per_op = ns_total / kIterations;
        double ops_per_sec = (kIterations / (ns_total / 1e9)) / 1e6;

        std::cout << "[Encode (With CRC-32)]  "
                  << std::fixed << std::setprecision(2)
                  << ns_per_op << " ns/msg  |  "
                  << ops_per_sec << " Million msgs/sec\n";
    }

    // Benchmark 3: Decode (Zero-Copy View without CRC)
    {
        // First encode a message
        BenchmarkTelemetry* out = sinew::prepare<BenchmarkTelemetry>(buffer);
        out->timestamp = 123456789;
        out->lat = 37.7749;
        out->lon = -122.4194;
        out->alt = 100.0f;
        sinew::finalize(out, false);

        volatile double sink = 0;
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < kIterations; ++i) {
            const BenchmarkTelemetry* in = sinew::view<BenchmarkTelemetry>(buffer, sizeof(buffer), false);
            sink = sink + in->lat + in->alt;
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ns_total = std::chrono::duration<double, std::nano>(end - start).count();
        double ns_per_op = ns_total / kIterations;
        double ops_per_sec = (kIterations / (ns_total / 1e9)) / 1e6;

        std::cout << "[Zero-Copy View]        "
                  << std::fixed << std::setprecision(2)
                  << ns_per_op << " ns/msg  |  "
                  << ops_per_sec << " Million msgs/sec\n";
    }

    std::cout << "\nBenchmarks completed successfully.\n";
    return 0;
}
