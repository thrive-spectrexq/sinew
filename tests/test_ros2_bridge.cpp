#include "test_harness.hpp"
#include <sinew/sinew.hpp>
#include <vector>

// Define a native Sinew robot IMU message
struct RobotImu {
    uint64_t timestamp_ns;
    float ax, ay, az;
    float gx, gy, gz;

    SINEW_REFLECT(timestamp_ns, ax, ay, az, gx, gy, gz)
};
SINEW_REGISTER_MESSAGE(RobotImu, 55, 1);

// Specialize Ros2Converter for RobotImu <-> sinew::ros2::ImuMessage
namespace sinew {
namespace ros2 {

template <>
struct Ros2Converter<RobotImu, ImuMessage> {
    static void to_ros(const RobotImu& in, ImuMessage& out) {
        out.header.sec = static_cast<uint32_t>(in.timestamp_ns / 1000000000ULL);
        out.header.nanosec = static_cast<uint32_t>(in.timestamp_ns % 1000000000ULL);
        out.header.frame_id = "imu_link";

        out.linear_acceleration.x = in.ax;
        out.linear_acceleration.y = in.ay;
        out.linear_acceleration.z = in.az;

        out.angular_velocity.x = in.gx;
        out.angular_velocity.y = in.gy;
        out.angular_velocity.z = in.gz;
    }

    static void from_ros(const ImuMessage& in, RobotImu& out) {
        out.timestamp_ns = static_cast<uint64_t>(in.header.sec) * 1000000000ULL + in.header.nanosec;
        out.ax = static_cast<float>(in.linear_acceleration.x);
        out.ay = static_cast<float>(in.linear_acceleration.y);
        out.az = static_cast<float>(in.linear_acceleration.z);
        out.gx = static_cast<float>(in.angular_velocity.x);
        out.gy = static_cast<float>(in.angular_velocity.y);
        out.gz = static_cast<float>(in.angular_velocity.z);
    }
};

} // namespace ros2
} // namespace sinew

TEST_CASE(TestRos2ConverterBidirectional) {
    RobotImu orig{
        1700000001500000000ULL,
        0.1f, -0.2f, 9.8f,
        0.01f, -0.02f, 0.03f
    };

    sinew::ros2::ImuMessage ros_msg{};
    sinew::ros2::Ros2Converter<RobotImu, sinew::ros2::ImuMessage>::to_ros(orig, ros_msg);

    REQUIRE_EQ(ros_msg.header.sec, 1700000001u);
    REQUIRE_EQ(ros_msg.header.nanosec, 500000000u);
    REQUIRE_EQ(ros_msg.header.frame_id.view(), "imu_link");
    REQUIRE_EQ(ros_msg.linear_acceleration.z, 9.8f);

    // Convert back
    RobotImu back{};
    sinew::ros2::Ros2Converter<RobotImu, sinew::ros2::ImuMessage>::from_ros(ros_msg, back);
    REQUIRE_EQ(back.timestamp_ns, orig.timestamp_ns);
    REQUIRE_EQ(back.ax, orig.ax);
    REQUIRE_EQ(back.az, orig.az);
    REQUIRE_EQ(back.gz, orig.gz);
}

TEST_CASE(TestRos2BridgePublisherWorker) {
    sinew::SpscRingBuffer<RobotImu, 16> ring_buffer;

    // Push 3 sensor packets into Sinew ring buffer
    for (uint64_t i = 0; i < 3; ++i) {
        RobotImu msg{
            1000000000ULL * (i + 1),
            0.0f, 0.0f, 9.81f,
            0.0f, 0.0f, 0.0f
        };
        REQUIRE(ring_buffer.push(msg));
    }
    REQUIRE_EQ(ring_buffer.size(), 3u);

    // Set up ROS 2 bridge publisher callback
    std::vector<sinew::ros2::ImuMessage> published_messages;
    sinew::ros2::BridgePublisher<RobotImu, sinew::ros2::ImuMessage, decltype(ring_buffer)> bridge(
        ring_buffer,
        [&](const sinew::ros2::ImuMessage& msg) {
            published_messages.push_back(msg);
        }
    );

    // Spin bridge worker
    size_t bridged = bridge.spin_some(10);
    REQUIRE_EQ(bridged, 3u);
    REQUIRE(ring_buffer.empty());
    REQUIRE_EQ(published_messages.size(), 3u);
    REQUIRE_EQ(published_messages[0].header.sec, 1u);
    REQUIRE_EQ(published_messages[1].header.sec, 2u);
    REQUIRE_EQ(published_messages[2].header.sec, 3u);
}

TEST_CASE(TestRos2BridgeSubscriberWorker) {
    sinew::SpscRingBuffer<RobotImu, 4> ring_buffer;
    sinew::ros2::BridgeSubscriber<RobotImu, sinew::ros2::ImuMessage, decltype(ring_buffer)> subscriber(ring_buffer);

    sinew::ros2::ImuMessage ros_msg{};
    ros_msg.header.sec = 42;
    ros_msg.header.nanosec = 100;
    ros_msg.linear_acceleration.z = 9.81;

    // Receive message via ROS callback and push into Sinew ring buffer
    REQUIRE(subscriber.handle_ros_message(ros_msg));
    REQUIRE_EQ(ring_buffer.size(), 1u);

    // Verify message inside Sinew ring buffer
    const RobotImu* in = ring_buffer.peek_read();
    REQUIRE(in != nullptr);
    REQUIRE_EQ(in->timestamp_ns, 42000000100ULL);
    REQUIRE_EQ(in->az, 9.81f);
}
