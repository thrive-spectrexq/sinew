#include "test_harness.hpp"
#include <sinew/sinew.hpp>
#include <thread>
#include <chrono>

struct Heartbeat {
    uint64_t uptime_ms;
    uint32_t status_flags;
};
SINEW_REGISTER_MESSAGE(Heartbeat, 10, 1);

struct JointCommand {
    uint32_t joint_id;
    float target_pos;
    float max_velocity;
};
SINEW_REGISTER_MESSAGE(JointCommand, 20, 1);

struct DiagnosticAlert {
    uint32_t severity;
    sinew::StaticString<32> message;
};
SINEW_REGISTER_MESSAGE(DiagnosticAlert, 30, 1);

TEST_CASE(TestIpcEnvelopeBasic) {
    const std::string shm_name = "sinew_test_ipc_env_basic";
    constexpr size_t kMaxPayload = 128;
    constexpr size_t kCapacity = 32;

    auto bus_producer = sinew::IpcEnvelopeRingBuffer<kMaxPayload, kCapacity>::create(shm_name);
    REQUIRE(bus_producer.is_valid());
    REQUIRE(bus_producer.empty());

    auto bus_consumer = sinew::IpcEnvelopeRingBuffer<kMaxPayload, kCapacity>::open(shm_name);
    REQUIRE(bus_consumer.is_valid());
    REQUIRE(bus_consumer.empty());

    // 1. Send Heartbeat
    Heartbeat hb{5000, 0x1};
    REQUIRE(bus_producer.push(hb));

    // 2. Send JointCommand
    JointCommand jcmd{2, 1.57f, 3.14f};
    REQUIRE(bus_producer.push(jcmd));

    // 3. Send DiagnosticAlert
    DiagnosticAlert alert{2, "motor_overtemp_warning"};
    REQUIRE(bus_producer.push(alert));

    REQUIRE_EQ(bus_consumer.size(), 3u);

    // Verify first packet via peek_packet
    sinew::PacketView pv = bus_consumer.peek_packet();
    REQUIRE(pv.is_valid());
    REQUIRE_EQ(pv.header.msg_id, 10);
    REQUIRE_EQ(pv.payload_len, sizeof(Heartbeat));

    // Dispatch messages
    int hb_count = 0;
    int jcmd_count = 0;
    int alert_count = 0;

    auto visitor = [&](auto&& msg) {
        using T = std::decay_t<decltype(msg)>;
        if constexpr (std::is_same_v<T, Heartbeat>) {
            hb_count++;
            REQUIRE_EQ(msg.uptime_ms, 5000u);
        } else if constexpr (std::is_same_v<T, JointCommand>) {
            jcmd_count++;
            REQUIRE_EQ(msg.joint_id, 2u);
            REQUIRE_EQ(msg.target_pos, 1.57f);
        } else if constexpr (std::is_same_v<T, DiagnosticAlert>) {
            alert_count++;
            REQUIRE_EQ(msg.severity, 2u);
            REQUIRE_EQ(msg.message.view(), "motor_overtemp_warning");
        }
    };

    // Dispatch 1st message (Heartbeat)
    REQUIRE((bus_consumer.dispatch_and_commit<Heartbeat, JointCommand, DiagnosticAlert>(visitor)));
    REQUIRE_EQ(hb_count, 1);
    REQUIRE_EQ(bus_consumer.size(), 2u);

    // Dispatch 2nd message (JointCommand)
    REQUIRE((bus_consumer.dispatch_and_commit<Heartbeat, JointCommand, DiagnosticAlert>(visitor)));
    REQUIRE_EQ(jcmd_count, 1);
    REQUIRE_EQ(bus_consumer.size(), 1u);

    // Dispatch 3rd message (DiagnosticAlert)
    REQUIRE((bus_consumer.dispatch_and_commit<Heartbeat, JointCommand, DiagnosticAlert>(visitor)));
    REQUIRE_EQ(alert_count, 1);
    REQUIRE(bus_consumer.empty());
}

TEST_CASE(TestIpcEnvelopeConcurrentMixedStream) {
    const std::string shm_name = "sinew_test_ipc_env_concur";
    constexpr size_t kMaxPayload = 128;
    constexpr size_t kCapacity = 512;
    constexpr size_t kRounds = 5000;

    auto producer = sinew::IpcEnvelopeRingBuffer<kMaxPayload, kCapacity>::create(shm_name);
    REQUIRE(producer.is_valid());

    std::thread consumer_thread([&]() {
        auto consumer = sinew::IpcEnvelopeRingBuffer<kMaxPayload, kCapacity>::open(shm_name);
        REQUIRE(consumer.is_valid());

        size_t total_received = 0;
        size_t heartbeats = 0;
        size_t commands = 0;

        auto visitor = [&](auto&& msg) {
            using T = std::decay_t<decltype(msg)>;
            if constexpr (std::is_same_v<T, Heartbeat>) {
                heartbeats++;
            } else if constexpr (std::is_same_v<T, JointCommand>) {
                commands++;
            }
        };

        while (total_received < kRounds * 2) {
            if (consumer.dispatch_and_commit<Heartbeat, JointCommand>(visitor)) {
                total_received++;
            } else {
                std::this_thread::yield();
            }
        }

        REQUIRE_EQ(heartbeats, kRounds);
        REQUIRE_EQ(commands, kRounds);
    });

    for (size_t i = 0; i < kRounds; ++i) {
        // Send Heartbeat
        Heartbeat hb{i, 0};
        while (!producer.push(hb)) {
            std::this_thread::yield();
        }

        // Send JointCommand
        JointCommand cmd{static_cast<uint32_t>(i), 1.0f, 2.0f};
        while (!producer.push(cmd)) {
            std::this_thread::yield();
        }
    }

    consumer_thread.join();
}
