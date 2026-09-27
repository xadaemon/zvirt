/*
 * x86 machine setup. Port of vmm/arch/x86/vm.zig.
 */
#include "vmm/arch/x86/vm.h"

#include "test_utils/mmap.h"
#include "vmm/arch/x86/acpi.h"
#include "vmm/arch/x86/gdt.h"
#include "vmm/arch/x86/layout.h"
#include "vmm/arch/x86/paging.h"
#include "vmm/vmm.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define DEFAULT_CMDLINE "console=ttyS0 earlycon=uart,io,0x3f8 nokaslr panic=-1 reboot=t"

/* Room for the command line, including the terminating NUL. */
#define CMDLINE_CAPACITY 4096

/* EFER: long mode enable, long mode active. */
#define EFER_LME (1 << 8)
#define EFER_LMA (1 << 10)

/* CR0: protected mode, paging. */
#define CR0_PE (1u << 0)
#define CR0_PG (1u << 31)

/* CR4: physical address extension (required for long mode). */
#define CR4_PAE (1 << 5)

/* Legacy IRQs that are wired to fixed devices. */
#define IRQ_PIT 0
#define IRQ_PIC_CASCADE 2
#define IRQ_COM2 3
#define IRQ_COM1 4

int zv_x86_setup_bs_vcpu(struct zv_kvm_vcpu *vcpu, uint64_t entry_point)
{
    struct kvm_sregs sregs;
    int err = zv_kvm_vcpu_get_sregs(vcpu, &sregs);

    if (err < 0)
        return err;

    /* Load the segment registers with (cached copies of) the GDT's descriptors. */
    zv_x86_gdt_setup_segment(&sregs.cs, true, ZV_X86_GDT_CODE_INDEX);
    zv_x86_gdt_setup_segment(&sregs.ds, false, ZV_X86_GDT_DATA_INDEX);
    zv_x86_gdt_setup_segment(&sregs.ss, false, ZV_X86_GDT_DATA_INDEX);
    zv_x86_gdt_setup_segment(&sregs.gs, false, ZV_X86_GDT_DATA_INDEX);
    zv_x86_gdt_setup_segment(&sregs.fs, false, ZV_X86_GDT_DATA_INDEX);
    zv_x86_gdt_setup_segment(&sregs.es, false, ZV_X86_GDT_DATA_INDEX);

    sregs.efer = EFER_LME | EFER_LMA;
    sregs.cr0 = CR0_PG | CR0_PE;
    sregs.cr4 = CR4_PAE;
    sregs.cr3 = ZV_X86_PGD_ADDR;

    err = zv_kvm_vcpu_set_sregs(vcpu, &sregs);
    if (err < 0)
        return err;

    /* The GDT register is set through sregs2. */
    struct kvm_sregs2 sregs2;

    err = zv_kvm_vcpu_get_sregs2(vcpu, &sregs2);
    if (err < 0)
        return err;

    sregs2.gdt.base = ZV_X86_GDT_ADDR;
    sregs2.gdt.limit = 31;

    err = zv_kvm_vcpu_set_sregs2(vcpu, &sregs2);
    if (err < 0)
        return err;

    struct kvm_regs regs;

    err = zv_kvm_vcpu_get_regs(vcpu, &regs);
    if (err < 0)
        return err;

    regs.rip = entry_point;
    regs.rsi = ZV_X86_BOOT_PARAM_ADDR;
    return zv_kvm_vcpu_set_regs(vcpu, &regs);
}

static void reserve_irq(struct zv_x86_vm *arch, size_t irq)
{
    if (zv_id_allocator_allocate_specific(&arch->irqs, irq) != 0) {
        fprintf(stderr, "x86 vm: IRQ %zu already reserved\n", irq);
        abort();
    }
}

void zv_x86_vm_init(struct zv_x86_vm *arch)
{
    zv_id_allocator_init(&arch->irqs, ZV_X86_IRQ_COUNT);
    reserve_irq(arch, IRQ_COM1);
    reserve_irq(arch, IRQ_COM2);
    reserve_irq(arch, IRQ_PIT);
    reserve_irq(arch, IRQ_PIC_CASCADE);

    zv_device_bus_init(&arch->device_bus);
}

