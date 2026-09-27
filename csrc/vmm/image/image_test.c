/*
 * Tests for vmm/image/image.c. New; image/root.zig has no tests of its own.
 */
#include "vmm/image/image.h"

#include "test_runner.h"
#include "utils/file.h"
#include "vmm/arch/x86/layout.h"

#include <stdlib.h>
#include <string.h>

#define RAM_SIZE (2 << 20)

static uint8_t ram[RAM_SIZE];

static void test_raw_binary_falls_back_to_default_address(void)
{
    uint8_t *binary = NULL;
    size_t binary_size = 0;
    struct zv_guest_memory *memory = NULL;
    struct zv_vm_config config = zv_vm_config_default();
    struct zv_image image;

    ZV_EXPECT_EQ(0, zv_read_file("test_bins/64bit_guest.bin", &binary, &binary_size));
    ZV_EXPECT_EQ(0, zv_guest_memory_new(&memory));
    ZV_EXPECT_EQ(0, zv_guest_memory_add(memory, 0, ram, RAM_SIZE, false));

    config.ram_size = RAM_SIZE;

    ZV_EXPECT_EQ(0, zv_image_parse(binary, binary_size, memory, &config, &image));
    ZV_EXPECT_EQ(ZV_X86_DEFAULT_LOAD_ADDRESS, image.entry_point);
    ZV_EXPECT_EQ(ZV_X86_DEFAULT_LOAD_ADDRESS, image.load_address);
    ZV_EXPECT(memcmp(ram + ZV_X86_DEFAULT_LOAD_ADDRESS, binary, binary_size) == 0);

    zv_guest_memory_deinit(memory);
    free(binary);
}

static const struct zv_test tests[] = {
    {"raw binary falls back to the default address", test_raw_binary_falls_back_to_default_address},
};

const struct zv_test_suite zv_vmm_image_suite = {"vmm.image", tests, ZV_ARRAY_LEN(tests)};
