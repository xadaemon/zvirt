/*
 * bzImage loader. Port of vmm/image/bzimage.zig.
 *
 * Zig panics when an offset computed from the file runs past its end, or when the initramfs is
 * bigger than the space below the end of RAM. Here both are explicit checks that return an error.
 */
#include "vmm/image/bzimage.h"

#include "vmm/arch/x86/layout.h"

#include <asm/bootparam.h>
#include <errno.h>
#include <string.h>

/* The setup header layout must match the boot protocol, as the Zig comptime checks do. */
_Static_assert(sizeof(struct setup_header) == ZV_BZIMAGE_SETUP_HEADER_SIZE, "setup_header size");
_Static_assert(offsetof(struct setup_header, boot_flag) == 0x0D, "setup_header.boot_flag");
_Static_assert(offsetof(struct setup_header, header) == 0x11, "setup_header.header");
_Static_assert(offsetof(struct setup_header, version) == 0x15, "setup_header.version");
_Static_assert(offsetof(struct setup_header, kernel_info_offset) == 0x77,
               "setup_header.kernel_info_offset");

/* The command line pointer in the setup header is 32 bits. */
_Static_assert(ZV_X86_BOOT_CMDLINE_ADDR <= UINT32_MAX, "command line must be below 4 GiB");

#define BOOTFLAG 0xAA55
/* "HdrS" */
#define MAGIC 0x53726448
/* Boot protocol 2.15. */
#define SUPPORTED_VERSION 0x20f

#define MINIMAL_SIZE (ZV_BZIMAGE_SETUP_HEADER_OFFSET + ZV_BZIMAGE_SETUP_HEADER_SIZE)

#define SECTOR_SIZE 512
#define PAGE_SIZE 4096

/* The 64-bit entry point is 0x200 bytes into the protected-mode kernel. */
#define ENTRY_64_OFFSET 0x200

/* "Undefined" boot loader id. Linux needs some value set to accept the initramfs. */
#define LOADER_TYPE_UNDEFINED 0xff

static void fill_e820(struct boot_params *params, const struct zv_vm_config *config)
{
    struct zv_x86_memory_slot layout[ZV_X86_MEMORY_SLOT_COUNT];

    zv_x86_memory_layout(config, layout);

    for (int i = 0; i < ZV_X86_MEMORY_SLOT_COUNT; i++) {
        params->e820_table[i].addr = layout[i].start;
        params->e820_table[i].size = layout[i].length;
        params->e820_table[i].type = (uint32_t)layout[i].kind;
    }

    params->e820_entries = ZV_X86_MEMORY_SLOT_COUNT;
}

/* Returns 0, or -ENOEXEC / -EFBIG if the header describes something this loader can't boot. */
static int check_header(const struct setup_header *header, const struct zv_vm_config *config)
{
    if (header->header != MAGIC)
        return -ENOEXEC;

    /* The header's real size: the jump instruction at its start skips over it. */
    if ((size_t)(header->jump >> 8) + 0x11 < ZV_BZIMAGE_SETUP_HEADER_SIZE)
        return -ENOEXEC;

    if (header->version != SUPPORTED_VERSION)
        return -ENOEXEC;

    if (header->boot_flag != BOOTFLAG)
        return -ENOEXEC;

    /* For now support only bzImage (loaded high). */
    if ((header->loadflags & LOADED_HIGH) == 0)
        return -ENOEXEC;

    /* The kernel must have a 64-bit entry point. */
    if ((header->xloadflags & XLF_KERNEL_64) == 0)
        return -ENOEXEC;

    /* We want to be able to load the initramfs above 4 GiB. */
    if ((header->xloadflags & XLF_CAN_BE_LOADED_ABOVE_4G) == 0)
        return -ENOEXEC;

    /* For now keep 1 GiB for simplicity. */
    if (header->init_size > (1u << 30))
        return -EFBIG;

    if (config->binary_size > config->ram_size)
        return -EFBIG;

    return 0;
}

/*
 * Copies the initramfs to the end of high RAM, away from the kernel so decompression can't
 * overwrite it, and records where it is in the boot parameters.
 */
static int load_initramfs(struct boot_params *params, struct zv_guest_memory *memory,
                          const struct zv_vm_config *config)
{
    uint64_t high_ram_end = ZV_X86_HIGH_RAM_BEGIN + config->ram_size;
    uint64_t initramfs_size = config->initramfs_size;

    if (initramfs_size > high_ram_end)
        return -EFBIG;

    uint64_t initrd_begin = (high_ram_end - initramfs_size) & ~(uint64_t)(PAGE_SIZE - 1);
    int err =
        zv_guest_memory_write(memory, initrd_begin, config->initramfs, config->initramfs_size);

    if (err < 0)
        return err;

    params->hdr.ramdisk_image = (uint32_t)initrd_begin;
    params->ext_ramdisk_image = (uint32_t)(initrd_begin >> 32);
    params->hdr.ramdisk_size = (uint32_t)initramfs_size;
    params->ext_ramdisk_size = (uint32_t)(initramfs_size >> 32);
    return 0;
}

int zv_bzimage_parse(const uint8_t *data, size_t size, struct zv_guest_memory *memory,
                     const struct zv_vm_config *config, struct zv_image *out)
{
    if (size < MINIMAL_SIZE)
        return -ENOEXEC;

    /* Copy the header out: at offset 0x1F1 of the file buffer it isn't aligned. */
    struct setup_header header;

    memcpy(&header, data + ZV_BZIMAGE_SETUP_HEADER_OFFSET, sizeof(header));

    int err = check_header(&header, config);

    if (err < 0)
        return err;

    /* The real-mode setup code (boot sector + setup sectors) comes before the kernel proper. */
    size_t setup_sectors = header.setup_sects == 0 ? 4 : header.setup_sects;
    size_t kernel_offset = (setup_sectors + 1) * SECTOR_SIZE;

    if (kernel_offset > size)
        return -ENOEXEC;

    /* 4 KiB and packed. Members are assigned directly; their addresses are never taken. */
    struct boot_params params;

    memset(&params, 0, sizeof(params));
    params.hdr = header;

    if (config->initramfs != NULL) {
        err = load_initramfs(&params, memory, config);
        if (err < 0)
            return err;
    }

    fill_e820(&params, config);

    params.hdr.cmd_line_ptr = ZV_X86_BOOT_CMDLINE_ADDR;
    params.hdr.type_of_loader = LOADER_TYPE_UNDEFINED;
    params.acpi_rsdp_addr = ZV_X86_BOOT_ACPI_ADDR;

    err = zv_guest_memory_write(memory, ZV_X86_BOOT_PARAM_ADDR, &params, sizeof(params));
    if (err < 0)
        return err;

    err = zv_guest_memory_write(memory, ZV_X86_HIGH_RAM_BEGIN, data + kernel_offset,
                                size - kernel_offset);
    if (err < 0)
        return err;

    out->entry_point = ZV_X86_HIGH_RAM_BEGIN + ENTRY_64_OFFSET;
    out->load_address = ZV_X86_HIGH_RAM_BEGIN;
    return 0;
}
