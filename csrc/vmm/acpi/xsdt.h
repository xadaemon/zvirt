/*
 * The XSDT (Extended System Description Table). Port of vmm/acpi/xsdt.zig.
 *
 * Layout: a table header followed by 64-bit addresses of the other tables (unaligned).
 */
#ifndef ZV_VMM_ACPI_XSDT_H
#define ZV_VMM_ACPI_XSDT_H

#include <stddef.h>
#include <stdint.h>

/* Size in bytes of an XSDT listing `entry_count` tables. */
size_t zv_acpi_xsdt_size(size_t entry_count);

/* Writes an XSDT listing `addresses` into `table`, which must hold zv_acpi_xsdt_size(count). */
void zv_acpi_xsdt_init(uint8_t *table, const uint64_t *addresses, size_t count);

#endif
