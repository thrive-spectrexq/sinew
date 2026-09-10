#pragma once

#include <sinew/types.hpp>
#include <sinew/crc32.hpp>
#include <sinew/endian.hpp>
#include <sinew/message.hpp>
#include <sinew/wire.hpp>
#include <sinew/ring_buffer.hpp>
#include <sinew/containers.hpp>
#include <sinew/dispatch.hpp>
#include <sinew/shm.hpp>
#include <sinew/ipc_ring_buffer.hpp>
#include <sinew/ipc_envelope.hpp>
#include <sinew/portable.hpp>
#include <sinew/ros2_bridge.hpp>

// High-level macro for defining standard Sinew messages with auto-generated unique compile-time hash ID
#define SINEW_MESSAGE(Name, ...) \
    struct Name { \
        __VA_ARGS__ \
    }; \
    SINEW_REGISTER_MESSAGE(Name, ::sinew::fnv1a16(#Name), 1) \
    static_assert(::sinew::is_valid_message_v<Name>, #Name " must be trivially copyable and standard layout");

// Define a Sinew message with an explicit Message ID
#define SINEW_MESSAGE_ID(Name, MsgId, ...) \
    struct Name { \
        __VA_ARGS__ \
    }; \
    SINEW_REGISTER_MESSAGE(Name, MsgId, 1) \
    static_assert(::sinew::is_valid_message_v<Name>, #Name " must be trivially copyable and standard layout");

// Define a Sinew message with an explicit Message ID and schema version
#define SINEW_MESSAGE_VERSIONED(Name, MsgId, MsgVersion, ...) \
    struct Name { \
        __VA_ARGS__ \
    }; \
    SINEW_REGISTER_MESSAGE(Name, MsgId, MsgVersion) \
    static_assert(::sinew::is_valid_message_v<Name>, #Name " must be trivially copyable and standard layout");

