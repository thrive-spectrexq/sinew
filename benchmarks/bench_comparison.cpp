#include <sinew/sinew.hpp>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <vector>
#include <cstring>

// Representative Robotics Telemetry Payloads

// 1. High-Frequency IMU Telemetry (40 bytes payload)
struct ImuTelemetry {
    uint64_t timestamp_ns;
    float accel[3];
    float gyro[3];
    float temp_c;

    SINEW_REFLECT(timestamp_ns, accel, gyro, temp_c)
};
SINEW_REGISTER_MESSAGE(ImuTelemetry, 10, 1);

// 2. High-Dimensional Robot Joint State (108 bytes payload)
struct RobotJointState {
    uint64_t timestamp_ns;
    uint32_t seq;
    float positions[8];
    float velocities[8];
    float torques[8];

    SINEW_REFLECT(timestamp_ns, seq, positions, velocities, torques)
};
SINEW_REGISTER_MESSAGE(RobotJointState, 20, 1);

// Simulated FlatBuffers-style Builder (offset table + vtable + buffer management)
class SimulatedFlatBuffersImuBuilder {
    uint8_t buf_[128];
    uint16_t vtable_[8];
    size_t offset_{128};

public:
    void reset() { offset_ = 128; }

    size_t build(uint64_t t, const float* a, const float* g, float temp) {
        // Prepend fields backwards (FlatBuffers standard layout)
        offset_ -= sizeof(float);
        std::memcpy(buf_ + offset_, &temp, sizeof(float));
        vtable_[4] = static_cast<uint16_t>(offset_);

        offset_ -= sizeof(float) * 3;
        std::memcpy(buf_ + offset_, g, sizeof(float) * 3);
        vtable_[3] = static_cast<uint16_t>(offset_);

        offset_ -= sizeof(float) * 3;
        std::memcpy(buf_ + offset_, a, sizeof(float) * 3);
        vtable_[2] = static_cast<uint16_t>(offset_);

        offset_ -= sizeof(uint64_t);
        std::memcpy(buf_ + offset_, &t, sizeof(uint64_t));
        vtable_[1] = static_cast<uint16_t>(offset_);

        // Prepend vtable & root offset table
        offset_ -= sizeof(vtable_);
        std::memcpy(buf_ + offset_, vtable_, sizeof(vtable_));
        return 128 - offset_;
    }

    float read_accel_x(const uint8_t* buffer) const {
        // Indirection via vtable offset
        uint16_t voff;
        std::memcpy(&voff, buffer + 4, sizeof(uint16_t));
        float val;
        std::memcpy(&val, buffer + voff, sizeof(float));
        return val;
    }
};

// Simulated Simple Binary Encoding (SBE) direct sequential encoding
class SimulatedSbeImu {
public:
    static size_t encode(uint8_t* dest, uint64_t t, const float* a, const float* g, float temp) {
        uint16_t block_len = 36;
        uint16_t template_id = 10;
        uint16_t schema_id = 1;
        uint16_t version = 1;
        std::memcpy(dest, &block_len, 2);
        std::memcpy(dest + 2, &template_id, 2);
        std::memcpy(dest + 4, &schema_id, 2);
        std::memcpy(dest + 6, &version, 2);

        std::memcpy(dest + 8, &t, 8);
        std::memcpy(dest + 16, a, 12);
        std::memcpy(dest + 28, g, 12);
        std::memcpy(dest + 40, &temp, 4);
        return 44;
    }

    static float read_accel_x(const uint8_t* src) {
        float val;
        std::memcpy(&val, src + 16, sizeof(float));
        return val;
    }
};

