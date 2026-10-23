/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Implementation of the 64-bit (x64) processor identification
 *          module: the data and the code behind the extended CPUID leaves
 *          (80000000h-80000008h) of the Athlon 64 / Pentium 4 EM64T /
 *          Pentium D / Core 2 class CPUs.
 *
 *          The execution core has no long mode yet, so these CPUs run in
 *          legacy 32-bit mode and only their identification is 64-bit
 *          accurate.  See doc/x64_support.md for the full status and for
 *          the remaining long mode work.
 *
 * Authors: PCBox contributors
 *
 *          Copyright 2026 PCBox contributors
 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "x64.h"

/* X64_REPORT_LONG_MODE gates the "Long Mode" feature bit (extended leaf
 * 80000001h, EDX bit 29); see x64.h.  Everything else about these CPUs is
 * reported exactly as the real silicon does. */


static const uint32_t x64_feat_edx_common =
    CPUID_FPU | CPUID_VME | CPUID_DE | CPUID_PSE | CPUID_TSC | CPUID_MSR |
    CPUID_PAE | CPUID_MCE | CPUID_CMPXCHG8B | CPUID_LAPIC | CPUID_SEP |
    CPUID_MTRR | CPUID_PGE | CPUID_MCA | CPUID_CMOV | CPUID_PAT |
    CPUID_PSE36 | CPUID_CLFLUSH | CPUID_MMX | CPUID_FXSR |
    CPUID_SSE | CPUID_SSE2;

void
x64_get_id(const CPU *cpu_s, x64_id_t *id)
{
    memset(id, 0, sizeof(*id));

    /* Every x64 machine we emulate is a 32-bit-bus, 36-bit-physical-address
     * design from the emulation core's point of view. */
    id->phys_bits = 36;
    id->virt_bits = 48;
    id->max_ext   = 0x80000008;
    id->cores     = 0;

    switch (cpu_s->cpu_type) {
        case CPU_ATHLON64:
            id->vendor   = X64_VENDOR_AMD;
            id->sig      = cpu_s->cpuid_model;   /* 0F_C0h: ClawHammer / Newcastle */
            id->max_std  = 0x00000001;
            id->l1d_kb   = 64;
            id->l1i_kb   = 64;
            id->l2_kb    = 512;
            id->l2_ways  = 16;
            snprintf(id->brand, sizeof(id->brand), "AMD Athlon(tm) 64 Processor 3200+");
            break;

        case CPU_ATHLON64_X2:
            id->vendor   = X64_VENDOR_AMD;
            id->sig      = cpu_s->cpuid_model;   /* 0F_B1h: Manchester / Toledo */
            id->max_std  = 0x00000001;
            id->l1d_kb   = 64;
            id->l1i_kb   = 64;
            id->l2_kb    = 512;
            id->l2_ways  = 16;
            id->cores    = 1;                   /* two cores */
            snprintf(id->brand, sizeof(id->brand), "AMD Athlon(tm) 64 X2 Dual Core Processor 4400+");
            break;

        case CPU_SEMPRON:
            id->vendor   = X64_VENDOR_AMD;
            id->sig      = cpu_s->cpuid_model;   /* 0F_C0h: Palermo */
            id->max_std  = 0x00000001;
            id->l1d_kb   = 64;
            id->l1i_kb   = 64;
            id->l2_kb    = 256;
            id->l2_ways  = 16;
            snprintf(id->brand, sizeof(id->brand), "AMD Sempron(tm) Processor 3400+");
            break;

        case CPU_PENTIUM4_EM64T:
            id->vendor   = X64_VENDOR_INTEL;
            id->sig      = cpu_s->cpuid_model;   /* 0F_41h: Prescott, LGA775 */
            id->max_std  = 0x00000002;           /* no deterministic cache leaf */
            id->l1d_kb   = 16;
            id->l1i_kb   = 12;                   /* trace cache, reported as 12K uop cache */
            id->l2_kb    = 1024;
            id->l2_ways  = 8;
            snprintf(id->brand, sizeof(id->brand), "Intel(R) Pentium(R) 4 CPU 3.20GHz");
            break;

        case CPU_PENTIUM_D:
            id->vendor   = X64_VENDOR_INTEL;
            id->sig      = cpu_s->cpuid_model;   /* 0F_64h: Presler */
            id->max_std  = 0x0000000A;           /* deterministic cache leaf present */
            id->l1d_kb   = 16;
            id->l1i_kb   = 12;
            id->l2_kb    = 2048;                 /* 2 x 1 MB, one per core */
            id->l2_ways  = 8;
            id->cores    = 1;
            snprintf(id->brand, sizeof(id->brand), "Intel(R) Pentium(R) D CPU 3.00GHz");
            break;

        case CPU_CORE2:
            id->vendor   = X64_VENDOR_INTEL;
            id->sig      = cpu_s->cpuid_model;   /* 06_F6h: Conroe / Merom */
            id->max_std  = 0x0000000A;
            id->l1d_kb   = 32;
            id->l1i_kb   = 32;
            id->l2_kb    = 2048;                 /* shared, 8-way */
            id->l2_ways  = 8;
            id->cores    = 1;
            snprintf(id->brand, sizeof(id->brand), "Intel(R) Core(TM)2 CPU          6400  @ 2.13GHz");
            break;

        case CPU_CORE2_QUAD:
            id->vendor   = X64_VENDOR_INTEL;
            id->sig      = cpu_s->cpuid_model;   /* 06_F7h: Kentsfield */
            id->max_std  = 0x0000000A;
            id->l1d_kb   = 32;
            id->l1i_kb   = 32;
            id->l2_kb    = 4096;                 /* 2 x 2 MB, shared per die pair */
            id->l2_ways  = 16;
            id->cores    = 3;
            snprintf(id->brand, sizeof(id->brand), "Intel(R) Core(TM)2 Quad CPU    Q6600  @ 2.40GHz");
            break;

        default:
            break;
    }

    id->feat_edx = x64_feat_edx_common;

    if (id->cores)
        id->feat_edx |= CPUID_HTT;

    if (id->vendor == X64_VENDOR_AMD) {
        /* K8: SSE3 on revision E and later, CMPXCHG16B everywhere. */
        id->feat_ecx = CPUID_CX16;
        if (cpu_s->cpu_type != CPU_SEMPRON)
            id->feat_ecx |= CPUID_SSE3;
        else
            id->feat_ecx |= CPUID_SSE3;   /* Palermo is SSE3-capable as well */

        id->ext_edx = CPUID_FPU | CPUID_SYSCALL | CPUID_MMX | CPUID_MMXEXT |
                      CPUID_3DNOW | CPUID_3DNOWE | CPUID_NX | CPUID_PAE |
                      CPUID_CMPXCHG8B | CPUID_SEP;
    } else {
        id->feat_ecx = CPUID_CX16;
        if (cpu_s->cpu_type == CPU_CORE2 || cpu_s->cpu_type == CPU_CORE2_QUAD) {
            id->feat_ecx |= CPUID_SSE3 | CPUID_SSSE3 | CPUID_MONITOR_MWAIT;
        } else {
            id->feat_ecx |= CPUID_SSE3 | CPUID_MONITOR_MWAIT;
        }

        id->ext_edx = CPUID_FPU | CPUID_NX | CPUID_PAE | CPUID_CMPXCHG8B |
                      CPUID_SEP | CPUID_CMOV | CPUID_PAT;
    }

#if X64_REPORT_LONG_MODE
    id->ext_edx |= CPUID_LM;
#endif
}

