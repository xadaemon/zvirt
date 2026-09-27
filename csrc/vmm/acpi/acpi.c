/*
 * ACPI helpers shared by all tables. Port of vmm/acpi/root.zig.
 */
#include "vmm/acpi/acpi.h"

#include <string.h>

void zv_acpi_table_header_init(struct zv_acpi_table_header *header, const char signature[4],
                               uint32_t length, uint8_t revision)
{
    memcpy(header->signature, signature, sizeof(header->signature));
    header->length = length;
    header->revision = revision;
    header->checksum = 0;
    memcpy(header->oem_id, "ZVIRT ", sizeof(header->oem_id));
    memcpy(header->oem_table_id, "ZVIRTVMM", sizeof(header->oem_table_id));
    header->oem_revision = 1;
    memcpy(header->asl_compiler_id, "ZIG ", sizeof(header->asl_compiler_id));
    header->asl_compiler_revision = 1;
}

uint8_t zv_acpi_checksum(const void *data, size_t size)
{
    const uint8_t *bytes = data;
    uint8_t sum = 0;

    for (size_t i = 0; i < size; i++)
        sum = (uint8_t)(sum + bytes[i]);

    return (uint8_t)(256 - sum);
}
