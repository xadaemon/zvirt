/*
 * VM configuration. Port of VmConfig from vmm/root.zig.
 */
#include "vmm/vm_config.h"

#include <errno.h>

struct zv_vm_config zv_vm_config_default(void)
{
    struct zv_vm_config config = {0};

    config.smp = 1;
    config.block_device.async = true;
    return config;
}

int zv_vm_config_verify(const struct zv_vm_config *config)
{
    if (config->smp == 0 || config->smp > ZV_VM_MAX_VCPUS)
        return -EINVAL;

    return 0;
}
