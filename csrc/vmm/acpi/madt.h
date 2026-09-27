/*
 * The MADT (Multiple APIC Description Table). Port of vmm/acpi/madt.zig.
 *
 * Tells the guest which CPUs exist and where the interrupt controllers are. Layout: the fixed part
 * below, one local APIC entry per CPU, then one I/O APIC entry.
 */
#ifndef ZV_VMM_ACPI_MADT_H
#define ZV_VMM_ACPI_MADT_H

#include "vmm/acpi/acpi.h"

#include <stddef.h>
#include <stdint.h>

struct zv_acpi_madt {
    /* Signature "APIC". */
    struct zv_acpi_table_header header;
    /* Physical address of the local APIC. */
    uint32_t address;
    /* Bit 0: the system also has dual 8259 PICs. */
    uint32_t flags;
} __attribute__((packed));

struct zv_acpi_madt_local_apic {
    /* Type 0, length 8. */
    struct zv_acpi_subtable_header header;
    /* ACPI processor id. */
    uint8_t processor_id;
    /* The processor's local APIC id. */
    uint8_t id;
    /* Bit 0: enabled. */
    uint32_t lapic_flags;
} __attribute__((packed));

struct zv_acpi_madt_io_apic {
    /* Type 1, length 12. */
    struct zv_acpi_subtable_header header;
    uint8_t id;
    uint8_t reserved;
    /* Physical address of the I/O APIC. */
    uint32_t address;
    /* First global interrupt number this I/O APIC handles. */
    uint32_t global_irq_base;
} __attribute__((packed));

_Static_assert(sizeof(struct zv_acpi_madt) == 44, "MADT fixed part is 44 bytes");
_Static_assert(sizeof(struct zv_acpi_madt_local_apic) == 8, "MADT local APIC entry is 8 bytes");
_Static_assert(sizeof(struct zv_acpi_madt_io_apic) == 12, "MADT I/O APIC entry is 12 bytes");

#define ZV_ACPI_MADT_LOCAL_APIC_ADDRESS 0xfee00000
#define ZV_ACPI_MADT_IO_APIC_ADDRESS 0xfec00000

/* Size in bytes of a MADT for `cpu_count` CPUs. */
size_t zv_acpi_madt_size(size_t cpu_count);

/*
 * Writes a MADT for CPUs 0 to cpu_count - 1 into `table`, which must hold zv_acpi_madt_size().
 * The I/O APIC gets id `cpu_count`. Aborts if cpu_count is above 255 (ids are 8 bits).
 */
void zv_acpi_madt_init(uint8_t *table, size_t cpu_count);

#endif
