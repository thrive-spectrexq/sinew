#include "test_harness.hpp"
#include <sinew/sinew.hpp>
#include <thread>
#include <chrono>

struct MotorCommand {
    uint32_t motor_id;
    float target_velocity;
    float torque_limit;
};
SINEW_REGISTER_MESSAGE(MotorCommand, 77, 1);

TEST_CASE(TestSharedMemoryRegionLifecycle) {
    const std::string shm_name = "sinew_test_shm_lifecycle";
    constexpr size_t shm_size = 4096;

    {
        sinew::SharedMemoryRegion creator = sinew::SharedMemoryRegion::create(shm_name, shm_size);
        REQUIRE(creator.is_valid());
        REQUIRE(creator.data() != nullptr);
        REQUIRE_EQ(creator.size(), shm_size);

        // Write a test pattern
        auto* ptr = static_cast<uint32_t*>(creator.data());
        ptr[0] = 0xDEADBEEF;

        // Open in reader
        sinew::SharedMemoryRegion reader = sinew::SharedMemoryRegion::open(shm_name, shm_size);
        REQUIRE(reader.is_valid());
        auto* r_ptr = static_cast<const uint32_t*>(reader.data());
        REQUIRE_EQ(r_ptr[0], 0xDEADBEEFu);
    }
}

TEST_CASE(TestIpcRingBufferBasic) {
    const std::string shm_name = "sinew_test_ipc_basic";

    auto producer = sinew::IpcRingBuffer<MotorCommand, 16>::create(shm_name);
    REQUIRE(producer.is_valid());
    REQUIRE(producer.empty());
    REQUIRE_EQ(producer.capacity(), 16u);

    auto consumer = sinew::IpcRingBuffer<MotorCommand, 16>::open(shm_name);
    REQUIRE(consumer.is_valid());
    REQUIRE(consumer.empty());

    // Push 3 commands
    MotorCommand cmd1{1, 10.5f, 2.0f};
    MotorCommand cmd2{2, -5.0f, 1.5f};
    MotorCommand cmd3{3, 0.0f, 0.5f};

    REQUIRE(producer.push(cmd1));
    REQUIRE(producer.push(cmd2));
    REQUIRE(producer.push(cmd3));

    REQUIRE_EQ(producer.size(), 3u);
    REQUIRE_EQ(consumer.size(), 3u);

    // Read from consumer
    MotorCommand out;
    REQUIRE(consumer.pop(out));
    REQUIRE_EQ(out.motor_id, 1u);
    REQUIRE_EQ(out.target_velocity, 10.5f);

    REQUIRE(consumer.pop(out));
    REQUIRE_EQ(out.motor_id, 2u);
    REQUIRE_EQ(out.target_velocity, -5.0f);

    REQUIRE(consumer.pop(out));
    REQUIRE_EQ(out.motor_id, 3u);
    REQUIRE_EQ(out.target_velocity, 0.0f);

    REQUIRE(consumer.empty());
}

TEST_CASE(TestIpcRingBufferMultiThreaded) {
    const std::string shm_name = "sinew_test_ipc_mt";
    constexpr size_t kMsgCount = 20000;

    auto producer = sinew::IpcRingBuffer<MotorCommand, 1024>::create(shm_name);
    REQUIRE(producer.is_valid());

    std::thread consumer_thread([&]() {
        auto consumer = sinew::IpcRingBuffer<MotorCommand, 1024>::open(shm_name);
        REQUIRE(consumer.is_valid());

        size_t count = 0;
        while (count < kMsgCount) {
            const MotorCommand* cmd = consumer.peek_read();
            if (cmd != nullptr) {
                REQUIRE_EQ(cmd->motor_id, static_cast<uint32_t>(count));
                count++;
                consumer.commit_read();
            } else {
                std::this_thread::yield();
            }
        }
        REQUIRE_EQ(count, kMsgCount);
    });

    for (size_t i = 0; i < kMsgCount; ++i) {
        MotorCommand* slot = nullptr;
        while ((slot = producer.prepare_write()) == nullptr) {
            std::this_thread::yield();
        }
        slot->motor_id = static_cast<uint32_t>(i);
        slot->target_velocity = static_cast<float>(i) * 0.1f;
        slot->torque_limit = 5.0f;
        producer.commit_write(/*compute_crc=*/false);
    }

    consumer_thread.join();
}
