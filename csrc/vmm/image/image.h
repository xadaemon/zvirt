/*
 * Kernel image loading. Port of vmm/image/root.zig.
 */
#ifndef ZV_VMM_IMAGE_IMAGE_H
#define ZV_VMM_IMAGE_IMAGE_H

#include "vmm/memory.h"
#include "vmm/vm_config.h"

#include <stddef.h>
#include <stdint.h>

struct zv_image {
    /* Where the boot vCPU starts executing. */
    uint64_t entry_point;
    /* Where the image was loaded. */
    uint64_t load_address;
};

/*
 * Loads `data` into guest memory. A bzImage is loaded following the Linux boot protocol (together
 * with the initramfs from `config`). Anything else, including a bzImage that fails to load for any
 * reason, is copied as-is to ZV_X86_DEFAULT_LOAD_ADDRESS and started there, as in the Zig version.
 *
 * Returns 0, or -errno if even the raw copy doesn't fit in guest memory.
 */
int zv_image_parse(const uint8_t *data, size_t size, struct zv_guest_memory *memory,
                   const struct zv_vm_config *config, struct zv_image *out);

#endif
