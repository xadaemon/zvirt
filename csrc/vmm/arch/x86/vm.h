/*
 * x86 machine setup. Port of vmm/arch/x86/vm.zig.
 */
#ifndef ZV_VMM_ARCH_X86_VM_H
#define ZV_VMM_ARCH_X86_VM_H

#include "kvm/vcpu.h"
#include "kvm/vm.h"
#include "utils/idalloc.h"
#include "vmm/arch/x86/device_bus.h"
#include "vmm/memory.h"
#include "vmm/vm_config.h"

#include <stdint.h>

struct zv_vm;
struct zv_vm_console_config;

/* The 16 legacy interrupt lines (IrqAllocator in Zig). */
#define ZV_X86_IRQ_COUNT 16

struct zv_x86_vm {
    struct zv_device_bus device_bus;
    struct zv_id_allocator irqs;
};

/*
 * Puts the boot vCPU directly into 64-bit long mode (paging on, the GDT's code and data segments
 * loaded) at `entry_point`, with RSI pointing to the boot parameters.
 */
int zv_x86_setup_bs_vcpu(struct zv_kvm_vcpu *vcpu, uint64_t entry_point);

/* Reserves the fixed IRQs (PIT, cascade, COM ports) and sets up the device bus. */
void zv_x86_vm_init(struct zv_x86_vm *arch);

void zv_x86_vm_deinit(struct zv_x86_vm *arch);

/*
 * Creates the in-kernel timer and the guest RAM described by the memory layout, and writes the
 * boot page tables and GDT. Returns 0 or -errno.
 */
int zv_x86_vm_setup_vm(struct zv_x86_vm *arch, struct zv_kvm_vm *kvm_vm,
                       struct zv_guest_memory *memory, const struct zv_vm_config *config);

int zv_x86_vm_attach_console(struct zv_x86_vm *arch, const struct zv_vm_console_config *console,
                             struct zv_vm *vm);

/* Creates the configured devices. None can be configured yet (block and net: phases 6 to 8). */
int zv_x86_vm_setup_devices(struct zv_x86_vm *arch, const struct zv_vm_config *config,
                            struct zv_vm *vm);

/*
 * Last step before the vCPUs start: writes the kernel command line and the ACPI tables. Returns
 * 0, -E2BIG if the command line is too long, or another -errno.
 */
int zv_x86_vm_prerun(struct zv_x86_vm *arch, struct zv_vm *vm);

#endif
