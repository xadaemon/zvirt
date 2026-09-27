/*
 * bzImage loader. Port of vmm/image/bzimage.zig.
 *
 * Follows the Linux x86 boot protocol (Documentation/arch/x86/boot.rst): checks the setup header,
 * fills in the boot parameters ("zero page"), places the initramfs at the end of RAM and copies
 * the protected-mode kernel to 1 MiB. The vCPU then enters the kernel's 64-bit entry point.
 */
#ifndef ZV_VMM_IMAGE_BZIMAGE_H
#define ZV_VMM_IMAGE_BZIMAGE_H

#include "vmm/image/image.h"
#include "vmm/memory.h"
#include "vmm/vm_config.h"

#include <stddef.h>
#include <stdint.h>

/* The setup header's position and size inside the bzImage file. */
#define ZV_BZIMAGE_SETUP_HEADER_OFFSET 0x1F1
#define ZV_BZIMAGE_SETUP_HEADER_SIZE 0x7B

/*
 * Loads a bzImage. Returns 0, -ENOEXEC if `data` isn't a bzImage this loader supports, -EFBIG if
 * the image or initramfs is too big for the configured RAM, or -EFAULT if guest memory is missing
 * where the boot protocol places things.
 *
 * The RAM-size check uses config->binary_size, not `size`, as in the Zig version.
 */
int zv_bzimage_parse(const uint8_t *data, size_t size, struct zv_guest_memory *memory,
                     const struct zv_vm_config *config, struct zv_image *out);

#endif
