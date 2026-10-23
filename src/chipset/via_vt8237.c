/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Implementation of the VIA VT8237 / VT8237A South Bridge used by
 *          AMD K8 (Athlon 64 / Athlon 64 X2) machines.
 *
 *          Scope of this implementation
 *          ----------------------------
 *          Modelled:
 *            - PCI-to-ISA bridge (device 17, function 0) with the correct
 *              IDs, command/status, class code and BIST handling,
 *            - two SFF-8038i compatible IDE channels (function 1), sharing
 *              the south bridge's PCI interrupt like the real part does,
 *            - USB 1.1 ports (function 2) driving the emulated USB hubs,
 *            - SMBus host controller and the VIA ACPI power management
 *              block, reusing the existing VT82C686 implementations, whose
 *              register layouts the VT8237 kept compatible,
 *            - the AT keyboard controller, NVR and port 92h hookups.
 *
 *          Not implemented (documented, not silent): Serial ATA (the fork
 *          has no AHCI core at all), the integrated Ethernet MAC/PHY, the
 *          V-Link north bridge interface, and USB 2.0 EHCI.  See
 *          docs/X64_SUPPORT.md.
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
#include <86box/dma.h>
#include <86box/device.h>
#include <86box/io.h>
#include <86box/mem.h>
#include <86box/timer.h>
#include <86box/nvr.h>
#include <86box/pci.h>
#include <86box/pic.h>
#include <86box/plat_unused.h>
#include <86box/port_92.h>
#include <86box/nvr.h>
#include <86box/acpi.h>
#include <86box/hdc.h>
#include <86box/hdc_ide.h>
#include <86box/hdc_ide_sff8038i.h>
#include <86box/keyboard.h>
#include <86box/machine.h>
#include <86box/smbus.h>
#include <86box/usb.h>
#include <86box/chipset.h>

#ifdef ENABLE_VIA_VT8237_LOG
int via_vt8237_do_log = ENABLE_VIA_VT8237_LOG;

static void
via_vt8237_log(const char *fmt, ...)
{
    va_list ap;

    if (via_vt8237_do_log) {
        va_start(ap, fmt);
        pclog_ex(fmt, ap);
        va_end(ap);
    }
}
#else
#    define via_vt8237_log(fmt, ...)
#endif

#define VIA_VT8237  0
#define VIA_VT8237A 1

typedef struct via_vt8237_t {
    uint8_t  pci_conf[3][256]; /* 0 = ISA/PM, 1 = IDE, 2 = USB */
    uint8_t  pci_slot;
    uint8_t  variant;
    uint8_t  pad[2];

    void     *ide[2];
    void     *usb[2];
    void     *smbus;
    void     *acpi;
    void     *nvr;
} via_vt8237_t;

#define via_vt8237_name(d) \
    (((d)->variant == VIA_VT8237A) ? "VIA VT8237A" : "VIA VT8237")