/* Intel deterministic cache parameters (CPUID leaf 4). */
static void
x64_cache_link(uint32_t level, uint32_t type, uint32_t kb, uint32_t ways,
                 uint32_t shared_cores)
{
    const uint32_t line_size = 64;
    uint32_t       sets      = (kb * 1024) / (ways * line_size);

    EAX = (type & 0x1f) | ((level & 0x07) << 5) | (1 << 8) |
          (((shared_cores - 1) & 0x0fff) << 14) | (((shared_cores - 1) & 0x3f) << 26);
    EBX = ((ways - 1) & 0x3ff) << 22;
    EBX |= (line_size - 1) & 0xfff;
    ECX = (sets - 1);
    EDX = 0;
}

void
x64_cpuid(x64_id_t *id, uint32_t leaf)
{
    EAX = EBX = ECX = EDX = 0;

    if (leaf == 0x80000000) {
        EAX = id->max_ext;
        if (id->vendor == X64_VENDOR_AMD) {
            EBX = 0x68747541; /* AuthenticAMD */
            EDX = 0x69746E65;
            ECX = 0x444D4163;
        } else {
            EBX = 0x756e6547; /* GenuineIntel */
            EDX = 0x49656e69;
            ECX = 0x6c65746e;
        }
        return;
    }

    if (leaf >= 0x80000002 && leaf <= 0x80000004) {
        const uint8_t *brand = (const uint8_t *) id->brand + ((leaf - 0x80000002) * 16);

        memcpy(&EAX, brand + 0, 4);
        memcpy(&EBX, brand + 4, 4);
        memcpy(&ECX, brand + 8, 4);
        memcpy(&EDX, brand + 12, 4);
        return;
    }

    if (id->vendor == X64_VENDOR_AMD) {
        switch (leaf) {
            case 0x00000000:
                EAX = id->max_std;
                EBX = 0x68747541; /* AuthenticAMD */
                EDX = 0x69746E65;
                ECX = 0x444D4163;
                break;

            case 0x00000001:
                EAX = id->sig;
                EBX = ((id->cores & 0xff) << 16);
                ECX = id->feat_ecx;
                EDX = id->feat_edx;
                if (!(msr.apic_base & (1 << 11)))
                    EDX &= ~CPUID_LAPIC;
                break;

            case 0x80000001:
                EAX = id->sig;
                EBX = ECX = 0;
                EDX = id->ext_edx;
                if (!(msr.apic_base & (1 << 11)))
                    EDX &= ~CPUID_LAPIC;
                break;

            case 0x80000005:
                /* TLB entries (junk) followed by L1 data/instruction caches:
                 * 64 KB, 2-way, 64-byte line for both. */
                EAX = 0xff30ff10;
                EBX = 0xff20ff20;
                ECX = 0x04004140; /* L1 data */
                EDX = 0x04004140; /* L1 instruction */
                break;

            case 0x80000006:
                /* 64-byte L2 cache line, 16-way, size in KB. */
                ECX = ((id->l2_kb & 0xffff) << 16) | (8 << 12) | (0 << 8) | 0x40;
                EDX = 0;
                break;

            case 0x80000007:
                /* Advanced power management: none of the legacy timers. */
                EAX = EBX = ECX = EDX = 0;
                break;

            case 0x80000008:
                EAX = (id->virt_bits << 8) | id->phys_bits;
                EBX = ECX = EDX = 0;
                break;

            default:
                EAX = EBX = ECX = EDX = 0;
                break;
        }
        return;
    }

    /* Intel */
    switch (leaf) {
        case 0x00000000:
            EAX = id->max_std;
            EBX = 0x756e6547; /* GenuineIntel */
            EDX = 0x49656e69;
            ECX = 0x6c65746e;
            break;

        case 0x00000001:
            EAX = id->sig;
            EBX = (8 << 8) | ((id->cores & 0xff) << 16);
            ECX = id->feat_ecx;
            EDX = id->feat_edx;
            if (!(msr.apic_base & (1 << 11)))
                EDX &= ~CPUID_LAPIC;
            break;

        case 0x00000002:
            if (id->max_std < 0x04) {
                /* Legacy descriptor table: 16 KB 4-way L1 data/instruction,
                 * 1 MB 8-way L2. */
                EAX = 0x00000001;
                EBX = 0x0c | (0x0d << 8) | (0x3b << 16) | (0x41 << 24);
                ECX = EDX = 0;
            } else {
                EAX = EBX = ECX = EDX = 0;
            }
            break;

        case 0x00000004:
            if (id->max_std < 0x04) {
                EAX = EBX = ECX = EDX = 0;
            } else if (ECX == 0) {
                x64_cache_link(1, 1, id->l1d_kb, 8, 1); /* L1 data, 8-way */
            } else if (ECX == 1) {
                x64_cache_link(1, 2, id->l1i_kb, 8, 1); /* L1 instruction */
            } else if (ECX == 2) {
                x64_cache_link(2, 3, id->l2_kb / (id->cores + 1), id->l2_ways, id->cores + 1);
            } else if (ECX == 3 && id->l3_kb) {
                x64_cache_link(3, 3, id->l3_kb, 16, id->cores + 1);
            } else {
                EAX = EBX = ECX = EDX = 0; /* no more caches */
            }
            break;

        case 0x00000005:
            EAX = EBX = ECX = EDX = 0; /* MONITOR/MWAIT leaf not provided */
            break;

        case 0x00000006:
            /* Digital thermal sensor / power management capabilities. */
            EAX = 0x00000001;
            EBX = ECX = EDX = 0;
            break;

        case 0x0000000A:
            /* Architectural performance monitoring: not implemented. */
            EAX = EBX = ECX = EDX = 0;
            break;

        case 0x80000001:
            EAX = id->sig;
            EBX = ECX = 0;
            EDX = id->ext_edx;
            break;

        case 0x80000008:
            EAX = (id->virt_bits << 8) | id->phys_bits;
            EBX = ECX = EDX = 0;
            break;

        default:
            EAX = EBX = ECX = EDX = 0;
            break;
    }
}

/* Answer a leaf straight from the selected CPU definition. */
void
x64_cpuid_leaf(const CPU *cpu_s, uint32_t leaf)
{
    x64_id_t id;

    x64_get_id(cpu_s, &id);
    x64_cpuid(&id, leaf);
}

int
x64_long_mode_reported(void)
{
    return X64_REPORT_LONG_MODE;
}
