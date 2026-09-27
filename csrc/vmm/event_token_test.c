/*
 * Tests for vmm/event_token.c. New; the Zig packed struct needed no test.
 */
#include "vmm/event_token.h"

#include "test_runner.h"

static void expect_round_trip(uint32_t id, int fd, enum zv_event_source source)
{
    struct zv_event_token token = {.id = id, .fd = fd, .source = source};
    struct zv_event_token decoded = zv_event_token_decode(zv_event_token_encode(token));

    ZV_EXPECT_EQ(id, decoded.id);
    ZV_EXPECT_EQ(fd, decoded.fd);
    ZV_EXPECT_EQ(source, decoded.source);
}

static void test_round_trip(void)
{
    expect_round_trip(0, 0, ZV_EVENT_SOURCE_VCPU);
    expect_round_trip(ZV_EVENT_TOKEN_MAX_ID, 0x7fffffff, ZV_EVENT_SOURCE_PCI);
    expect_round_trip(15, 3, ZV_EVENT_SOURCE_IO_BUS);
    expect_round_trip(1, 1024, ZV_EVENT_SOURCE_VIRTIO);
}

static void test_bit_layout(void)
{
    /* Same layout as Zig's packed struct(u64): id bits 0-28, fd bits 29-60, source bits 61-63. */
    struct zv_event_token token = {.id = 1, .fd = 1, .source = ZV_EVENT_SOURCE_PCI};

    ZV_EXPECT(zv_event_token_encode(token) == (1ull | (1ull << 29) | (3ull << 61)));
}

static const struct zv_test tests[] = {
    {"round trip", test_round_trip},
    {"bit layout", test_bit_layout},
};

const struct zv_test_suite zv_vmm_event_token_suite = {"vmm.event_token", tests,
                                                       ZV_ARRAY_LEN(tests)};
