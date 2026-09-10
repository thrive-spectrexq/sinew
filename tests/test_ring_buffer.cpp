#include "test_harness.hpp"
#include <sinew/sinew.hpp>
#include <thread>
#include <atomic>
#include <memory>

struct RingMsg {
    uint64_t seq;
    uint32_t val;
};
SINEW_REGISTER_MESSAGE(RingMsg, 10, 1);

TEST_CASE(TestRingBufferBasic) {
    sinew::SpscRingBuffer<RingMsg, 4> rb;

    REQUIRE_EQ(rb.capacity(), 4);
    REQUIRE(rb.empty());
    REQUIRE(!rb.full());

    // Write slot 1
    RingMsg* w1 = rb.prepare_write();
    REQUIRE(w1 != nullptr);
    w1->seq = 1;
    w1->val = 100;
    rb.commit_write();

    // Write slot 2
    RingMsg* w2 = rb.prepare_write();
    REQUIRE(w2 != nullptr);
    w2->seq = 2;
    w2->val = 200;
    rb.commit_write();

    REQUIRE(!rb.empty());

    // Read slot 1
    const RingMsg* r1 = rb.peek_read();
    REQUIRE(r1 != nullptr);
    REQUIRE_EQ(r1->seq, 1);
    REQUIRE_EQ(r1->val, 100);
    rb.commit_read();

    // Read slot 2
    const RingMsg* r2 = rb.peek_read();
    REQUIRE(r2 != nullptr);
    REQUIRE_EQ(r2->seq, 2);
    REQUIRE_EQ(r2->val, 200);
    rb.commit_read();

    REQUIRE(rb.empty());
}

TEST_CASE(TestRingBufferMultiThreadedSPSC) {
    constexpr size_t kTotalMessages = 50000;
    auto rb = std::make_unique<sinew::SpscRingBuffer<RingMsg, 1024>>();
    std::atomic<bool> producer_done{false};

    // Consumer thread
    std::thread consumer([&]() {
        uint64_t expected_seq = 0;
        size_t received_count = 0;

        while (received_count < kTotalMessages) {
            const RingMsg* msg = rb->peek_read();
            if (msg != nullptr) {
                REQUIRE_EQ(msg->seq, expected_seq);
                REQUIRE_EQ(msg->val, static_cast<uint32_t>(expected_seq * 10));
                expected_seq++;
                received_count++;
                rb->commit_read();
            } else {
                std::this_thread::yield();
            }
        }
    });

    // Producer thread
    std::thread producer([&]() {
        for (uint64_t i = 0; i < kTotalMessages; ++i) {
            RingMsg* slot = nullptr;
            while ((slot = rb->prepare_write()) == nullptr) {
                std::this_thread::yield();
            }
            slot->seq = i;
            slot->val = static_cast<uint32_t>(i * 10);
            rb->commit_write();
        }
        producer_done.store(true);
    });

    producer.join();
    consumer.join();
}

TEST_CASE(TestRingBufferConsumeCallback) {
    sinew::SpscRingBuffer<RingMsg, 4> rb;

    RingMsg msg1{10, 100};
    RingMsg msg2{20, 200};
    REQUIRE(rb.push(msg1));
    REQUIRE(rb.push(msg2));

    uint64_t processed_seq = 0;
    bool consumed = rb.consume([&](const RingMsg& msg) {
        processed_seq = msg.seq;
        REQUIRE_EQ(msg.val, 100u);
    });
    REQUIRE(consumed);
    REQUIRE_EQ(processed_seq, 10u);

    // Second message
    consumed = rb.consume([&](const RingMsg& msg) {
        processed_seq = msg.seq;
        REQUIRE_EQ(msg.val, 200u);
    });
    REQUIRE(consumed);
    REQUIRE_EQ(processed_seq, 20u);

    // Third consume on empty buffer should return false
    consumed = rb.consume([&](const RingMsg&) {});
    REQUIRE(!consumed);
    REQUIRE(rb.empty());
}

