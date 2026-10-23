/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Implementation of the Intel 82915P/82915G/82945P/82945G Memory
 *          Controller Hub (MCH) used by LGA775 (Pentium 4 EM64T, Pentium D,
 *          Core 2) machines.
 *
 *          Scope of this implementation
 *          ----------------------------
 *          Modelled, following the 82845 MCH already in the tree:
 *            - PCI configuration space (device 0, function 0) with the real
 *              vendor/device/revision IDs for the 915P, 915G, 945P and 945G,
 *            - the TOLM (top of low memory) register, so the memory map
 *              follows the configured RAM size,
 *            - the PAM registers (90000h-9FFFFh is covered by the FDHC
 *              register at 97h), giving working BIOS/option ROM shadowing,
 *            - SMRAM/ESMRAMC with the TSEG (top of memory segment) window,
 *            - the MCHBAR register shadow block.
 *
 *          Not implemented (documented, not silent): the PCI Express root
 *          complex and its root ports, the DDR2 DRAM controller timing
 *          registers, Turbo Memory, and the integrated graphics core of the
 *          G variants.  Boards using this MCH therefore have working PCI
 *          slots but no PCIe slots.  See docs/X64_SUPPORT.md.
 *
 * Authors: PCBox contributors
 *
 *          Copyright 2026 PCBox contributors
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include <86box/86box.h>
#include "cpu.h"
#include <86box/device.h>
#include <86box/plat_unused.h>
#include <86box/chipset.h>
#include <86box/mem.h>
#include <86box/pci.h>
#include <86box/smram.h>

#ifdef ENABLE_INTEL_915_LOG
int intel_915_do_log = ENABLE_INTEL_915_LOG;

