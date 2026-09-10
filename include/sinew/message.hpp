#pragma once

#include <type_traits>
#include <cstdint>
#include <cstddef>
#include <string_view>

namespace sinew {

// Primary template for MessageTraits. Specialize for your message types.
template <typename T, typename = void>
struct MessageTraits {
    static constexpr uint16_t id = 0;
    static constexpr uint16_t version = 1;
    static constexpr std::string_view name = "UnknownMessage";
};

template <typename T>
constexpr void check_message_type_constraints() noexcept {
    static_assert(std::is_trivially_copyable_v<T>, 
                  "Sinew message types must be trivially copyable (zero-copy wire safety)");
    static_assert(std::is_standard_layout_v<T>, 
                  "Sinew message types must be standard layout (deterministic memory offsets)");
    static_assert(!std::is_pointer_v<T>, 
                  "Sinew message types cannot be pointers");
}

template <typename T>
inline constexpr bool is_valid_message_v = 
    std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T> && !std::is_pointer_v<T>;

#if __cplusplus >= 202002L
template <typename T>
concept Message = is_valid_message_v<T>;
#endif

} // namespace sinew

// Macro to specialize message traits for an existing struct
#define SINEW_REGISTER_MESSAGE(Type, MsgId, MsgVersion) \
    namespace sinew { \
        template <> \
        struct MessageTraits<Type> { \
            static constexpr uint16_t id = static_cast<uint16_t>(MsgId); \
            static constexpr uint16_t version = static_cast<uint16_t>(MsgVersion); \
            static constexpr std::string_view name = #Type; \
        }; \
    }

// Macro helpers for defining messages
#define SINEW_FIELD(type, name) type name;

#define SINEW_MESSAGE_STRUCT(Name, ...) \
    struct Name { \
        __VA_ARGS__ \
    }; \
    SINEW_REGISTER_MESSAGE(Name, 0, 1)

