/*
 * ACPI helpers shared by all tables. Port of vmm/acpi/root.zig.
 *
 * ACPI tables are byte-packed structures in guest memory. The structs here are packed with sizes
 * checked at compile time. Variable-length tables (XSDT, MADT) are built by copying each record to
 * its byte offset with memcpy, so no unaligned pointers are ever formed.
 */
#ifndef ZV_VMM_ACPI_ACPI_H
#define ZV_VMM_ACPI_ACPI_H

#include <stddef.h>
#include <stdint.h>

/* The header every ACPI table except the RSDP starts with. */
struct zv_acpi_table_header {
    /* ASCII table signature, e.g. "APIC". */
    char signature[4];
    /* Length of the table in bytes, including this header. */
    uint32_t length;
    /* Revision of the table's structure. */
    uint8_t revision;
    /* Makes the sum of all bytes of the table 0 (mod 256). */
    uint8_t checksum;
    /* ASCII OEM identification. */
    char oem_id[6];
    /* ASCII OEM table identification. */
    char oem_table_id[8];
    uint32_t oem_revision;
    /* ASCII ID of the compiler that built the table. */
    char asl_compiler_id[4];
    uint32_t asl_compiler_revision;
} __attribute__((packed));

_Static_assert(sizeof(struct zv_acpi_table_header) == 36, "ACPI table header is 36 bytes");

/* The header of each MADT entry. */
struct zv_acpi_subtable_header {
    uint8_t type;
    /* Length of the entry in bytes, including this header. */
    uint8_t length;
} __attribute__((packed));

/*
 * Fills a table header with zvirt's OEM identification and a zero checksum. The caller computes
 * the checksum once the whole table is written.
 */
void zv_acpi_table_header_init(struct zv_acpi_table_header *header, const char signature[4],
                               uint32_t length, uint8_t revision);

/* Returns the byte that makes the sum of `size` bytes plus itself 0 (mod 256). */
uint8_t zv_acpi_checksum(const void *data, size_t size);

#endif