static void
intel_915_log(const char *fmt, ...)
{
    va_list ap;

    if (intel_915_do_log) {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#else
#    define intel_915_log(fmt, ...)
#endif

/* MCH variants: local = DID index. */
#define INTEL_915P 0 /* 8086:2580 */
#define INTEL_915G 1 /* 8086:2582 */
#define INTEL_945P 2 /* 8086:2774 */
#define INTEL_945G 3 /* 8086:2776 */

typedef struct intel_915_t {
    uint8_t       pci_conf[256];
    uint8_t       mchbar_regs[0x100];
    uint8_t       pci_slot;
    uint8_t       variant;
    uint8_t       subsystem_locked[2];
    uint32_t      tseg_base;
    uint32_t      tseg_size;
    smram_t      *c_segment;
    smram_t      *h_segment;
    smram_t      *tseg_segment;
    mem_mapping_t mchbar_mapping;
} intel_915_t;

static const uint16_t intel_915_dids[4] = { 0x2580, 0x2582, 0x2774, 0x2776 };
static const uint8_t  intel_915_rids[4] = { 0x04, 0x04, 0x02, 0x02 };

static uint32_t
intel_915_tolm(const intel_915_t *dev)
{
    const uint16_t tolm = (dev->pci_conf[0xc4] | (dev->pci_conf[0xc5] << 8)) & 0xfff0;

    return (uint32_t) tolm << 16;
}

static uint16_t
intel_915_default_tolm(void)
{
    const uint16_t tolm = (mem_size >> 6) & 0xfff0;

    return (tolm < 0x0200) ? 0x0200 : tolm;
}

static void
intel_915_set_tolm(intel_915_t *dev, uint16_t tolm)
{
    dev->pci_conf[0xc4] = tolm & 0xff;
    dev->pci_conf[0xc5] = tolm >> 8;
}

static void
intel_915_pam_recalc(int addr, uint8_t val)
{
    int region = 0xc0000 + ((addr - 0x91) << 15);

    if (addr == 0x90)
        mem_set_mem_state_both(0xf0000, 0x10000,
                               ((val & 0x10) ? MEM_READ_INTERNAL : MEM_READ_EXTANY) |
                               ((val & 0x20) ? MEM_WRITE_INTERNAL : MEM_WRITE_EXTANY));
    else {
        mem_set_mem_state_both(region, 0x4000,
                               ((val & 0x01) ? MEM_READ_INTERNAL : MEM_READ_EXTANY) |
                               ((val & 0x02) ? MEM_WRITE_INTERNAL : MEM_WRITE_EXTANY));
        mem_set_mem_state_both(region + 0x4000, 0x4000,
                               ((val & 0x10) ? MEM_READ_INTERNAL : MEM_READ_EXTANY) |
                               ((val & 0x20) ? MEM_WRITE_INTERNAL : MEM_WRITE_EXTANY));
    }

    flushmmucache();
}

static void
intel_915_fdhc_recalc(intel_915_t *dev)
{
    if ((mem_size << 10) > 0xf00000) {
        mem_set_mem_state_both(0xf00000, 0x100000,
                               (dev->pci_conf[0x97] & 0x80) ?
                               (MEM_READ_EXTANY | MEM_WRITE_EXTANY) :
                               (MEM_READ_INTERNAL | MEM_WRITE_INTERNAL));
        flushmmucache();
    }
}

static void
intel_915_smram_recalc(intel_915_t *dev)
{
    const uint8_t smram    = dev->pci_conf[0x9d];
    const uint8_t esmramc  = dev->pci_conf[0x9e];
    const int     g_smrame = !!(smram & 0x08);
    const int     d_open   = ((smram & 0x50) == 0x40);
    const int     d_closed = !!(smram & 0x20);
    uint32_t      tolm     = intel_915_tolm(dev);

    if (dev->tseg_size != 0) {
        mem_set_mem_state_both(dev->tseg_base, dev->tseg_size, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
        dev->tseg_base = dev->tseg_size = 0;
    }

    smram_disable(dev->c_segment);
    smram_disable(dev->h_segment);
    smram_disable(dev->tseg_segment);

    if ((tolm == 0) || (tolm > (uint32_t) (mem_size << 10)))
        tolm = (uint32_t) (mem_size << 10);

    if (g_smrame) {
        smram_enable(dev->c_segment, 0x000a0000, 0x000a0000, 0x20000, d_open, 1);

        if (d_closed)
            mem_set_mem_state_smram_ex(1, 0x000a0000, 0x20000, ACCESS_SMRAM_W);

        if (esmramc & 0x80)
            smram_enable(dev->h_segment, 0xfeda0000, 0x000a0000, 0x20000, d_open, 1);

        if ((esmramc & 0x01) && (tolm != 0)) {
            dev->tseg_size = 1 << (17 + ((esmramc >> 1) & 0x03));

            if (tolm >= dev->tseg_size) {
                dev->tseg_base = tolm - dev->tseg_size;
                smram_enable(dev->tseg_segment, dev->tseg_base, dev->tseg_base, dev->tseg_size, d_open, 1);
            } else
                dev->tseg_base = dev->tseg_size = 0;
        }
    }

    flushmmucache();
}

static uint32_t
intel_915_mchbar_base(const intel_915_t *dev)
{
    return ((dev->pci_conf[0x17] << 24) | (dev->pci_conf[0x16] << 16) |
            (dev->pci_conf[0x15] << 8) | dev->pci_conf[0x14]) & 0xfffffe00;
}

static void
intel_915_mchbar_recalc(intel_915_t *dev)
{
    const uint32_t base = intel_915_mchbar_base(dev);

    if (base != 0) {
        intel_915_log("Intel 915 MCH: MCHBAR enabled at %08X\n", base);
        mem_mapping_set_addr(&dev->mchbar_mapping, base, 0x400);
    } else
        mem_mapping_disable(&dev->mchbar_mapping);
}

static uint8_t
intel_915_mchbar_readb(uint32_t addr, void *priv)
{
    const intel_915_t *dev = (intel_915_t *) priv;

    return dev->mchbar_regs[addr & 0xff];
}

static uint16_t
intel_915_mchbar_readw(uint32_t addr, void *priv)
{
    return intel_915_mchbar_readb(addr, priv) | (intel_915_mchbar_readb(addr + 1, priv) << 8);
}

static uint32_t
intel_915_mchbar_readl(uint32_t addr, void *priv)
{
    return intel_915_mchbar_readw(addr, priv) | (intel_915_mchbar_readw(addr + 2, priv) << 16);
}

static void
intel_915_mchbar_writeb(uint32_t addr, uint8_t val, void *priv)
{
    intel_915_t *dev = (intel_915_t *) priv;

    dev->mchbar_regs[addr & 0xff] = val;
}

static void
intel_915_mchbar_writew(uint32_t addr, uint16_t val, void *priv)
{
    intel_915_mchbar_writeb(addr, val & 0xff, priv);
    intel_915_mchbar_writeb(addr + 1, val >> 8, priv);
}

static void
intel_915_mchbar_writel(uint32_t addr, uint32_t val, void *priv)
{
    intel_915_mchbar_writew(addr, val & 0xffff, priv);
    intel_915_mchbar_writew(addr + 2, val >> 16, priv);
}

static void
intel_915_write(int func, int addr, UNUSED(int len), uint8_t val, void *priv)
{
    intel_915_t *dev = (intel_915_t *) priv;

    intel_915_log("Intel 915 MCH: [W] %02X = %02X\n", addr, val);

    if (func)
        return;

    switch (addr) {
        case 0x05:
            dev->pci_conf[addr] = val & 0x01;
            break;

        case 0x07:
            dev->pci_conf[addr] &= ~(val & 0x70);
            break;

        case 0x14:
            dev->pci_conf[addr] = 0x00;
            intel_915_mchbar_recalc(dev);
            break;

        case 0x15:
            dev->pci_conf[addr] = val & 0xfe;
            intel_915_mchbar_recalc(dev);
            break;

        case 0x16:
        case 0x17:
            dev->pci_conf[addr] = val;
            intel_915_mchbar_recalc(dev);
            break;

        case 0x2c ... 0x2f:
            /* Subsystem ID: write-once, locked until reset. */
            if (!dev->subsystem_locked[addr - 0x2c]) {
                dev->pci_conf[addr] = val;
                dev->subsystem_locked[addr - 0x2c] = 1;
            }
            break;

        case 0x90:
            dev->pci_conf[addr] = val & 0x30;
            intel_915_pam_recalc(addr, dev->pci_conf[addr]);
            break;

        case 0x91 ... 0x96:
            dev->pci_conf[addr] = val & 0x33;
            intel_915_pam_recalc(addr, dev->pci_conf[addr]);
            break;

        case 0x97:
            dev->pci_conf[addr] = val & 0x80;
            intel_915_fdhc_recalc(dev);
            break;

        case 0x9d:
            dev->pci_conf[addr] = (val & 0x78) | 0x02;
            intel_915_smram_recalc(dev);
            break;

        case 0x9e:
            dev->pci_conf[addr] = (val & 0x87) | 0x38;
            intel_915_smram_recalc(dev);
            break;

        case 0xc4:
            dev->pci_conf[addr] = val & 0xf0;
            intel_915_smram_recalc(dev);
            break;

        case 0xc5:
            dev->pci_conf[addr] = val;
            intel_915_smram_recalc(dev);
            break;

        default:
            break;
    }
}

static uint8_t
intel_915_read(int func, int addr, UNUSED(int len), void *priv)
{
    const intel_915_t *dev = (intel_915_t *) priv;
    uint8_t            ret;

    if (func)
        ret = 0xff;
    else
        ret = dev->pci_conf[addr];

    intel_915_log("Intel 915 MCH: [R] %02X = %02X\n", addr, ret);

    return ret;
}

static void
intel_915_reset(void *priv)
{
    intel_915_t *dev = (intel_915_t *) priv;
    const uint16_t did = intel_915_dids[dev->variant & 3];

    if (dev->tseg_size != 0) {
        mem_set_mem_state_both(dev->tseg_base, dev->tseg_size, MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
        dev->tseg_base = dev->tseg_size = 0;
    }

    memset(dev->pci_conf, 0x00, sizeof(dev->pci_conf));
    memset(dev->mchbar_regs, 0x00, sizeof(dev->mchbar_regs));
    memset(dev->subsystem_locked, 0x00, sizeof(dev->subsystem_locked));

    dev->pci_conf[0x00] = 0x86; /* VID - Intel */
    dev->pci_conf[0x01] = 0x80;
    dev->pci_conf[0x02] = did & 0xff; /* DID */
    dev->pci_conf[0x03] = did >> 8;
    dev->pci_conf[0x04] = 0x06; /* PCICMD */
    dev->pci_conf[0x06] = 0x90; /* PCISTS */
    dev->pci_conf[0x08] = intel_915_rids[dev->variant & 3]; /* RID */
    dev->pci_conf[0x0b] = 0x06; /* BCC - host bridge */
    dev->pci_conf[0x0e] = 0x00; /* header type 0, single function */
    dev->pci_conf[0x78] = 0x10;
    dev->pci_conf[0x9d] = 0x02; /* SMRAM */
    dev->pci_conf[0x9e] = 0x38; /* ESMRAMC */

    intel_915_set_tolm(dev, intel_915_default_tolm());

    intel_915_mchbar_recalc(dev);

    for (int i = 0x90; i <= 0x96; i++)
        intel_915_pam_recalc(i, dev->pci_conf[i]);

    intel_915_fdhc_recalc(dev);
    intel_915_smram_recalc(dev);
}

static void
intel_915_close(void *priv)
{
    intel_915_t *dev = (intel_915_t *) priv;

    smram_del(dev->c_segment);
    smram_del(dev->h_segment);
    smram_del(dev->tseg_segment);
    mem_mapping_disable(&dev->mchbar_mapping);
    free(dev);
}

static void *
intel_915_init(const device_t *info)
{
    intel_915_t *dev = (intel_915_t *) calloc(1, sizeof(intel_915_t));

    dev->variant = (uint8_t) info->local;

    /* LGA775 front side bus machines run their PCI at 33 MHz. */
    cpu_set_pci_speed(33333333);

    pci_add_card(PCI_ADD_NORTHBRIDGE, intel_915_read, intel_915_write, dev, &dev->pci_slot);

    mem_mapping_add(&dev->mchbar_mapping, 0, 0,
                    intel_915_mchbar_readb,
                    intel_915_mchbar_readw,
                    intel_915_mchbar_readl,
                    intel_915_mchbar_writeb,
                    intel_915_mchbar_writew,
                    intel_915_mchbar_writel,
                    NULL, MEM_MAPPING_EXTERNAL, dev);

    cpu_cache_int_enabled = 1;
    cpu_cache_ext_enabled = 1;
    cpu_update_waitstates();

    dev->c_segment    = smram_add();
    dev->h_segment    = smram_add();
    dev->tseg_segment = smram_add();

    intel_915_reset(dev);

    return dev;
}

const device_t intel_915p_device = {
    .name          = "Intel 82915P MCH",
    .internal_name = "intel_915p",
    .flags         = DEVICE_PCI,
    .local         = INTEL_915P,
    .init          = intel_915_init,
    .close         = intel_915_close,
    .reset         = intel_915_reset,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};

const device_t intel_915g_device = {
    .name          = "Intel 82915G MCH",
    .internal_name = "intel_915g",
    .flags         = DEVICE_PCI,
    .local         = INTEL_915G,
    .init          = intel_915_init,
    .close         = intel_915_close,
    .reset         = intel_915_reset,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};

const device_t intel_945p_device = {
    .name          = "Intel 82945P MCH",
    .internal_name = "intel_945p",
    .flags         = DEVICE_PCI,
    .local         = INTEL_945P,
    .init          = intel_915_init,
    .close         = intel_915_close,
    .reset         = intel_915_reset,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};

const device_t intel_945g_device = {
    .name          = "Intel 82945G MCH",
    .internal_name = "intel_945g",
    .flags         = DEVICE_PCI,
    .local         = INTEL_945G,
    .init          = intel_915_init,
    .close         = intel_915_close,
    .reset         = intel_915_reset,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};
