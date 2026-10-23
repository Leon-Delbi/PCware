/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Implementation of the VIA K8T800 / K8T800 Pro / K8T890 Host
 *          Bridge (North Bridge) used by AMD K8 (Athlon 64) machines.
 *
 *          Scope of this implementation
 *          ----------------------------
 *          The x64 era north bridges integrate the DRAM controller and are
 *          reached over HyperTransport, which is not modelled by this fork.
 *          What is modelled here is the part the rest of the emulator needs
 *          and can actually exercise:
 *
 *            - PCI configuration space with the correct vendor/device/
 *              revision IDs, BIST/command/status and class code,
 *            - the DRAM controller's top-of-memory register, so the memory
 *              map of the emulated machine matches the configured RAM size,
 *            - the BIOS shadow (PAM-style) control registers, so option ROMs
 *              and the system BIOS can be shadowed the way real VIA BIOSes
 *              do it,
 *            - the AGP 3.0 bridge for the AGP variants (K8T800/Pro), using
 *              the common VIA AGP target implementation.
 *
 *          Not implemented (documented, not silent): HyperTransport link
 *          configuration, the integrated DRAM controller's timing registers,
 *          DDR SPD auto-sizing, PCI Express root complex (K8T890), and the
 *          integrated V-Link south bridge interface.  See docs/X64_SUPPORT.md.
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
#include <86box/io.h>
#include <86box/mem.h>
#include <86box/pci.h>
#include <86box/plat_unused.h>
#include <86box/chipset.h>

#ifdef ENABLE_VIA_K8T800_LOG
int via_k8t800_do_log = ENABLE_VIA_K8T800_LOG;

