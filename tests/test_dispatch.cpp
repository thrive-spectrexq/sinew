#include "test_harness.hpp"
#include <sinew/sinew.hpp>

struct PingMsg {
    uint32_t seq;
};
SINEW_REGISTER_MESSAGE(PingMsg, 1, 1);

struct PongMsg {
    uint32_t seq;
    uint64_t reply_time_ns;
};
SINEW_REGISTER_MESSAGE(PongMsg, 2, 1);

struct StatusMsg {
    uint32_t code;
};
SINEW_REGISTER_MESSAGE(StatusMsg, 3, 1);

TEST_CASE(TestInspectPacket) {
    alignas(16) uint8_t buffer[sinew::message_size<PingMsg>()];
    PingMsg* ping = sinew::prepare<PingMsg>(buffer);
    REQUIRE(ping != nullptr);
    ping->seq = 42;
    size_t sz = sinew::finalize(ping);

    sinew::PacketView pv;
    REQUIRE_EQ(sinew::inspect_packet(buffer, sz, pv), sinew::ErrorCode::Ok);
    REQUIRE(pv.is_valid());
    REQUIRE_EQ(pv.header.msg_id, 1);
    REQUIRE_EQ(pv.payload_len, sizeof(PingMsg));

    // Test truncated buffer
    sinew::PacketView pv_bad;
    REQUIRE_EQ(sinew::inspect_packet(buffer, sz - 1, pv_bad), sinew::ErrorCode::BufferTooSmall);

    // Test integer overflow craft: huge payload_len that wraps around size
    sinew::Header overflow_hdr{};
    overflow_hdr.magic = SINEW_MAGIC;
    overflow_hdr.payload_len = UINT32_MAX - sizeof(sinew::Header) + 2;
    REQUIRE_EQ(sinew::inspect_packet(&overflow_hdr, sizeof(sinew::Header) + 16, pv_bad),
               sinew::ErrorCode::BufferTooSmall);

    // Test bad magic
    buffer[0] = 0x00;
    REQUIRE_EQ(sinew::inspect_packet(buffer, sz, pv_bad), sinew::ErrorCode::BadMagic);
}

TEST_CASE(TestHeterogeneousDispatch) {
    alignas(16) uint8_t ping_buf[sinew::message_size<PingMsg>()];
    PingMsg* ping = sinew::prepare<PingMsg>(ping_buf);
    ping->seq = 101;
    size_t ping_sz = sinew::finalize(ping, true);

    alignas(16) uint8_t pong_buf[sinew::message_size<PongMsg>()];
    PongMsg* pong = sinew::prepare<PongMsg>(pong_buf);
    pong->seq = 101;
    pong->reply_time_ns = 987654321ULL;
    size_t pong_sz = sinew::finalize(pong, true);

    alignas(16) uint8_t status_buf[sinew::message_size<StatusMsg>()];
    StatusMsg* status = sinew::prepare<StatusMsg>(status_buf);
    status->code = 200;
    size_t status_sz = sinew::finalize(status, true);

    int ping_count = 0;
    int pong_count = 0;

    auto visitor = [&](auto&& msg) {
        using T = std::decay_t<decltype(msg)>;
        if constexpr (std::is_same_v<T, PingMsg>) {
            ping_count++;
            REQUIRE_EQ(msg.seq, 101u);
        } else if constexpr (std::is_same_v<T, PongMsg>) {
            pong_count++;
            REQUIRE_EQ(msg.seq, 101u);
            REQUIRE_EQ(msg.reply_time_ns, 987654321ULL);
        }
    };

    // Dispatch PingMsg
    bool res1 = sinew::dispatch<PingMsg, PongMsg>(ping_buf, ping_sz, visitor, true);
    REQUIRE(res1);
    REQUIRE_EQ(ping_count, 1);

    // Dispatch PongMsg
    bool res2 = sinew::dispatch<PingMsg, PongMsg>(pong_buf, pong_sz, visitor, true);
    REQUIRE(res2);
    REQUIRE_EQ(pong_count, 1);

    // Dispatch unhandled StatusMsg should return false
    bool res3 = sinew::dispatch<PingMsg, PongMsg>(status_buf, status_sz, visitor, true);
    REQUIRE(!res3);

    // Test dispatch from PacketView
    sinew::PacketView pv;
    REQUIRE_EQ(sinew::inspect_packet(ping_buf, ping_sz, pv), sinew::ErrorCode::Ok);
    bool res_pv = sinew::dispatch<PingMsg, PongMsg>(pv, visitor, true);
    REQUIRE(res_pv);
    REQUIRE_EQ(ping_count, 2);
}

