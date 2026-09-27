/*
 * The XSDT (Extended System Description Table). Port of vmm/acpi/xsdt.zig.
 */
#include "vmm/acpi/xsdt.h"

#include "vmm/acpi/acpi.h"

#include <string.h>

size_t zv_acpi_xsdt_size(size_t entry_count)
{
    return sizeof(struct zv_acpi_table_header) + entry_count * sizeof(uint64_t);
}

void zv_acpi_xsdt_init(uint8_t *table, const uint64_t *addresses, size_t count)
{
    size_t size = zv_acpi_xsdt_size(count);
    struct zv_acpi_table_header header;

    zv_acpi_table_header_init(&header, "XSDT", (uint32_t)size, 2);
    memcpy(table, &header, sizeof(header));

    for (size_t i = 0; i < count; i++) {
        size_t offset = sizeof(header) + i * sizeof(uint64_t);

        memcpy(table + offset, &addresses[i], sizeof(uint64_t));
    }

    /* The checksum byte is still 0, so summing the whole table gives the right value. */
    table[offsetof(struct zv_acpi_table_header, checksum)] = zv_acpi_checksum(table, size);
}
