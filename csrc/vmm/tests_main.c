/*
 * Test executable for the vmm module (the `vmm_tests` step in build.zig). Grows with each phase.
 */
#include "test_runner.h"

extern const struct zv_test_suite zv_vmm_suite;
extern const struct zv_test_suite zv_vmm_cmos_suite;
extern const struct zv_test_suite zv_vmm_event_token_suite;
extern const struct zv_test_suite zv_vmm_memory_suite;
extern const struct zv_test_suite zv_vmm_x86_mmio_bus_suite;
extern const struct zv_test_suite zv_vmm_x86_acpi_suite;
extern const struct zv_test_suite zv_vmm_x86_gdt_suite;
extern const struct zv_test_suite zv_vmm_x86_paging_suite;
extern const struct zv_test_suite zv_vmm_image_suite;
extern const struct zv_test_suite zv_vmm_bzimage_suite;

int main(int argc, char **argv)
{
    const struct zv_test_suite *suites[] = {
        &zv_vmm_suite,
        &zv_vmm_cmos_suite,
        &zv_vmm_event_token_suite,
        &zv_vmm_memory_suite,
        &zv_vmm_x86_acpi_suite,
        &zv_vmm_x86_gdt_suite,
        &zv_vmm_x86_mmio_bus_suite,
        &zv_vmm_x86_paging_suite,
        &zv_vmm_image_suite,
        &zv_vmm_bzimage_suite,
    };

    return zv_test_main(argc, argv, suites, ZV_ARRAY_LEN(suites));
}
