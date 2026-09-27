/*
 * The RSDP (Root System Description Pointer). Port of vmm/acpi/rsdt.zig.
 *
 * The guest finds all ACPI tables starting from the RSDP, whose address it gets through the boot
 * parameters. The RSDP points to the XSDT.
 */
#ifndef ZV_VMM_ACPI_RSDT_H
#define ZV_VMM_ACPI_RSDT_H

#include <stddef.h>
#include <stdint.h>

struct zv_acpi_rsdp {
    /* "RSD PTR " */
    char signature[8];
    /* Checksum of the first 20 bytes (the ACPI 1.0 part). */
    uint8_t checksum;
    char oem_id[6];
    /* 2 for ACPI 2.0 and later. */
    uint8_t revision;
    /* 32-bit address of the RSDT, which zvirt doesn't provide (0). */
    uint32_t rsdt_physical_address;
    /* Length of this structure: 36. */
    uint32_t length;
    /* 64-bit address of the XSDT. */
    uint64_t xsdt_physical_address;
    /* Checksum of all 36 bytes. */
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed));

_Static_assert(sizeof(struct zv_acpi_rsdp) == 36, "RSDP is 36 bytes");
_Static_assert(offsetof(struct zv_acpi_rsdp, xsdt_physical_address) == 0x18,
               "RSDP XSDT address is at offset 0x18");

/* The part covered by the ACPI 1.0 checksum. */
#define ZV_ACPI_RSDP_V1_LENGTH 20

void zv_acpi_rsdp_init(struct zv_acpi_rsdp *rsdp, uint32_t rsdt_address, uint64_t xsdt_address);

#endif
