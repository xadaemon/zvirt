/*
 * Guest physical-address layout used during x86 boot. Port of vmm/arch/x86/layout.zig.
 */
#include "vmm/arch/x86/layout.h"

#define PAGE_SIZE 4096

static uint64_t align_up_to_page(uint64_t value)
{
    return (value + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);
}

/*
 * NOTE (from the Zig version): Linux reserves the first 64k of RAM for allocations
 * (memblock_reserve(0, SZ_64K)). This memory may be used for AP CPU bootstrap, which has a limit
 * of 1 MiB. TODO: figure out why.
 */
void zv_x86_memory_layout(const struct zv_vm_config *config,
                          struct zv_x86_memory_slot out[ZV_X86_MEMORY_SLOT_COUNT])
{
    out[0] = (struct zv_x86_memory_slot){
        .start = ZV_X86_LOW_RAM_BEGIN,
        .length = 0x9e000,
        .kind = ZV_X86_MEMORY_RAM,
    };
    out[1] = (struct zv_x86_memory_slot){
        .start = ZV_X86_BOOT_ACPI_ADDR,
        .length = ZV_X86_BOOT_ACPI_SIZE,
        .kind = ZV_X86_MEMORY_ACPI,
    };
    out[2] = (struct zv_x86_memory_slot){
        .start = 0x000A0000,
        .length = 0x00060000,
        .kind = ZV_X86_MEMORY_RESERVED,
    };
    out[3] = (struct zv_x86_memory_slot){
        .start = ZV_X86_HIGH_RAM_BEGIN,
        .length = config->ram_size,
        .kind = ZV_X86_MEMORY_RAM,
    };
    out[4] = (struct zv_x86_memory_slot){
        .start = ZV_X86_HIGH_RAM_BEGIN + config->ram_size,
        .length = 0x5000,
        .kind = ZV_X86_MEMORY_RESERVED,
    };
}

uint64_t zv_x86_virtio_device_address(const struct zv_vm_config *config, uint64_t index)
{
    uint64_t ram_end = align_up_to_page(ZV_X86_HIGH_RAM_BEGIN + config->ram_size);

    return ram_end + index * PAGE_SIZE;
}

struct zv_x86_memory_slot zv_x86_pci_range(const struct zv_vm_config *config)
{
    struct zv_x86_memory_slot layout[ZV_X86_MEMORY_SLOT_COUNT];

    zv_x86_memory_layout(config, layout);
    return layout[ZV_X86_MEMORY_SLOT_COUNT - 1];
}
