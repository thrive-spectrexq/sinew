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

// High-level macro for defining standard Sinew messages
#define SINEW_MESSAGE(Name, ...) \
    struct Name { \
        __VA_ARGS__ \
    }; \
    SINEW_REGISTER_MESSAGE(Name, 0, 1) \
    static_assert(::sinew::is_valid_message_v<Name>, #Name " must be trivially copyable and standard layout");

#define SINEW_MESSAGE_VERSIONED(Name, MsgId, MsgVersion, ...) \
    struct Name { \
        __VA_ARGS__ \
    }; \
    SINEW_REGISTER_MESSAGE(Name, MsgId, MsgVersion) \
    static_assert(::sinew::is_valid_message_v<Name>, #Name " must be trivially copyable and standard layout");
