/*
 * Tests for vmm/image/bzimage.c. "test Linux kernel" is ported from bzimage.zig; the others are
 * new.
 */
#include "vmm/image/bzimage.h"

#include "test_runner.h"
#include "test_utils/mmap.h"
#include "test_utils/test_utils.h"
#include "utils/file.h"
#include "vmm/arch/x86/layout.h"

#include <asm/bootparam.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define GIB ((size_t)1 << 30)

static void test_linux_kernel(void)
{
    zv_mmap_tracker_start();
    struct zv_fd_leak_snapshot fds = zv_fd_leak_snapshot();

    uint8_t *binary = NULL;
    size_t binary_size = 0;

    ZV_EXPECT_EQ(0, zv_read_file("test_bins/bzImage", &binary, &binary_size));

    struct zv_guest_memory *memory = NULL;
    void *ram = NULL;

    ZV_EXPECT_EQ(0, zv_guest_memory_new(&memory));
    ZV_EXPECT_EQ(
        0, zv_mmap(NULL, GIB, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0, &ram));
    ZV_EXPECT_EQ(0, zv_guest_memory_add(memory, 0x0, ram, GIB, true));

    /* As in the Zig test: 2 GiB configured, 1 GiB mapped, no binary in the config. */
    struct zv_vm_config config = zv_vm_config_default();

    config.ram_size = 2 * GIB;

    struct zv_image image;

    ZV_EXPECT_EQ(0, zv_bzimage_parse(binary, binary_size, memory, &config, &image));
    ZV_EXPECT_EQ(ZV_X86_HIGH_RAM_BEGIN + 0x200, image.entry_point);
    ZV_EXPECT_EQ(ZV_X86_HIGH_RAM_BEGIN, image.load_address);

    /* The boot parameters landed where the vCPU's RSI will point. */
    struct boot_params params;
    uint8_t *params_bytes = NULL;

    ZV_EXPECT_EQ(
        0, zv_guest_memory_as_slice(memory, ZV_X86_BOOT_PARAM_ADDR, sizeof(params), &params_bytes));
    memcpy(&params, params_bytes, sizeof(params));
    ZV_EXPECT_EQ(0x53726448, params.hdr.header);
    ZV_EXPECT_EQ(ZV_X86_BOOT_CMDLINE_ADDR, params.hdr.cmd_line_ptr);
    ZV_EXPECT_EQ(0xff, params.hdr.type_of_loader);
    ZV_EXPECT_EQ(ZV_X86_BOOT_ACPI_ADDR, params.acpi_rsdp_addr);
    ZV_EXPECT_EQ(ZV_X86_MEMORY_SLOT_COUNT, params.e820_entries);
    ZV_EXPECT_EQ(0, params.hdr.ramdisk_size);

    zv_guest_memory_deinit(memory);
    free(binary);

    ZV_EXPECT_EQ(0, zv_mmap_tracker_stop());
    ZV_EXPECT(!zv_fd_leak_check(&fds));
}

static void test_rejects_non_bzimage(void)
{
    uint8_t *binary = NULL;
    size_t binary_size = 0;
    struct zv_vm_config config = zv_vm_config_default();
    struct zv_image image;

    config.ram_size = GIB;

    /* Nothing is written to guest memory before the header checks, so no memory is needed. */
    struct zv_guest_memory *memory = NULL;

    ZV_EXPECT_EQ(0, zv_guest_memory_new(&memory));

    ZV_EXPECT_EQ(0, zv_read_file("test_bins/64bit_guest.bin", &binary, &binary_size));
    ZV_EXPECT_EQ(-ENOEXEC, zv_bzimage_parse(binary, binary_size, memory, &config, &image));
    free(binary);

    /* A valid header whose kernel lies past the end of the data (Zig would panic here). */
    ZV_EXPECT_EQ(0, zv_read_file("test_bins/bzImage", &binary, &binary_size));
    ZV_EXPECT_EQ(-ENOEXEC, zv_bzimage_parse(binary, 1024, memory, &config, &image));
    free(binary);

    zv_guest_memory_deinit(memory);
}

static void test_rejects_initramfs_bigger_than_ram(void)
{
    uint8_t *binary = NULL;
    size_t binary_size = 0;
    static const uint8_t initramfs[1] = {0};
    struct zv_vm_config config = zv_vm_config_default();
    struct zv_image image;
    struct zv_guest_memory *memory = NULL;

    ZV_EXPECT_EQ(0, zv_guest_memory_new(&memory));
    ZV_EXPECT_EQ(0, zv_read_file("test_bins/bzImage", &binary, &binary_size));

    /* Claims to be larger than everything up to the end of high RAM (Zig would panic here). */
    config.ram_size = GIB;
    config.initramfs = initramfs;
    config.initramfs_size = ZV_X86_HIGH_RAM_BEGIN + GIB + 1;

    ZV_EXPECT_EQ(-EFBIG, zv_bzimage_parse(binary, binary_size, memory, &config, &image));

    free(binary);
    zv_guest_memory_deinit(memory);
}

static const struct zv_test tests[] = {
    {"test Linux kernel", test_linux_kernel},
    {"rejects non-bzImage data", test_rejects_non_bzimage},
    {"rejects initramfs bigger than RAM", test_rejects_initramfs_bigger_than_ram},
};

const struct zv_test_suite zv_vmm_bzimage_suite = {"vmm.image.bzimage", tests, ZV_ARRAY_LEN(tests)};