static void
via_k8t800_log(const char *fmt, ...)
{
    va_list ap;

    if (via_k8t800_do_log) {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#else
#    define via_k8t800_log(fmt, ...)
#endif

/* North bridge variants. */
#define VIA_K8T800    0 /* VT8382, AGP 8x, Socket 754/939 */
#define VIA_K8T800PRO 1 /* VT8382 rev. 2, AGP 8x, 1 GHz HyperTransport */
#define VIA_K8T890    2 /* VT8238/8251 companion, PCIe x16, Socket 939/AM2 */

typedef struct via_k8t800_t {
    uint8_t  pci_conf[256];
    uint8_t  pci_slot;
    uint8_t  variant;
    uint8_t  pad;
    uint32_t tom; /* top of memory, in bytes */
} via_k8t800_t;

/* Macro rather than a function so that it does not become an unused static
 * function in builds where the logging below is compiled out. */
#define via_k8t800_name(d) \
    (((d)->variant == VIA_K8T800PRO) ? "VIA K8T800 Pro" : \
     (((d)->variant == VIA_K8T890) ? "VIA K8T890" : "VIA K8T800"))

static void
via_k8t800_tom_recalc(via_k8t800_t *dev)
{
    /* DRAM Base/Top registers: 64 MB granularity, in 1 MB units split over
     * two bytes each; we only need the aggregate top of memory. */
    uint32_t tom = ((uint32_t) dev->pci_conf[0x88] << 24) |
                   ((uint32_t) dev->pci_conf[0x87] << 16) |
                   ((uint32_t) dev->pci_conf[0x86] << 8) |
                   (uint32_t) dev->pci_conf[0x85];

    tom &= 0xfc000000;

    /* Keep the register consistent with the configured RAM size: a real
     * BIOS programs it from SPD, and we have no DDR SPD support yet. */
    if ((tom == 0) || (tom > (uint32_t) (mem_size << 10)))
        tom = (uint32_t) (mem_size << 10);

    dev->tom = tom;

    dev->pci_conf[0x85] = tom & 0xff;
    dev->pci_conf[0x86] = (tom >> 8) & 0xff;
    dev->pci_conf[0x87] = (tom >> 16) & 0xff;
    dev->pci_conf[0x88] = (tom >> 24) & 0xff;

    via_k8t800_log("%s: top of memory = %08X\n", via_k8t800_name(dev), tom);
}

static void
via_k8t800_shadow_recalc(via_k8t800_t *dev)
{
    uint32_t addr;

    /* Registers 0x80-0x87 shadow 32 KB blocks from C0000h upwards, register
     * 0x88 bit 0x10 covers the F0000h-FFFFFh block, mirroring the VIA
     * "shadow RAM control" block.  Bit 0x01 = reads from the shadow, bit
     * 0x02 = writes go to the shadow. */
    for (int i = 0; i < 8; i++) {
        const uint8_t val = dev->pci_conf[0x80 + i];

        addr = 0x000c0000 + (i * 0x8000);

        mem_set_mem_state_both(addr, 0x8000,
                               ((val & 0x01) ? MEM_READ_INTERNAL : MEM_READ_EXTANY) |
                               ((val & 0x02) ? MEM_WRITE_INTERNAL : MEM_WRITE_EXTANY));
    }

    mem_set_mem_state_both(0x000f0000, 0x10000,
                           ((dev->pci_conf[0x88] & 0x10) ? MEM_READ_INTERNAL : MEM_READ_EXTANY) |
                           ((dev->pci_conf[0x88] & 0x20) ? MEM_WRITE_INTERNAL : MEM_WRITE_EXTANY));

    flushmmucache();
}

static void
via_k8t800_write(int func, int addr, UNUSED(int len), uint8_t val, void *priv)
{
    via_k8t800_t *dev = (via_k8t800_t *) priv;

    via_k8t800_log("%s: [W] %02X = %02X\n", via_k8t800_name(dev), addr, val);

    if (func)
        return;

    switch (addr) {
        case 0x04:
            dev->pci_conf[addr] = val & 0x07;
            break;

        case 0x05:
            dev->pci_conf[addr] = 0x00;
            break;

        case 0x06:
            dev->pci_conf[addr] &= ~(val & 0xf8);
            break;

        case 0x07:
            dev->pci_conf[addr] &= ~(val & 0x70);
            break;

        case 0x0c:
            dev->pci_conf[addr] = val & 0x08;
            break;

        case 0x0d:
            dev->pci_conf[addr] = val & 0x40;
            break;

        case 0x0e:
            dev->pci_conf[addr] = 0x01;
            break;

        case 0x0f:
            dev->pci_conf[addr] = 0x00;
            break;

        case 0x80 ... 0x87:
            dev->pci_conf[addr] = val & 0x03;
            via_k8t800_shadow_recalc(dev);
            break;

        case 0x88:
            dev->pci_conf[addr] = val & 0x30;
            via_k8t800_tom_recalc(dev);
            via_k8t800_shadow_recalc(dev);
            break;

        default:
            break;
    }
}

static uint8_t
via_k8t800_read(int func, int addr, UNUSED(int len), void *priv)
{
    const via_k8t800_t *dev = (via_k8t800_t *) priv;
    uint8_t             ret;

    if (func)
        ret = 0xff;
    else
        ret = dev->pci_conf[addr];

    via_k8t800_log("%s: [R] %02X = %02X\n", via_k8t800_name(dev), addr, ret);

    return ret;
}

static void
via_k8t800_reset(void *priv)
{
    via_k8t800_t *dev = (via_k8t800_t *) priv;
    uint16_t      did = 0x0282;
    uint8_t       rid = 0x00;

    memset(dev->pci_conf, 0x00, sizeof(dev->pci_conf));

    switch (dev->variant) {
        case VIA_K8T800PRO:
            did = 0x0282;
            rid = 0x02;
            break;

        case VIA_K8T890:
            did = 0x0238;
            rid = 0x00;
            break;

        default:
            did = 0x0282;
            rid = 0x00;
            break;
    }

    dev->pci_conf[0x00] = 0x06; /* VID - VIA */
    dev->pci_conf[0x01] = 0x11;
    dev->pci_conf[0x02] = did & 0xff; /* DID */
    dev->pci_conf[0x03] = did >> 8;
    dev->pci_conf[0x04] = 0x06; /* PCICMD */
    dev->pci_conf[0x06] = 0x20; /* PCISTS */
    dev->pci_conf[0x07] = 0x22;
    dev->pci_conf[0x08] = rid;  /* RID */
    dev->pci_conf[0x0b] = 0x06; /* BCC - host bridge */
    dev->pci_conf[0x0e] = 0x01; /* header type 0, single function */

    dev->tom = 0;
    via_k8t800_tom_recalc(dev);
    via_k8t800_shadow_recalc(dev);
}

static void
via_k8t800_close(void *priv)
{
    free(priv);
}

static void *
via_k8t800_init(const device_t *info)
{
    via_k8t800_t *dev = (via_k8t800_t *) calloc(1, sizeof(via_k8t800_t));

    dev->variant = (uint8_t) info->local;

    /* HyperTransport clocked host bridge; the PCI clock is fixed at 33 MHz
     * as the K8 memory controller is independent of it. */
    cpu_set_pci_speed(33333333);

    pci_add_card(PCI_ADD_NORTHBRIDGE, via_k8t800_read, via_k8t800_write, dev, &dev->pci_slot);

    if (dev->variant != VIA_K8T890) {
        /* AGP 3.0 target: the closest VIA implementation in the tree. */
        cpu_set_agp_speed(66666667);
        device_add(&via_apro_agp_device);
    }

    cpu_cache_int_enabled = 1;
    cpu_cache_ext_enabled = 1;
    cpu_update_waitstates();

    via_k8t800_reset(dev);

    return dev;
}

const device_t via_k8t800_device = {
    .name          = "VIA K8T800 Host Bridge",
    .internal_name = "via_k8t800",
    .flags         = DEVICE_PCI,
    .local         = VIA_K8T800,
    .init          = via_k8t800_init,
    .close         = via_k8t800_close,
    .reset         = via_k8t800_reset,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};

const device_t via_k8t800pro_device = {
    .name          = "VIA K8T800 Pro Host Bridge",
    .internal_name = "via_k8t800pro",
    .flags         = DEVICE_PCI,
    .local         = VIA_K8T800PRO,
    .init          = via_k8t800_init,
    .close         = via_k8t800_close,
    .reset         = via_k8t800_reset,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};

const device_t via_k8t890_device = {
    .name          = "VIA K8T890 Host Bridge",
    .internal_name = "via_k8t890",
    .flags         = DEVICE_PCI,
    .local         = VIA_K8T890,
    .init          = via_k8t800_init,
    .close         = via_k8t800_close,
    .reset         = via_k8t800_reset,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};
