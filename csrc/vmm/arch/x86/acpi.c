/*
 * x86 ACPI setup. Port of vmm/arch/x86/acpi.zig.
 *
 * The Zig version places the tables with a bump allocator over the ACPI area. The offsets are
 * computed explicitly here and match it: RSDP at 0, then the MADT and the XSDT, each at the next
 * 16-byte boundary.
 */
#include "vmm/arch/x86/acpi.h"

#include "vmm/acpi/madt.h"
#include "vmm/acpi/rsdt.h"
#include "vmm/acpi/xsdt.h"
#include "vmm/arch/x86/layout.h"

#include <errno.h>
#include <string.h>

#define TABLE_ALIGNMENT 16

static size_t align_up(size_t value, size_t alignment)
{
    return (value + alignment - 1) / alignment * alignment;
}

int zv_x86_acpi_setup_tables(struct zv_guest_memory *memory, size_t vcpu_count)
{
    uint8_t *area = NULL;
    int err = zv_guest_memory_as_slice(memory, ZV_X86_BOOT_ACPI_ADDR, ZV_X86_BOOT_ACPI_SIZE, &area);

    if (err < 0)
        return err;

    /* The RSDP must come first: the boot parameters point at the start of the ACPI area. */
    size_t rsdp_offset = 0;
    size_t madt_offset = align_up(rsdp_offset + sizeof(struct zv_acpi_rsdp), TABLE_ALIGNMENT);
    size_t madt_size = zv_acpi_madt_size(vcpu_count);
    size_t xsdt_offset = align_up(madt_offset + madt_size, TABLE_ALIGNMENT);
    size_t xsdt_size = zv_acpi_xsdt_size(1);

    if (xsdt_offset + xsdt_size > ZV_X86_BOOT_ACPI_SIZE)
        return -ENOMEM;

    zv_acpi_madt_init(area + madt_offset, vcpu_count);

    uint64_t madt_address = ZV_X86_BOOT_ACPI_ADDR + madt_offset;

    zv_acpi_xsdt_init(area + xsdt_offset, &madt_address, 1);

    struct zv_acpi_rsdp rsdp;

    zv_acpi_rsdp_init(&rsdp, 0, ZV_X86_BOOT_ACPI_ADDR + xsdt_offset);
    memcpy(area + rsdp_offset, &rsdp, sizeof(rsdp));

    return 0;
}
