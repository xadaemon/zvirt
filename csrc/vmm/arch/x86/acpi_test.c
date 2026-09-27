/*
 * Tests for vmm/arch/x86/acpi.c and the tables in vmm/acpi/. New; the Zig ACPI code only has
 * compile-time size checks. The tests walk the tables in guest memory the way a kernel would.
 */
#include "vmm/arch/x86/acpi.h"

#include "test_runner.h"
#include "vmm/acpi/acpi.h"
#include "vmm/acpi/madt.h"
#include "vmm/acpi/rsdt.h"
#include "vmm/arch/x86/layout.h"

#include <errno.h>
#include <string.h>

/* Low RAM up to and including the ACPI area. */
#define LOW_MEMORY_SIZE (ZV_X86_BOOT_ACPI_ADDR + ZV_X86_BOOT_ACPI_SIZE)

static uint8_t low_ram[LOW_MEMORY_SIZE];

/* Sum of all bytes mod 256; 0 for a table with a correct checksum. */
static uint8_t byte_sum(const uint8_t *data, size_t size)
{
    uint8_t sum = 0;

    for (size_t i = 0; i < size; i++)
        sum = (uint8_t)(sum + data[i]);

    return sum;
}

/* Returns a pointer to guest physical address `gpa` inside low_ram. */
static const uint8_t *guest(uint64_t gpa, size_t size)
{
    ZV_EXPECT(gpa + size <= LOW_MEMORY_SIZE);
    return low_ram + gpa;
}

static void check_tables(size_t cpu_count)
{
    struct zv_guest_memory *memory = NULL;

    memset(low_ram, 0, sizeof(low_ram));
    ZV_EXPECT_EQ(0, zv_guest_memory_new(&memory));
    ZV_EXPECT_EQ(0, zv_guest_memory_add(memory, 0, low_ram, sizeof(low_ram), false));
    ZV_EXPECT_EQ(0, zv_x86_acpi_setup_tables(memory, cpu_count));
    zv_guest_memory_deinit(memory);

    /* RSDP, at the address the boot parameters give the kernel. */
    struct zv_acpi_rsdp rsdp;
    const uint8_t *rsdp_bytes = guest(ZV_X86_BOOT_ACPI_ADDR, sizeof(rsdp));

    memcpy(&rsdp, rsdp_bytes, sizeof(rsdp));
    ZV_EXPECT(memcmp(rsdp.signature, "RSD PTR ", 8) == 0);
    ZV_EXPECT_EQ(2, rsdp.revision);
    ZV_EXPECT_EQ(0, byte_sum(rsdp_bytes, ZV_ACPI_RSDP_V1_LENGTH));
    ZV_EXPECT_EQ(0, byte_sum(rsdp_bytes, sizeof(rsdp)));

    /* XSDT, with a single entry pointing to the MADT. */
    struct zv_acpi_table_header xsdt;
    const uint8_t *xsdt_bytes = guest(rsdp.xsdt_physical_address, sizeof(xsdt));

    memcpy(&xsdt, xsdt_bytes, sizeof(xsdt));
    ZV_EXPECT(memcmp(xsdt.signature, "XSDT", 4) == 0);
    ZV_EXPECT_EQ(sizeof(xsdt) + sizeof(uint64_t), xsdt.length);
    ZV_EXPECT_EQ(0, byte_sum(xsdt_bytes, xsdt.length));

    uint64_t madt_address;

    memcpy(&madt_address, xsdt_bytes + sizeof(xsdt), sizeof(madt_address));

    /* MADT: fixed part, one local APIC per CPU, one I/O APIC. */
    struct zv_acpi_madt madt;
    const uint8_t *madt_bytes = guest(madt_address, sizeof(madt));

    memcpy(&madt, madt_bytes, sizeof(madt));
    ZV_EXPECT(memcmp(madt.header.signature, "APIC", 4) == 0);
    ZV_EXPECT_EQ(44 + 8 * cpu_count + 12, madt.header.length);
    ZV_EXPECT_EQ(0, byte_sum(madt_bytes, madt.header.length));
    ZV_EXPECT_EQ(ZV_ACPI_MADT_LOCAL_APIC_ADDRESS, madt.address);

    size_t offset = sizeof(madt);

    for (size_t cpu = 0; cpu < cpu_count; cpu++) {
        struct zv_acpi_madt_local_apic local_apic;

        memcpy(&local_apic, madt_bytes + offset, sizeof(local_apic));
        ZV_EXPECT_EQ(0, local_apic.header.type);
        ZV_EXPECT_EQ(8, local_apic.header.length);
        ZV_EXPECT_EQ(cpu, local_apic.processor_id);
        ZV_EXPECT_EQ(cpu, local_apic.id);
        ZV_EXPECT_EQ(1, local_apic.lapic_flags);
        offset += sizeof(local_apic);
    }

    struct zv_acpi_madt_io_apic io_apic;

    memcpy(&io_apic, madt_bytes + offset, sizeof(io_apic));
    ZV_EXPECT_EQ(1, io_apic.header.type);
    ZV_EXPECT_EQ(12, io_apic.header.length);
    ZV_EXPECT_EQ(cpu_count, io_apic.id);
    ZV_EXPECT_EQ(ZV_ACPI_MADT_IO_APIC_ADDRESS, io_apic.address);
}

static void test_tables_for_one_cpu(void)
{
    check_tables(1);
}

static void test_tables_for_sixteen_cpus(void)
{
    check_tables(16);
}

static void test_missing_acpi_area_is_an_error(void)
{
    struct zv_guest_memory *memory = NULL;

    /* Only low RAM below the ACPI area is mapped. */
    ZV_EXPECT_EQ(0, zv_guest_memory_new(&memory));
    ZV_EXPECT_EQ(0, zv_guest_memory_add(memory, 0, low_ram, ZV_X86_BOOT_ACPI_ADDR, false));
    ZV_EXPECT_EQ(-EFAULT, zv_x86_acpi_setup_tables(memory, 1));
    zv_guest_memory_deinit(memory);
}

static const struct zv_test tests[] = {
    {"tables for one CPU", test_tables_for_one_cpu},
    {"tables for sixteen CPUs", test_tables_for_sixteen_cpus},
    {"missing ACPI area is an error", test_missing_acpi_area_is_an_error},
};

const struct zv_test_suite zv_vmm_x86_acpi_suite = {"vmm.arch.x86.acpi", tests,
                                                    ZV_ARRAY_LEN(tests)};