static void
via_vt8237_write(int func, int addr, UNUSED(int len), uint8_t val, void *priv)
{
    via_vt8237_t *dev = (via_vt8237_t *) priv;

    via_vt8237_log("%s: [W] %d:%02X = %02X\n", via_vt8237_name(dev), func, addr, val);

    if (func > 2)
        return;

    switch (func) {
        case 0x00:
            switch (addr) {
                case 0x04:
                    dev->pci_conf[0][addr] = val & 0x0f;
                    break;

                case 0x05:
                    dev->pci_conf[0][addr] = 0x00;
                    break;

                case 0x06:
                    dev->pci_conf[0][addr] &= ~(val & 0xc0);
                    break;

                case 0x07:
                    dev->pci_conf[0][addr] = val & 0x78;
                    break;

                default:
                    dev->pci_conf[0][addr] = val;
                    break;
            }
            break;

        case 0x01:
            switch (addr) {
                case 0x04:
                    dev->pci_conf[1][addr] = val & 0x07;
                    break;

                case 0x05:
                    dev->pci_conf[1][addr] = 0x00;
                    break;

                case 0x09:
                    dev->pci_conf[1][addr] = val & 0x05;
                    break;

                case 0x0d:
                    dev->pci_conf[1][addr] = val & 0xff;
                    break;

                case 0x3c:
                    dev->pci_conf[1][addr] = val & 0xff;
                    break;

                case 0x3d:
                    dev->pci_conf[1][addr] = val & 0x0f;
                    break;

                case 0x41:
                case 0x43:
                    /* Channel enable / native-mode flags.  The SFF-8038i core
                     * in this fork always decodes the legacy command blocks,
                     * so the bits are stored only. */
                    dev->pci_conf[1][addr] = val & 0x83;
                    break;

                default:
                    dev->pci_conf[1][addr] = val;
                    break;
            }
            break;

        case 0x02:
            switch (addr) {
                case 0x04:
                    dev->pci_conf[2][addr] = val & 0x0f;
                    break;

                case 0x06:
                    dev->pci_conf[2][addr] &= ~(val & 0x80);
                    break;

                case 0x07:
                    dev->pci_conf[2][addr] = val & 0x78;
                    break;

                case 0x3d:
                    dev->pci_conf[2][addr] = val & 0x0f;
                    break;

                default:
                    dev->pci_conf[2][addr] = val;
                    break;
            }
            break;
    }
}

static uint8_t
via_vt8237_read(int func, int addr, UNUSED(int len), void *priv)
{
    const via_vt8237_t *dev = (via_vt8237_t *) priv;
    uint8_t             ret = 0xff;

    if (func <= 2)
        ret = dev->pci_conf[func][addr];

    via_vt8237_log("%s: [R] %d:%02X = %02X\n", via_vt8237_name(dev), func, addr, ret);

    return ret;
}

static void
via_vt8237_reset(void *priv)
{
    via_vt8237_t *dev = (via_vt8237_t *) priv;

    memset(dev->pci_conf, 0x00, sizeof(dev->pci_conf));

    /* Function 0: PCI-to-ISA bridge. */
    dev->pci_conf[0][0x00] = 0x06; /* VID - VIA */
    dev->pci_conf[0][0x01] = 0x11;
    dev->pci_conf[0][0x02] = (dev->variant == VIA_VT8237A) ? 0x37 : 0x27; /* DID */
    dev->pci_conf[0][0x03] = (dev->variant == VIA_VT8237A) ? 0x33 : 0x32;
    dev->pci_conf[0][0x04] = 0x0f;
    dev->pci_conf[0][0x06] = 0x80;
    dev->pci_conf[0][0x07] = 0x02;
    dev->pci_conf[0][0x08] = (dev->variant == VIA_VT8237A) ? 0x13 : 0x00; /* RID */
    dev->pci_conf[0][0x0a] = 0x01;
    dev->pci_conf[0][0x0b] = 0x06; /* BCC - ISA bridge */
    dev->pci_conf[0][0x0e] = 0x80; /* header type 0, multi-function */

    /* Function 1: IDE. */
    dev->pci_conf[1][0x00] = 0x06;
    dev->pci_conf[1][0x01] = 0x11;
    if (dev->variant == VIA_VT8237A) {
        dev->pci_conf[1][0x02] = 0x39; /* VT8237A SATA/IDE */
        dev->pci_conf[1][0x03] = 0x05;
    } else {
        dev->pci_conf[1][0x02] = 0x71; /* VT82C571 IDE */
        dev->pci_conf[1][0x03] = 0x05;
    }
    dev->pci_conf[1][0x04] = 0x05;
    dev->pci_conf[1][0x06] = 0x80;
    dev->pci_conf[1][0x07] = 0x02;
    dev->pci_conf[1][0x08] = 0x06;
    dev->pci_conf[1][0x09] = 0x8a; /* programming interface: bus master IDE */
    dev->pci_conf[1][0x0a] = 0x01;
    dev->pci_conf[1][0x0b] = 0x01;
    dev->pci_conf[1][0x0e] = 0x80;
    dev->pci_conf[1][0x10] = 0x01; /* primary command block at 1F0h */
    dev->pci_conf[1][0x14] = 0x01; /* primary control block at 3F0h */
    dev->pci_conf[1][0x18] = 0x01; /* secondary command block at 170h */
    dev->pci_conf[1][0x1c] = 0x01; /* secondary control block at 370h */
    dev->pci_conf[1][0x20] = 0x01; /* bus master registers at E000h */
    dev->pci_conf[1][0x3c] = 0xff;
    dev->pci_conf[1][0x3d] = 0x0b; /* IRQ 14/15 */

    /* Function 2: USB 1.1/2.0 host controller. */
    dev->pci_conf[2][0x00] = 0x06;
    dev->pci_conf[2][0x01] = 0x11;
    dev->pci_conf[2][0x02] = 0x38;
    dev->pci_conf[2][0x03] = 0x30; /* VT83C572/8237 USB */
    dev->pci_conf[2][0x04] = 0x07;
    dev->pci_conf[2][0x06] = 0x80;
    dev->pci_conf[2][0x07] = 0x02;
    dev->pci_conf[2][0x08] = 0x80;
    dev->pci_conf[2][0x09] = 0x00;
    dev->pci_conf[2][0x0a] = 0x03;
    dev->pci_conf[2][0x0b] = 0x0c;
    dev->pci_conf[2][0x0e] = 0x80;
    dev->pci_conf[2][0x20] = 0x01;
    dev->pci_conf[2][0x3d] = 0x0b;

    pic_set_pci();
}

