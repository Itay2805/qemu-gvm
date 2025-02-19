/*
 * QEMU GVM support
 *
 * Copyright IBM, Corp. 2008
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef QEMU_GVM_H
#define QEMU_GVM_H

#include "qemu/queue.h"
#include "hw/core/cpu.h"
#include "exec/memattrs.h"
#include "qemu/accel.h"
#include "qom/object.h"

/* These constants must never be used at runtime if gvm_enabled() is false.
 * They exist so we don't need #ifdefs around GVM-specific code that already
 * checks gvm_enabled() properly.
 */
#define GVM_CPUID_SIGNATURE      0
#define GVM_CPUID_FEATURES       0
#define GVM_FEATURE_MMU_OP       0
#define GVM_FEATURE_PV_EOI       0

extern bool gvm_allowed;
extern bool gvm_kernel_irqchip;

#define gvm_enabled()           (gvm_allowed)
/**
 * gvm_irqchip_in_kernel:
 *
 * Returns: true if the user asked us to create an in-kernel
 * irqchip via the "kernel_irqchip=on" machine option.
 * What this actually means is architecture and machine model
 * specific: on PC, for instance, it means that the LAPIC,
 * IOAPIC and PIT are all in kernel. This function should never
 * be used from generic target-independent code: use one of the
 * following functions or some other specific check instead.
 */
#define gvm_irqchip_in_kernel() (gvm_kernel_irqchip)

struct gvm_run;
struct gvm_lapic_state;
struct gvm_irq_routing_entry;

typedef struct GVMCapabilityInfo {
    const char *name;
    int value;
} GVMCapabilityInfo;

#define GVM_CAP_INFO(CAP) { "GVM_CAP_" stringify(CAP), GVM_CAP_##CAP }
#define GVM_CAP_LAST_INFO { NULL, 0 }

struct GVMState;
typedef struct GVMState GVMState;
extern GVMState *gvm_state;

/* external API */

void* gvm_gpa2hva(uint64_t gpa, bool* found);
int gvm_hva2gpa(void* hva, uint64_t length, int array_size,
                uint64_t* gpa, uint64_t* size);
int gvm_gpa_protect(uint64_t gpa, uint64_t size, uint64_t flags);
bool gvm_has_free_slot(MachineState *ms);

#ifdef NEED_CPU_H
#include "cpu.h"

void gvm_flush_coalesced_mmio_buffer(void);

int gvm_insert_breakpoint(CPUState *cpu, target_ulong addr,
                          target_ulong len, int type);
int gvm_remove_breakpoint(CPUState *cpu, target_ulong addr,
                          target_ulong len, int type);
void gvm_remove_all_breakpoints(CPUState *cpu);
int gvm_update_guest_debug(CPUState *cpu, unsigned long reinject_trap);
#ifndef _WIN32
int gvm_set_signal_mask(CPUState *cpu, const sigset_t *sigset);
#endif

int gvm_on_sigbus_vcpu(CPUState *cpu, int code, void *addr);
int gvm_on_sigbus(int code, void *addr);

/* internal API */

int gvm_ioctl(GVMState *s, int type,
        void *input, size_t input_size,
        void *output, size_t output_size);
int gvm_vm_ioctl(GVMState *s, int type,
        void *input, size_t input_size,
        void *output, size_t output_size);
int gvm_vcpu_ioctl(CPUState *cpu, int type,
        void *input, size_t input_size,
        void *output, size_t output_size);

/* Arch specific hooks */

extern const GVMCapabilityInfo gvm_arch_required_capabilities[];

void gvm_arch_pre_run(CPUState *cpu, struct gvm_run *run);
MemTxAttrs gvm_arch_post_run(CPUState *cpu, struct gvm_run *run);

int gvm_arch_handle_exit(CPUState *cpu, struct gvm_run *run);

int gvm_arch_handle_ioapic_eoi(CPUState *cpu, struct gvm_run *run);

