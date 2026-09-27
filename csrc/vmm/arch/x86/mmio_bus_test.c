/*
 * Tests for vmm/arch/x86/mmio_bus.c. New; mmio_bus.zig has no tests. Phases 6 and 7 register
 * virtio-mmio devices and PCI BARs here.
 */
#include "vmm/arch/x86/mmio_bus.h"

#include "test_runner.h"

#include <errno.h>
#include <string.h>

/* A fake device that records the last access and answers reads with a fixed pattern. */
struct fake_device {
    uint64_t last_offset;
    size_t last_length;
    uint8_t last_written[8];
};

static int fake_read(void *context, uint64_t offset, uint8_t *data, size_t length)
{
    struct fake_device *device = context;

    device->last_offset = offset;
    device->last_length = length;
    for (size_t i = 0; i < length; i++)
        data[i] = (uint8_t)(0x10 + i);

    return 0;
}

static int fake_write(void *context, uint64_t offset, const uint8_t *data, size_t length)
{
    struct fake_device *device = context;

    device->last_offset = offset;
    device->last_length = length;
    memcpy(device->last_written, data, length);
    return 0;
}

static struct zv_mmio_device make_device(struct fake_device *fake)
{
    struct zv_mmio_device device = {.context = fake, .read = fake_read, .write = fake_write};

    return device;
}

static void test_registration_rejects_overlaps(void)
{
    struct zv_mmio_bus bus;
    struct fake_device fake = {0};

    zv_mmio_bus_init(&bus);
    ZV_EXPECT_EQ(0, zv_mmio_bus_register_range(&bus, 0xd0000000, 0x1000, make_device(&fake)));

    /* Overlapping at the start, at the end, and fully inside. */
    ZV_EXPECT_EQ(-EEXIST, zv_mmio_bus_register_range(&bus, 0xcffff000, 0x1001, make_device(&fake)));
    ZV_EXPECT_EQ(-EEXIST, zv_mmio_bus_register_range(&bus, 0xd0000fff, 0x10, make_device(&fake)));
    ZV_EXPECT_EQ(-EEXIST, zv_mmio_bus_register_range(&bus, 0xd0000100, 0x10, make_device(&fake)));

    /* Directly adjacent ranges are fine. */
    ZV_EXPECT_EQ(0, zv_mmio_bus_register_range(&bus, 0xd0001000, 0x1000, make_device(&fake)));
    ZV_EXPECT_EQ(0, zv_mmio_bus_register_range(&bus, 0xcffff000, 0x1000, make_device(&fake)));

    /* Empty and wrapping ranges. */
    ZV_EXPECT_EQ(-EINVAL, zv_mmio_bus_register_range(&bus, 0xe0000000, 0, make_device(&fake)));
    ZV_EXPECT_EQ(-EINVAL, zv_mmio_bus_register_range(&bus, UINT64_MAX, 2, make_device(&fake)));

    zv_mmio_bus_deinit(&bus);
}

static void test_accesses_reach_the_device(void)
{
    struct zv_mmio_bus bus;
    struct fake_device fake = {0};
    struct zv_kvm_io_result result;

    zv_mmio_bus_init(&bus);
    ZV_EXPECT_EQ(0, zv_mmio_bus_register_range(&bus, 0xd0000000, 0x1000, make_device(&fake)));

    /* A 4-byte read at the last dword of the range. */
    struct zv_kvm_mmio_exit read = {
        .physical_address = 0xd0000ffc, .data = 0, .is_write = false, .length = 4};

    ZV_EXPECT_EQ(0, zv_mmio_bus_handle_mmio(&bus, &read, &result));
    ZV_EXPECT_EQ(0xffc, fake.last_offset);
    ZV_EXPECT_EQ(4, fake.last_length);
    ZV_EXPECT_EQ(ZV_KVM_IO_RESULT_MMIO, result.kind);
    ZV_EXPECT(result.mmio_data == 0x13121110);

    /* A 2-byte write at the start. */
    struct zv_kvm_mmio_exit write = {
        .physical_address = 0xd0000000, .data = 0xbeef, .is_write = true, .length = 2};

    ZV_EXPECT_EQ(0, zv_mmio_bus_handle_mmio(&bus, &write, &result));
    ZV_EXPECT_EQ(0, fake.last_offset);
    ZV_EXPECT_EQ(2, fake.last_length);
    ZV_EXPECT_EQ(0xef, fake.last_written[0]);
    ZV_EXPECT_EQ(0xbe, fake.last_written[1]);
    ZV_EXPECT_EQ(ZV_KVM_IO_RESULT_NONE, result.kind);

    zv_mmio_bus_deinit(&bus);
}

static void test_legacy_hole_reads_as_all_ones(void)
{
    struct zv_mmio_bus bus;
    struct zv_kvm_io_result result;

    zv_mmio_bus_init(&bus);

    struct zv_kvm_mmio_exit vga = {
        .physical_address = 0xa0000, .data = 0, .is_write = false, .length = 1};

    ZV_EXPECT_EQ(0, zv_mmio_bus_handle_mmio(&bus, &vga, &result));
    ZV_EXPECT_EQ(ZV_KVM_IO_RESULT_MMIO, result.kind);
    ZV_EXPECT(result.mmio_data == UINT64_MAX);

    zv_mmio_bus_deinit(&bus);
}

static const struct zv_test tests[] = {
    {"registration rejects overlaps", test_registration_rejects_overlaps},
    {"accesses reach the device", test_accesses_reach_the_device},
    {"legacy hole reads as all ones", test_legacy_hole_reads_as_all_ones},
};

const struct zv_test_suite zv_vmm_x86_mmio_bus_suite = {"vmm.arch.x86.mmio_bus", tests,
                                                        ZV_ARRAY_LEN(tests)};
