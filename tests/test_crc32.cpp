#include "test_harness.hpp"
#include <sinew/crc32.hpp>
#include <cstring>

TEST_CASE(TestCrc32StandardVector) {
    const char* test_str = "123456789";
    uint32_t result = sinew::crc32(test_str, std::strlen(test_str));
    REQUIRE_EQ(result, 0xCBF43926u);
}

TEST_CASE(TestCrc32Empty) {
    uint32_t result = sinew::crc32(nullptr, 0);
    REQUIRE_EQ(result, 0u);
}

TEST_CASE(TestCrc32DifferentData) {
    uint32_t a = 12345;
    uint32_t b = 12346;
    uint32_t crc_a = sinew::crc32(&a, sizeof(a));
    uint32_t crc_b = sinew::crc32(&b, sizeof(b));
    REQUIRE_NE(crc_a, crc_b);
}