int gvm_arch_process_async_events(CPUState *cpu);

int gvm_arch_get_registers(CPUState *cpu);

/* state subset only touched by the VCPU itself during runtime */
#define GVM_PUT_RUNTIME_STATE   1
/* state subset modified during VCPU reset */
#define GVM_PUT_RESET_STATE     2
/* full state set, modified during initialization or on vmload */
#define GVM_PUT_FULL_STATE      3

int gvm_arch_put_registers(CPUState *cpu, int level);

int gvm_arch_init(MachineState *ms, GVMState *s);

int gvm_arch_init_vcpu(CPUState *cpu);

bool gvm_vcpu_id_is_valid(int vcpu_id);

/* Returns VCPU ID to be used on GVM_CREATE_VCPU ioctl() */
unsigned long gvm_arch_vcpu_id(CPUState *cpu);

int gvm_arch_on_sigbus_vcpu(CPUState *cpu, int code, void *addr);
int gvm_arch_on_sigbus(int code, void *addr);

void gvm_arch_init_irq_routing(GVMState *s);

int gvm_arch_fixup_msi_route(struct gvm_irq_routing_entry *route,
                             uint64_t address, uint32_t data, PCIDevice *dev);

/* Notify arch about newly added MSI routes */
int gvm_arch_add_msi_route_post(struct gvm_irq_routing_entry *route,
                                int vector, PCIDevice *dev);
/* Notify arch about released MSI routes */
int gvm_arch_release_virq_post(int virq);

int gvm_set_irq(GVMState *s, int irq, int level);
int gvm_irqchip_send_msi(GVMState *s, MSIMessage msg);

void gvm_irqchip_add_irq_route(GVMState *s, int gsi, int irqchip, int pin);

void gvm_put_apic_state(DeviceState *d, struct gvm_lapic_state *kapic);
void gvm_get_apic_state(DeviceState *d, struct gvm_lapic_state *kapic);

struct gvm_guest_debug;
struct gvm_debug_exit_arch;

struct gvm_sw_breakpoint {
    target_ulong pc;
    target_ulong saved_insn;
    int use_count;
    QTAILQ_ENTRY(gvm_sw_breakpoint) entry;
};

struct gvm_sw_breakpoint *gvm_find_sw_breakpoint(CPUState *cpu,
                                                 target_ulong pc);

int gvm_sw_breakpoints_active(CPUState *cpu);

int gvm_arch_insert_sw_breakpoint(CPUState *cpu,
                                  struct gvm_sw_breakpoint *bp);
int gvm_arch_remove_sw_breakpoint(CPUState *cpu,
                                  struct gvm_sw_breakpoint *bp);
int gvm_arch_insert_hw_breakpoint(target_ulong addr,
                                  target_ulong len, int type);
int gvm_arch_remove_hw_breakpoint(target_ulong addr,
                                  target_ulong len, int type);
void gvm_arch_remove_all_hw_breakpoints(void);

void gvm_arch_update_guest_debug(CPUState *cpu, struct gvm_guest_debug *dbg);

bool gvm_arch_stop_on_emulation_error(CPUState *cpu);

int gvm_check_extension(GVMState *s, unsigned int extension);

int gvm_vm_check_extension(GVMState *s, unsigned int extension);

#define gvm_vm_enable_cap(s, capability, cap_flags, ...)             \
    ({                                                               \
        struct gvm_enable_cap cap = {                                \
            .cap = capability,                                       \
            .flags = cap_flags,                                      \
        };                                                           \
        uint64_t args_tmp[] = { __VA_ARGS__ };                       \
        int i;                                                       \
        for (i = 0; i < (int)ARRAY_SIZE(args_tmp) &&                 \
                     i < ARRAY_SIZE(cap.args); i++) {                \
            cap.args[i] = args_tmp[i];                               \
        }                                                            \
        gvm_vm_ioctl(s, GVM_ENABLE_CAP, &cap, sizeof(cap), NULL, 0); \
    })

