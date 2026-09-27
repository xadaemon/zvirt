/*
 * The MADT (Multiple APIC Description Table). Port of vmm/acpi/madt.zig.
 */
#include "vmm/acpi/madt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MADT_TYPE_LOCAL_APIC 0
#define MADT_TYPE_IO_APIC 1

#define PCAT_COMPAT (1 << 0)
#define LAPIC_ENABLED (1 << 0)

size_t zv_acpi_madt_size(size_t cpu_count)
{
    return sizeof(struct zv_acpi_madt) + cpu_count * sizeof(struct zv_acpi_madt_local_apic) +
           sizeof(struct zv_acpi_madt_io_apic);
}

void zv_acpi_madt_init(uint8_t *table, size_t cpu_count)
{
    if (cpu_count > UINT8_MAX) {
        fprintf(stderr, "madt: %zu CPUs don't fit 8-bit APIC ids\n", cpu_count);
        abort();
    }

    size_t size = zv_acpi_madt_size(cpu_count);
    size_t offset = 0;

    struct zv_acpi_madt madt;

    zv_acpi_table_header_init(&madt.header, "APIC", (uint32_t)size, 1);
    madt.address = ZV_ACPI_MADT_LOCAL_APIC_ADDRESS;
    madt.flags = PCAT_COMPAT;
    memcpy(table + offset, &madt, sizeof(madt));
    offset += sizeof(madt);

    for (size_t cpu = 0; cpu < cpu_count; cpu++) {
        struct zv_acpi_madt_local_apic local_apic;

        local_apic.header.type = MADT_TYPE_LOCAL_APIC;
        local_apic.header.length = sizeof(local_apic);
        local_apic.processor_id = (uint8_t)cpu;
        local_apic.id = (uint8_t)cpu;
        local_apic.lapic_flags = LAPIC_ENABLED;
        memcpy(table + offset, &local_apic, sizeof(local_apic));
        offset += sizeof(local_apic);
    }

    struct zv_acpi_madt_io_apic io_apic;

    io_apic.header.type = MADT_TYPE_IO_APIC;
    io_apic.header.length = sizeof(io_apic);
    io_apic.id = (uint8_t)cpu_count;
    io_apic.reserved = 0;
    io_apic.address = ZV_ACPI_MADT_IO_APIC_ADDRESS;
    io_apic.global_irq_base = 0;
    memcpy(table + offset, &io_apic, sizeof(io_apic));

    /* The checksum byte is still 0, so summing the whole table gives the right value. */
    table[offsetof(struct zv_acpi_table_header, checksum)] = zv_acpi_checksum(table, size);
}