static void
via_vt8237_close(void *priv)
{
    free(priv);
}

static void *
via_vt8237_init(const device_t *info)
{
    via_vt8237_t *dev = (via_vt8237_t *) calloc(1, sizeof(via_vt8237_t));

    dev->variant = (uint8_t) info->local;

    pci_add_card(PCI_ADD_SOUTHBRIDGE, via_vt8237_read, via_vt8237_write, dev, &dev->pci_slot);

    /* ACPI / power management, using the VT82C686 compatible block. */
    dev->acpi = device_add(&acpi_via_device);
    acpi_set_slot(dev->acpi, dev->pci_slot);
    acpi_set_irq_mode(dev->acpi, 0);

    /* DMA aliases and LPC style decoding. */
    dma_alias_set();

    /* NVR handler. */
    dev->nvr = device_add_params(&nvr_at_device, (void *) (uintptr_t) NVR_VIA);
    acpi_set_nvr(dev->acpi, dev->nvr);

    /* System Control Port A. */
    device_add(&port_92_pci_device);

    /* SMBus host controller. */
    dev->smbus = device_add(&via_smbus_device);

    /* IDE: two SFF-8038i channels, both on the south bridge's INTA. */
    dev->ide[0] = device_add_inst(&sff8038i_device, 1);
    dev->ide[1] = device_add_inst(&sff8038i_device, 2);

    sff_set_slot(dev->ide[0], dev->pci_slot);
    sff_set_slot(dev->ide[1], dev->pci_slot);
    sff_set_irq_mode(dev->ide[0], IRQ_MODE_LEGACY);
    sff_set_irq_mode(dev->ide[1], IRQ_MODE_LEGACY);
    sff_set_irq_pin(dev->ide[0], PCI_INTA);
    sff_set_irq_pin(dev->ide[1], PCI_INTA);

    /* USB hubs. */
    dev->usb[0] = device_add_inst(&usb_device, 1);
    dev->usb[1] = device_add_inst(&usb_device, 2);

    via_vt8237_reset(dev);

    return dev;
}

const device_t via_vt8237_device = {
    .name          = "VIA VT8237 South Bridge",
    .internal_name = "via_vt8237",
    .flags         = DEVICE_PCI,
    .local         = VIA_VT8237,
    .init          = via_vt8237_init,
    .close         = via_vt8237_close,
    .reset         = via_vt8237_reset,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};

const device_t via_vt8237a_device = {
    .name          = "VIA VT8237A South Bridge",
    .internal_name = "via_vt8237a",
    .flags         = DEVICE_PCI,
    .local         = VIA_VT8237A,
    .init          = via_vt8237_init,
    .close         = via_vt8237_close,
    .reset         = via_vt8237_reset,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};