#define gvm_vcpu_enable_cap(cpu, capability, cap_flags, ...)         \
    ({                                                               \
        struct gvm_enable_cap cap = {                                \
            .cap = capability,                                       \
            .flags = cap_flags,                                      \
        };                                                           \
        uint64_t args_tmp[] = { __VA_ARGS__ };                       \
        int i;                                                       \
        for (i = 0; i < (int)ARRAY_SIZE(args_tmp) &&                 \
                     i < ARRAY_SIZE(cap.args); i++) {                \
            cap.args[i] = args_tmp[i];                               \
        }                                                            \
        gvm_vcpu_ioctl(cpu, GVM_ENABLE_CAP,                          \
                &cap, sizeof(cap), NULL, 0);                         \
    })

uint32_t gvm_arch_get_supported_cpuid(GVMState *env, uint32_t function,
                                      uint32_t index, int reg);

void gvm_set_sigmask_len(GVMState *s, unsigned int sigmask_len);

#if !defined(CONFIG_USER_ONLY)
int gvm_physical_memory_addr_from_host(GVMState *s, void *ram_addr,
                                       hwaddr *phys_addr);
#endif

#endif /* NEED_CPU_H */

void gvm_raise_event(CPUState *cpu);
void gvm_cpu_synchronize_state(CPUState *cpu);

/**
 * gvm_irqchip_add_msi_route - Add MSI route for specific vector
 * @s:      GVM state
 * @vector: which vector to add. This can be either MSI/MSIX
 *          vector. The function will automatically detect whether
 *          MSI/MSIX is enabled, and fetch corresponding MSI
 *          message.
 * @dev:    Owner PCI device to add the route. If @dev is specified
 *          as @NULL, an empty MSI message will be inited.
 * @return: virq (>=0) when success, errno (<0) when failed.
 */
int gvm_irqchip_add_msi_route(GVMState *s, int vector, PCIDevice *dev);
int gvm_irqchip_update_msi_route(GVMState *s, int virq, MSIMessage msg,
                                 PCIDevice *dev);
void gvm_irqchip_commit_routes(GVMState *s);
void gvm_irqchip_release_virq(GVMState *s, int virq);

int gvm_irqchip_add_adapter_route(GVMState *s, AdapterInfo *adapter);

void gvm_irqchip_set_qemuirq_gsi(GVMState *s, qemu_irq irq, int gsi);
void gvm_pc_setup_irq_routing(bool pci_enabled);
void gvm_init_irq_routing(GVMState *s);

/**
 * gvm_arch_irqchip_create:
 * @GVMState: The GVMState pointer
 * @MachineState: The MachineState pointer
 *
 * Allow architectures to create an in-kernel irq chip themselves.
 *
 * Returns: < 0: error
 *            0: irq chip was not created
 *          > 0: irq chip was created
 */
int gvm_arch_irqchip_create(MachineState *ms, GVMState *s);

/**
 * gvm_set_one_reg - set a register value in GVM via GVM_SET_ONE_REG ioctl
 * @id: The register ID
 * @source: The pointer to the value to be set. It must point to a variable
 *          of the correct type/size for the register being accessed.
 *
 * Returns: 0 on success, or a negative errno on failure.
 */
int gvm_set_one_reg(CPUState *cs, uint64_t id, void *source);

/**
 * gvm_get_one_reg - get a register value from GVM via GVM_GET_ONE_REG ioctl
 * @id: The register ID
 * @target: The pointer where the value is to be stored. It must point to a
 *          variable of the correct type/size for the register being accessed.
 *
 * Returns: 0 on success, or a negative errno on failure.
 */
int gvm_get_one_reg(CPUState *cs, uint64_t id, void *target);
int gvm_get_max_memslots(void);
#endif
