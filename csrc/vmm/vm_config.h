/*
 * VM configuration. Port of VmConfig from vmm/root.zig.
 *
 * VmConfig gets its own header (instead of living in vmm/vmm.h with the Vm) so the memory layout
 * and image loaders can use it without depending on the whole VM.
 *
 * Absent values are NULL pointers (initramfs, block device path, cmdline, network iface) or, for
 * the network device, `enabled == false`. Start from zv_vm_config_default(), which sets the
 * defaults the Zig struct declares (e.g. smp = 1); a zeroed config has smp = 0 and fails verify.
 */
#ifndef ZV_VMM_VM_CONFIG_H
#define ZV_VMM_VM_CONFIG_H

#include "utils/mac.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ZV_VM_MAX_VCPUS 16

struct zv_vm_block_device_config {
    /* Path to the disk image, or NULL for no block device. */
    const char *path;
    /* true: io_uring engine; false: synchronous pread/pwrite. Default true. */
    bool async;
};

struct zv_vm_network_config {
    bool enabled;
    struct zv_mac mac;
    /* Name of an existing TAP interface. */
    const char *iface;
};

struct zv_vm_config {
    /* Memory size in bytes. */
    size_t ram_size;

    /* Kernel image (bzImage, or a raw binary loaded at the default address). */
    const uint8_t *binary;
    size_t binary_size;

    /* initramfs image, or NULL. */
    const uint8_t *initramfs;
    size_t initramfs_size;

    struct zv_vm_block_device_config block_device;

    /* Kernel command line, or NULL (or "") for the default one. */
    const char *cmdline;

    /* Number of vCPUs, 1 to ZV_VM_MAX_VCPUS. Default 1. */
    uint8_t smp;

    /* Put virtio devices on a PCI bus instead of virtio-mmio. */
    bool pci;

    struct zv_vm_network_config network;
};

/* Everything absent or off, smp = 1, block_device.async = true. */
struct zv_vm_config zv_vm_config_default(void);

/* Returns 0, or -EINVAL if smp is 0 or above ZV_VM_MAX_VCPUS. */
int zv_vm_config_verify(const struct zv_vm_config *config);

#endif
