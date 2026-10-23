/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Interface of the 64-bit (x64) processor identification module.
 *
 *          The CPU core in this tree has no long mode, so the CPUs modelled
 *          here execute in legacy 32-bit mode.  They still identify
 *          themselves as the real silicon they emulate through the extended
 *          CPUID leaves, because 32-bit operating systems of the era use
 *          those leaves to pick kernel variants (PAE, NX, SSE3, cache
 *          topology, brand string, ...).
 *
 * Authors: PCBox contributors
 *
 *          Copyright 2026 PCBox contributors
 */
#ifndef EMU_X64_H
#define EMU_X64_H

#include <stdint.h>
#include <stddef.h>

#include "cpu.h"

/*
 * Reporting the "Long Mode" feature bit (extended leaf 80000001h, EDX bit
 * 29) while the execution core cannot run 64-bit code makes x64 installers
 * switch to long mode and triple fault, so it stays off until the core grows
 * long mode support.  Flipping this to 1 is the last step of the roadmap in
 * doc/x64_support.md.
 */
#define X64_REPORT_LONG_MODE 0

/* Extended CPUID feature bits, leaf 80000001h. */
#define CPUID_LM       (1U << 29) /* EDX: Long Mode (x86-64) */
#define CPUID_MMXEXT   (1U << 22) /* EDX: AMD extended MMX */
#define CPUID_SYSCALL  (1U << 11) /* EDX: SYSCALL/SYSRET */

/* Standard CPUID feature bits that only the x64-era CPUs report. */
#define CPUID_HTT      (1U << 28) /* leaf 1 EDX: Hyper-Threading Technology */
#define CPUID_CX16     (1U << 13) /* leaf 1 ECX: CMPXCHG16B */

/* CPUID vendor the identification data is built for.  These select the
 * layout of the standard and extended leaves, not the emulated chipset. */
enum {
    X64_VENDOR_INTEL = 0,
    X64_VENDOR_AMD   = 1
};

/*
 * Everything that has to be known about one x64-era CPU in order to answer
 * the extended CPUID leaves.  Filled in by x64_get_id() from the selected
 * CPU definition, so the CPU tables stay the single source of truth.
 */
typedef struct x64_id_t {
    int      vendor;     /* X64_VENDOR_INTEL or X64_VENDOR_AMD */
    uint32_t sig;        /* leaf 1 EAX: family/model/stepping */
    uint32_t feat_edx;   /* leaf 1 EDX: standard features */
    uint32_t feat_ecx;   /* leaf 1 ECX: standard features */
    uint32_t ext_edx;    /* 80000001h EDX: extended features */
    uint32_t ext_ecx;    /* 80000001h ECX: extended features */
    uint32_t max_std;    /* highest standard leaf */
    uint32_t max_ext;    /* highest extended leaf */
    uint32_t cores;      /* logical processors - 1, for leaf 1 EBX[23:16] */
    uint32_t phys_bits;  /* physical address bits (80000008h EAX[7:0]) */
    uint32_t virt_bits;  /* linear address bits (80000008h EAX[15:8]) */
    uint32_t l1d_kb;     /* L1 data cache, KB */
    uint32_t l1i_kb;     /* L1 instruction cache, KB */
    uint32_t l2_kb;      /* L2 cache, KB */
    uint32_t l2_ways;    /* L2 associativity */
    uint32_t l3_kb;      /* L3 cache, KB (0 = none) */
    char     brand[49];  /* brand string for leaves 80000002h-80000004h */
} x64_id_t;

/* Build the identification data for a CPU definition.  Unknown CPU types
 * yield an all-zero structure with only the address widths filled in. */
extern void x64_get_id(const CPU *cpu_s, x64_id_t *id);

/* Answer one CPUID leaf.  Writes EAX, EBX, ECX and EDX; leaves the module
 * state untouched, so it is safe to call for every CPUID instruction. */
extern void x64_cpuid(x64_id_t *id, uint32_t leaf);

/* x64_get_id() followed by x64_cpuid(), i.e. answer a leaf straight from the
 * selected CPU.  This is what cpu.c calls for the x64-era CPU types. */
extern void x64_cpuid_leaf(const CPU *cpu_s, uint32_t leaf);

/* Non-zero when the Long Mode bit is actually advertised to the guest. */
extern int x64_long_mode_reported(void);

#endif /*EMU_X64_H*/