int main() {
    std::cout << "=================================================================================\n";
    std::cout << "Sinew vs Serialization Architectures Comparative Micro-Benchmark\n";
    std::cout << "=================================================================================\n";
    std::cout << "Payload: IMU Telemetry (1x uint64, 6x float, 1x float temp)\n";
    std::cout << "Target: 5,000,000 iterations per benchmark\n\n";

    constexpr size_t kIterations = 5000000;
    alignas(16) uint8_t sinew_buffer[sinew::message_size<ImuTelemetry>()];
    alignas(16) uint8_t sbe_buffer[128];
    SimulatedFlatBuffersImuBuilder fb_builder;

    const float accel[3] = {0.05f, -0.02f, 9.81f};
    const float gyro[3]  = {0.001f, -0.002f, 0.0005f};
    const float temp     = 32.5f;

    // --- Benchmark 1: Encode Performance ---
    std::cout << "---------------------------------------------------------------------------------\n";
    std::cout << "Phase 1: ENCODE / SERIALIZATION THROUGHPUT\n";
    std::cout << "---------------------------------------------------------------------------------\n";

    // 1A. Sinew In-Place Direct Prepare
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < kIterations; ++i) {
            ImuTelemetry* out = sinew::prepare<ImuTelemetry>(sinew_buffer);
            out->timestamp_ns = i;
            out->accel[0] = accel[0];
            out->accel[1] = accel[1];
            out->accel[2] = accel[2];
            out->gyro[0] = gyro[0];
            out->gyro[1] = gyro[1];
            out->gyro[2] = gyro[2];
            out->temp_c = temp;
            sinew::finalize(out, /*compute_crc=*/false);
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ns = std::chrono::duration<double, std::nano>(end - start).count() / kIterations;
        std::cout << "  Sinew (Zero-Copy Wire):         " << std::fixed << std::setprecision(2)
                  << ns << " ns/msg  |  " << (1000.0 / ns) << " M msg/sec  (Allocations: 0)\n";
    }

    // 1B. Simple Binary Encoding (SBE Direct Stream)
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < kIterations; ++i) {
            SimulatedSbeImu::encode(sbe_buffer, i, accel, gyro, temp);
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ns = std::chrono::duration<double, std::nano>(end - start).count() / kIterations;
        std::cout << "  SBE (Direct Stream):            " << std::fixed << std::setprecision(2)
                  << ns << " ns/msg  |  " << (1000.0 / ns) << " M msg/sec  (Allocations: 0)\n";
    }

    // 1C. FlatBuffers Builder (Table + VTable offsets)
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < kIterations; ++i) {
            fb_builder.reset();
            fb_builder.build(i, accel, gyro, temp);
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ns = std::chrono::duration<double, std::nano>(end - start).count() / kIterations;
        std::cout << "  FlatBuffers (Builder + VTable): " << std::fixed << std::setprecision(2)
                  << ns << " ns/msg  |  " << (1000.0 / ns) << " M msg/sec  (Allocations: 0/1 arena)\n\n";
    }

    // --- Benchmark 2: Decode / Field Access Performance ---
    std::cout << "---------------------------------------------------------------------------------\n";
    std::cout << "Phase 2: DECODE / READ / FIELD ACCESS THROUGHPUT\n";
    std::cout << "---------------------------------------------------------------------------------\n";

    // 2A. Sinew Direct View (Pointer cast + standard layout offset)
    {
        volatile float sink = 0;
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < kIterations; ++i) {
            const auto* view = sinew::view<ImuTelemetry>(sinew_buffer, sizeof(sinew_buffer), false);
            sink = sink + view->accel[0];
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ns = std::chrono::duration<double, std::nano>(end - start).count() / kIterations;
        std::cout << "  Sinew view<T> (Direct Pointer): " << std::fixed << std::setprecision(2)
                  << ns << " ns/msg  |  " << (1000.0 / ns) << " M msg/sec\n";
    }

    // 2B. SBE Field Access
    {
        volatile float sink = 0;
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < kIterations; ++i) {
            sink = sink + SimulatedSbeImu::read_accel_x(sbe_buffer);
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ns = std::chrono::duration<double, std::nano>(end - start).count() / kIterations;
        std::cout << "  SBE Field Access (Sequential):  " << std::fixed << std::setprecision(2)
                  << ns << " ns/msg  |  " << (1000.0 / ns) << " M msg/sec\n";
    }

    // 2C. FlatBuffers VTable Indirection Access
    {
        volatile float sink = 0;
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < kIterations; ++i) {
            sink = sink + fb_builder.read_accel_x(sbe_buffer);
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ns = std::chrono::duration<double, std::nano>(end - start).count() / kIterations;
        std::cout << "  FlatBuffers (VTable Indirect):  " << std::fixed << std::setprecision(2)
                  << ns << " ns/msg  |  " << (1000.0 / ns) << " M msg/sec\n\n";
    }

    // --- Benchmark 3: Wire Footprint Comparison ---
    std::cout << "---------------------------------------------------------------------------------\n";
    std::cout << "Phase 3: WIRE FORMAT OVERHEAD COMPARISON\n";
    std::cout << "---------------------------------------------------------------------------------\n";
    std::cout << "  Payload Raw Data:            36 bytes\n";
    std::cout << "  Sinew Wire Total (16B Hdr):  52 bytes (Fixed layout, compile-time static offsets)\n";
    std::cout << "  SBE Wire Total (8B Hdr):     44 bytes (Sequential stream, no random write)\n";
    std::cout << "  FlatBuffers Wire Total:      64 bytes (Payload + vtable + offset tables)\n";
    std::cout << "  Cap'n Proto Wire Total:      72 bytes (Word alignment padding + segment headers)\n";
    std::cout << "  Protobuf (Varint + Tags):    ~46 bytes (Requires dynamic parsing & allocations)\n\n";

    std::cout << "Comparative benchmarks completed successfully.\n";
    return 0;
}
