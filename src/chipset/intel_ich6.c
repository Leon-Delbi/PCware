/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Implementation of the Intel ICH6 / ICH6R / ICH7 I/O Controller
 *          Hub (South Bridge) used by LGA775 machines.
 *
 *          Scope of this implementation
 *          ----------------------------
 *          Modelled:
 *            - the LPC interface bridge (device 31, function 0) with the
 *              correct IDs, command/status and class code,
 *            - the IDE controller (function 1) as two SFF-8038i channels,
 *            - the SMBus controller (function 3),
 *            - USB 1.1/2.0 root hubs driving the emulated USB controllers,
 *            - the emulated ACPI power management block, NVR, port 92h and
 *              PCI interrupt routing, reusing the in-tree ICH2 building
 *              blocks whose register interface the ICH6 kept compatible.
 *
 *          Not implemented (documented, not silent): Serial ATA in both
 *          legacy and AHCI modes (this fork has no AHCI core), the High
 *          Definition Audio controller, the integrated Gigabit Ethernet,
 *          and the ICH6 GPIO/trap blocks.  See docs/X64_SUPPORT.md.
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
#include <86box/dma.h>
#include <86box/io.h>
#include <86box/mem.h>
#include <86box/timer.h>
#include <86box/apic.h>
#include <86box/nvr.h>
#include <86box/acpi.h>
#include <86box/pci.h>
#include <86box/pic.h>
#include <86box/port_92.h>
#include <86box/hdc.h>
#include <86box/hdc_ide.h>
#include <86box/hdc_ide_sff8038i.h>
#include <86box/usb.h>
#include <86box/smbus.h>
#include <86box/chipset.h>

#ifdef ENABLE_INTEL_ICH6_LOG
int intel_ich6_do_log = ENABLE_INTEL_ICH6_LOG;

