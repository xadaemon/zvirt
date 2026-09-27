/*
 * Guest physical-address layout used during x86 boot. Port of vmm/arch/x86/layout.zig.
 *
 * arch/root.zig only picks the x86 implementation (and rejects other architectures), so it has no
 * C file; that check lives here instead, and code includes the x86 headers directly. Everything
 * under arch/x86/ uses the zv_x86_ / ZV_X86_ prefix.
 */
#ifndef ZV_VMM_ARCH_X86_LAYOUT_H
#define ZV_VMM_ARCH_X86_LAYOUT_H

#if !defined(__x86_64__)
#error "zvirt only supports x86_64"
#endif

#include "vmm/vm_config.h"

#include <stdint.h>

/* Boot structures, all in low RAM. */
#define ZV_X86_PGD_ADDR 0x1000
#define ZV_X86_PUD_ADDR 0x2000
#define ZV_X86_PMD_ADDR 0x3000
#define ZV_X86_GDT_ADDR 0x4000
#define ZV_X86_BOOT_PARAM_ADDR 0x5000
#define ZV_X86_BOOT_CMDLINE_ADDR 0x6000
#define ZV_X86_BOOT_ACPI_ADDR 0x9e000
#define ZV_X86_BOOT_ACPI_SIZE 0x2000

/* Where a non-bzImage binary is loaded and started. */
#define ZV_X86_DEFAULT_LOAD_ADDRESS 0x100000

#define ZV_X86_LOW_RAM_BEGIN 0x0
#define ZV_X86_HIGH_RAM_BEGIN 0x00100000

/* Region kinds, with the values the E820 memory map uses. */
enum zv_x86_memory_kind {
    ZV_X86_MEMORY_RAM = 1,
    ZV_X86_MEMORY_RESERVED = 2,
    ZV_X86_MEMORY_ACPI = 3,
};

struct zv_x86_memory_slot {
    uint64_t start;
    uint64_t length;
    enum zv_x86_memory_kind kind;
};

#define ZV_X86_MEMORY_SLOT_COUNT 5

/*
 * The guest's memory map: low RAM, the ACPI tables, the legacy VGA/BIOS hole, high RAM
 * (`config->ram_size` bytes from 1 MiB), and a reserved window after it for device MMIO.
 */
void zv_x86_memory_layout(const struct zv_vm_config *config,
                          struct zv_x86_memory_slot out[ZV_X86_MEMORY_SLOT_COUNT]);

/* Address of virtio-mmio device number `index`, one 4 KiB page each, right after high RAM. */
uint64_t zv_x86_virtio_device_address(const struct zv_vm_config *config, uint64_t index);

/* The reserved window after high RAM, used for PCI BARs. */
struct zv_x86_memory_slot zv_x86_pci_range(const struct zv_vm_config *config);

#endif
