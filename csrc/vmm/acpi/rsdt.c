/*
 * The RSDP (Root System Description Pointer). Port of vmm/acpi/rsdt.zig.
 *
 * Deviation: the Zig version computes the extended checksum first, while the ACPI 1.0 checksum
 * byte is still 0, then sets that byte, which leaves the 36-byte sum wrong. Here the 20-byte
 * checksum comes first, so both sums are 0 as the spec requires.
 */
#include "vmm/acpi/rsdt.h"

#include "vmm/acpi/acpi.h"

#include <string.h>

void zv_acpi_rsdp_init(struct zv_acpi_rsdp *rsdp, uint32_t rsdt_address, uint64_t xsdt_address)
{
    memset(rsdp, 0, sizeof(*rsdp));
    memcpy(rsdp->signature, "RSD PTR ", sizeof(rsdp->signature));
    memcpy(rsdp->oem_id, "zvirt ", sizeof(rsdp->oem_id));
    rsdp->revision = 2;
    rsdp->rsdt_physical_address = rsdt_address;
    rsdp->length = sizeof(*rsdp);
    rsdp->xsdt_physical_address = xsdt_address;

    /* Both checksum bytes are 0 here. The extended checksum covers the other one, so it's last. */
    rsdp->checksum = zv_acpi_checksum(rsdp, ZV_ACPI_RSDP_V1_LENGTH);
    rsdp->extended_checksum = zv_acpi_checksum(rsdp, sizeof(*rsdp));
}
