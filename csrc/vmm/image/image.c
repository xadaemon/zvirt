/*
 * Kernel image loading. Port of vmm/image/root.zig.
 */
#include "vmm/image/image.h"

#include "vmm/arch/x86/layout.h"
#include "vmm/image/bzimage.h"

int zv_image_parse(const uint8_t *data, size_t size, struct zv_guest_memory *memory,
                   const struct zv_vm_config *config, struct zv_image *out)
{
    if (zv_bzimage_parse(data, size, memory, config, out) == 0)
        return 0;

    /* Not a bzImage we can load: run it as a raw binary. */
    int err = zv_guest_memory_write(memory, ZV_X86_DEFAULT_LOAD_ADDRESS, data, size);

    if (err < 0)
        return err;

    out->entry_point = ZV_X86_DEFAULT_LOAD_ADDRESS;
    out->load_address = ZV_X86_DEFAULT_LOAD_ADDRESS;
    return 0;
}
