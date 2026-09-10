#pragma once

#include <sinew/types.hpp>
#include <sinew/wire.hpp>
#include <sinew/ring_buffer.hpp>
#include <sinew/ipc_ring_buffer.hpp>
#include <sinew/containers.hpp>
#include <cstdint>
#include <cstddef>
#include <string_view>
#include <functional>

#if defined(SINEW_HAS_ROS2)
#include <rclcpp/rclcpp.hpp>
#endif

namespace sinew {
namespace ros2 {

/**
 * @brief Standardized POD representations of common ROS 2 message types
 * used for zero-copy bridging and conversion without requiring ROS 2 headers.
 */

struct Vector3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

struct Quaternion {
    double x{0.0};
    double y{0.0};
    double z{0.0};
    double w{1.0};
};

struct Header {
    uint32_t sec{0};
    uint32_t nanosec{0};
    StaticString<32> frame_id{};
};

struct ImuMessage {
    Header header;
    Quaternion orientation;
    double orientation_covariance[9]{};
    Vector3 angular_velocity;
    double angular_velocity_covariance[9]{};
    Vector3 linear_acceleration;
    double linear_acceleration_covariance[9]{};
};

struct TwistMessage {
    Vector3 linear;
    Vector3 angular;
};

struct JointStateMessage {
    Header header;
    StaticVector<StaticString<16>, 8> name;
    StaticVector<double, 8> position;
    StaticVector<double, 8> velocity;
    StaticVector<double, 8> effort;
};

/**
 * @brief Conversion trait for transforming between native Sinew telemetry messages
 * and ROS 2 standard messages. Specialize for your custom types.
 */
template <typename SinewMsg, typename RosMsg>
struct Ros2Converter {
    // Default fallback uses explicit assignment if types match
    static void to_ros(const SinewMsg& in, RosMsg& out) {
        out = in;
    }
    static void from_ros(const RosMsg& in, SinewMsg& out) {
        out = in;
    }
};

/**
 * @brief Zero-Allocation Bridge Worker.
 *
 * Reads from a high-frequency Sinew SPSC / IPC ring buffer and translates
 * messages to ROS 2 topics via a callback or ROS 2 publisher.
 */
template <typename SinewMsg, typename RosMsg, typename RingBufferType>
class BridgePublisher {
public:
    using Callback = std::function<void(const RosMsg&)>;

    explicit BridgePublisher(RingBufferType& ring_buffer, Callback publish_cb)
        : ring_buffer_(ring_buffer), publish_cb_(std::move(publish_cb)) {}

    /**
     * @brief Process up to max_messages from the Sinew ring buffer and publish them to ROS.
     * @return Number of messages bridged.
     */
    size_t spin_some(size_t max_messages = 64) {
        size_t processed = 0;
        RosMsg ros_msg{};

        while (processed < max_messages) {
            const SinewMsg* in = ring_buffer_.peek_read();
            if (in == nullptr) {
                break;
            }

            Ros2Converter<SinewMsg, RosMsg>::to_ros(*in, ros_msg);
            if (publish_cb_) {
                publish_cb_(ros_msg);
            }

            ring_buffer_.commit_read();
            processed++;
        }

        return processed;
    }

private:
    RingBufferType& ring_buffer_;
    Callback publish_cb_;
};

/**
 * @brief Zero-Allocation Bridge Ingestion.
 *
 * Receives messages from a ROS 2 subscription callback and pushes them directly
 * into a Sinew high-frequency IPC / SPSC ring buffer with zero heap allocation.
 */
template <typename SinewMsg, typename RosMsg, typename RingBufferType>
class BridgeSubscriber {
public:
    explicit BridgeSubscriber(RingBufferType& ring_buffer)
        : ring_buffer_(ring_buffer) {}

    /**
     * @brief Receive a ROS message and forward directly to the Sinew ring buffer.
     * @return true if successfully pushed; false if ring buffer is full.
     */
    bool handle_ros_message(const RosMsg& ros_msg, bool compute_crc = false) {
        SinewMsg* slot = ring_buffer_.prepare_write();
        if (slot == nullptr) {
            return false; // Ring buffer full (backpressure)
        }

        Ros2Converter<SinewMsg, RosMsg>::from_ros(ros_msg, *slot);
        ring_buffer_.commit_write(compute_crc);
        return true;
    }

private:
    RingBufferType& ring_buffer_;
};

} // namespace ros2
} // namespace sinew