void zv_x86_vm_deinit(struct zv_x86_vm *arch)
{
    zv_device_bus_deinit(&arch->device_bus);
}

/* Maps the RAM and ACPI regions of the memory layout and writes the boot page tables and GDT. */
static int setup_memory(struct zv_guest_memory *memory, const struct zv_vm_config *config)
{
    struct zv_x86_memory_slot layout[ZV_X86_MEMORY_SLOT_COUNT];

    zv_x86_memory_layout(config, layout);

    for (int i = 0; i < ZV_X86_MEMORY_SLOT_COUNT; i++) {
        if (layout[i].kind != ZV_X86_MEMORY_RAM && layout[i].kind != ZV_X86_MEMORY_ACPI)
            continue;

        void *ram = NULL;
        int err = zv_mmap(NULL, layout[i].length, PROT_READ | PROT_WRITE,
                          MAP_ANONYMOUS | MAP_PRIVATE, -1, 0, &ram);

        if (err < 0)
            return err;

        err = zv_guest_memory_add(memory, layout[i].start, ram, layout[i].length, true);
        if (err < 0) {
            zv_munmap(ram, layout[i].length);
            return err;
        }
    }

    uint64_t table[ZV_X86_PAGE_TABLE_ENTRIES];
    int err;

    zv_x86_paging_fill_pgd(table);
    err = zv_guest_memory_write(memory, ZV_X86_PGD_ADDR, table, sizeof(table));
    if (err < 0)
        return err;

    zv_x86_paging_fill_pud(table);
    err = zv_guest_memory_write(memory, ZV_X86_PUD_ADDR, table, sizeof(table));
    if (err < 0)
        return err;

    zv_x86_paging_fill_pmd(table);
    err = zv_guest_memory_write(memory, ZV_X86_PMD_ADDR, table, sizeof(table));
    if (err < 0)
        return err;

    struct zv_x86_gdt gdt = zv_x86_gdt_build();

    return zv_guest_memory_write(memory, ZV_X86_GDT_ADDR, &gdt, sizeof(gdt));
}

int zv_x86_vm_setup_vm(struct zv_x86_vm *arch, struct zv_kvm_vm *kvm_vm,
                       struct zv_guest_memory *memory, const struct zv_vm_config *config)
{
    (void)arch;

    int err = zv_kvm_vm_create_pit(kvm_vm);

    if (err < 0)
        return err;

    return setup_memory(memory, config);
}

int zv_x86_vm_attach_console(struct zv_x86_vm *arch, const struct zv_vm_console_config *console,
                             struct zv_vm *vm)
{
    return zv_device_bus_attach_console(&arch->device_bus, console, vm);
}

int zv_x86_vm_setup_devices(struct zv_x86_vm *arch, const struct zv_vm_config *config,
                            struct zv_vm *vm)
{
    (void)arch;
    (void)config;
    (void)vm;

    /* Block and network devices come in phases 6 to 8; zv_vm_new rejects them until then. */
    return 0;
}

int zv_x86_vm_prerun(struct zv_x86_vm *arch, struct zv_vm *vm)
{
    (void)arch;

    /* A user command line replaces the default one. An empty one counts as none, as in Zig. */
    const char *user = vm->config.cmdline;
    const char *base = (user != NULL && user[0] != '\0') ? user : DEFAULT_CMDLINE;
    char cmdline[CMDLINE_CAPACITY];
    int length = snprintf(cmdline, sizeof(cmdline), "%s%s", base, vm->config.pci ? "" : " pci=off");

    if (length < 0 || (size_t)length >= sizeof(cmdline))
        return -E2BIG;

    /* The virtio_mmio.device=... arguments are appended here in phase 6. */

    /* Unlike the Zig version, the terminating NUL is written too. */
    int err =
        zv_guest_memory_write(vm->memory, ZV_X86_BOOT_CMDLINE_ADDR, cmdline, (size_t)length + 1);

    if (err < 0)
        return err;

    return zv_x86_acpi_setup_tables(vm->memory, vm->vcpus_count);
}