static void
intel_ich6_log(const char *fmt, ...)
{
    va_list ap;

    if (intel_ich6_do_log) {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#else
#    define intel_ich6_log(fmt, ...)
#endif

#define INTEL_ICH6  0 /* 8086:2640 LPC interface */
#define INTEL_ICH6R 1 /* 8086:2640, RAID capable */
#define INTEL_ICH7  2 /* 8086:27B8 */

typedef struct intel_ich6_t {
    uint8_t pci_conf[4][256]; /* 0 = LPC, 1 = IDE, 3 = SMBus */
    uint8_t pci_slot;
    uint8_t variant;
    uint8_t pad[2];

    void   *ide[2];
    void   *usb[2];
    void   *smbus;
    void   *acpi;
    void   *nvr;
} intel_ich6_t;

#define intel_ich6_name(d) \
    (((d)->variant == INTEL_ICH6R) ? "Intel ICH6R" : \
     (((d)->variant == INTEL_ICH7) ? "Intel ICH7" : "Intel ICH6"))

static void
intel_ich6_write(int func, int addr, UNUSED(int len), uint8_t val, void *priv)
{
    intel_ich6_t *dev = (intel_ich6_t *) priv;

    intel_ich6_log("%s: [W] %d:%02X = %02X\n", intel_ich6_name(dev), func, addr, val);

    if (func > 3 || func == 2)
        return;

    switch (addr) {
        case 0x04:
            dev->pci_conf[func][addr] = val & 0x07;
            break;

        case 0x05:
            dev->pci_conf[func][addr] = 0x00;
            break;

        case 0x06:
            dev->pci_conf[func][addr] &= ~(val & 0xf0);
            break;

        case 0x07:
            dev->pci_conf[func][addr] = val & 0x78;
            break;

        default:
            if (func == 1 && ((addr >= 0x40) && (addr <= 0x43))) {
                /* IDE timing / channel enable registers: stored only, the
                 * SFF-8038i core always decodes the legacy command blocks. */
                dev->pci_conf[1][addr] = val;
            } else
                dev->pci_conf[func][addr] = val;
            break;
    }
}

static uint8_t
intel_ich6_read(int func, int addr, UNUSED(int len), void *priv)
{
    const intel_ich6_t *dev = (intel_ich6_t *) priv;
    uint8_t             ret  = 0xff;

    if (func <= 3 && func != 2)
        ret = dev->pci_conf[func][addr];

    intel_ich6_log("%s: [R] %d:%02X = %02X\n", intel_ich6_name(dev), func, addr, ret);

    return ret;
}

static void
intel_ich6_reset(void *priv)
{
    intel_ich6_t *dev = (intel_ich6_t *) priv;
    uint16_t      lpc_did;

    memset(dev->pci_conf, 0x00, sizeof(dev->pci_conf));

    lpc_did = (dev->variant == INTEL_ICH7) ? 0x27b8 : 0x2640;

    /* Function 0: LPC interface bridge. */
    dev->pci_conf[0][0x00] = 0x86;
    dev->pci_conf[0][0x01] = 0x80;
    dev->pci_conf[0][0x02] = lpc_did & 0xff;
    dev->pci_conf[0][0x03] = lpc_did >> 8;
    dev->pci_conf[0][0x04] = 0x07;
    dev->pci_conf[0][0x06] = 0x80;
    dev->pci_conf[0][0x07] = 0x02;
    dev->pci_conf[0][0x08] = (dev->variant == INTEL_ICH7) ? 0x01 : 0x03;
    dev->pci_conf[0][0x0a] = 0x01;
    dev->pci_conf[0][0x0b] = 0x06; /* BCC - ISA bridge */
    dev->pci_conf[0][0x0e] = 0x80; /* multi-function device */

    /* Function 1: IDE. */
    dev->pci_conf[1][0x00] = 0x86;
    dev->pci_conf[1][0x01] = 0x80;
    dev->pci_conf[1][0x02] = (dev->variant == INTEL_ICH7) ? 0x27 : 0x26;
    dev->pci_conf[1][0x03] = (dev->variant == INTEL_ICH7) ? 0xdf : 0x91;
    dev->pci_conf[1][0x04] = 0x05;
    dev->pci_conf[1][0x06] = 0x80;
    dev->pci_conf[1][0x07] = 0x02;
    dev->pci_conf[1][0x08] = 0x05;
    dev->pci_conf[1][0x09] = 0x8a; /* bus master IDE */
    dev->pci_conf[1][0x0a] = 0x01;
    dev->pci_conf[1][0x0b] = 0x01;
    dev->pci_conf[1][0x0e] = 0x80;
    dev->pci_conf[1][0x10] = 0x01;
    dev->pci_conf[1][0x14] = 0x01;
    dev->pci_conf[1][0x18] = 0x01;
    dev->pci_conf[1][0x1c] = 0x01;
    dev->pci_conf[1][0x20] = 0x01;
    dev->pci_conf[1][0x3c] = 0xff;
    dev->pci_conf[1][0x3d] = 0x0b;

    /* Function 3: SMBus controller. */
    dev->pci_conf[3][0x00] = 0x86;
    dev->pci_conf[3][0x01] = 0x80;
    dev->pci_conf[3][0x02] = (dev->variant == INTEL_ICH7) ? 0x3a : 0x20;
    dev->pci_conf[3][0x03] = (dev->variant == INTEL_ICH7) ? 0x28 : 0x26;
    dev->pci_conf[3][0x04] = 0x01;
    dev->pci_conf[3][0x06] = 0x80;
    dev->pci_conf[3][0x07] = 0x02;
    dev->pci_conf[3][0x08] = 0x03;
    dev->pci_conf[3][0x09] = 0x00;
    dev->pci_conf[3][0x0a] = 0x05;
    dev->pci_conf[3][0x0b] = 0x0c;
    dev->pci_conf[3][0x0e] = 0x80;
    dev->pci_conf[3][0x20] = 0x01;
    dev->pci_conf[3][0x3d] = 0x02;

    pic_set_pci();
}

static void
intel_ich6_close(void *priv)
{
    free(priv);
}

static void *
intel_ich6_init(const device_t *info)
{
    intel_ich6_t *dev = (intel_ich6_t *) calloc(1, sizeof(intel_ich6_t));

    dev->variant = (uint8_t) info->local;

    pci_add_card(PCI_ADD_SOUTHBRIDGE, intel_ich6_read, intel_ich6_write, dev, &dev->pci_slot);

    /* ACPI / power management. */
    dev->acpi = device_add(&acpi_intel_ich2_device);
    acpi_set_slot(dev->acpi, dev->pci_slot);

    /* DMA and LPC decoding. */
    dma_alias_set_piix();
    dma_lpc_init();

    /* NVR handler. */
    dev->nvr = device_add_params(&nvr_at_device, (void *) (uintptr_t) NVR_PIIX4);
    acpi_set_nvr(dev->acpi, dev->nvr);

    /* System Control Port A. */
    device_add(&port_92_pci_device);

    /* SMBus. */
    dev->smbus = device_add(&intel_ich2_smbus_device);
    smbus_piix4_get_acpi(dev->smbus, dev->acpi);

    /* IDE: two SFF-8038i channels on the south bridge's INTA. */
    dev->ide[0] = device_add_inst(&sff8038i_device, 1);
    dev->ide[1] = device_add_inst(&sff8038i_device, 2);

    sff_set_slot(dev->ide[0], dev->pci_slot);
    sff_set_slot(dev->ide[1], dev->pci_slot);
    sff_set_irq_mode(dev->ide[0], IRQ_MODE_LEGACY);
    sff_set_irq_mode(dev->ide[1], IRQ_MODE_LEGACY);
    sff_set_irq_pin(dev->ide[0], PCI_INTA);
    sff_set_irq_pin(dev->ide[1], PCI_INTA);

    /* USB root hubs. */
    dev->usb[0] = device_add_inst(&usb_device, 1);
    dev->usb[1] = device_add_inst(&usb_device, 2);

    intel_ich6_reset(dev);

    if (!current_ioapic) {
        device_add(&i82093aa_ioapic_device);
        mem_mapping_disable(&current_ioapic->ioapic_mem_window);
    }

    return dev;
}

const device_t intel_ich6_device = {
    .name          = "Intel ICH6",
    .internal_name = "intel_ich6",
    .flags         = DEVICE_PCI,
    .local         = INTEL_ICH6,
    .init          = intel_ich6_init,
    .close         = intel_ich6_close,
    .reset         = intel_ich6_reset,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};

const device_t intel_ich6r_device = {
    .name          = "Intel ICH6R",
    .internal_name = "intel_ich6r",
    .flags         = DEVICE_PCI,
    .local         = INTEL_ICH6R,
    .init          = intel_ich6_init,
    .close         = intel_ich6_close,
    .reset         = intel_ich6_reset,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};

const device_t intel_ich7_device = {
    .name          = "Intel ICH7",
    .internal_name = "intel_ich7",
    .flags         = DEVICE_PCI,
    .local         = INTEL_ICH7,
    .init          = intel_ich6_init,
    .close         = intel_ich6_close,
    .reset         = intel_ich6_reset,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};
