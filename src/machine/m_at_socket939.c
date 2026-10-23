/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Implementation of AMD Socket 939 (K8) machines.
 *
 * Authors: PCBox contributors
 *
 *          Copyright 2026 PCBox contributors
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <86box/86box.h>
#include <86box/mem.h>
#include <86box/io.h>
#include <86box/rom.h>
#include <86box/pci.h>
#include <86box/device.h>
#include <86box/chipset.h>
#include <86box/hdc.h>
#include <86box/hdc_ide.h>
#include <86box/keyboard.h>
#include <86box/flash.h>
#include <86box/sio.h>
#include <86box/spd.h>
#include <86box/video.h>
#include "cpu.h"
#include <86box/machine.h>

int
machine_at_a8v_init(const machine_t *model)
{
    int ret;

    /* ASUS A8V, K8T800 Pro + VT8237, AMI BIOS, 4 Mbit flash. */
    ret = bios_load_linear("roms/machines/a8v/a8v_1017.002",
                           0x000c0000, 524288, 0);

    if (bios_only || !ret)
        return ret;

    machine_at_common_init(model);

    pci_init(PCI_CONFIG_TYPE_1);
    pci_register_bus_slot(0, 0x00, PCI_CARD_NORTHBRIDGE, 0, 0, 0, 0);
    pci_register_bus_slot(0, 0x01, PCI_CARD_AGPBRIDGE,   1, 2, 3, 4);
    pci_register_bus_slot(0, 0x11, PCI_CARD_SOUTHBRIDGE, 1, 2, 3, 4);
    pci_register_bus_slot(1, 0x0a, PCI_CARD_NORMAL,      1, 2, 3, 4);
    pci_register_bus_slot(1, 0x0b, PCI_CARD_NORMAL,      2, 3, 4, 1);
    pci_register_bus_slot(1, 0x0c, PCI_CARD_NORMAL,      3, 4, 1, 2);
    pci_register_bus_slot(1, 0x0d, PCI_CARD_NORMAL,      4, 1, 2, 3);
    pci_register_bus_slot(1, 0x0e, PCI_CARD_NORMAL,      1, 2, 3, 4);

    device_add(&via_k8t800pro_device);       /* VIA K8T800 Pro host bridge */
    device_add(&via_vt8237_device);          /* VIA VT8237 south bridge */
    device_add(&w83627hf_no_port_92_device);
    device_add(&sst_flash_49lf004_device);

    return ret;
}

int
machine_at_k8tneo2_init(const machine_t *model)
{
    int ret;

    /* MSI K8T Neo2-FIR, K8T800 Pro + VT8237, AMI BIOS, 4 Mbit flash. */
    ret = bios_load_linear("roms/machines/k8tneo2/k8tneo2_3b0.rom",
                           0x000c0000, 524288, 0);

    if (bios_only || !ret)
        return ret;

    machine_at_common_init(model);

    pci_init(PCI_CONFIG_TYPE_1);
    pci_register_bus_slot(0, 0x00, PCI_CARD_NORTHBRIDGE, 0, 0, 0, 0);
    pci_register_bus_slot(0, 0x01, PCI_CARD_AGPBRIDGE,   1, 2, 3, 4);
    pci_register_bus_slot(0, 0x11, PCI_CARD_SOUTHBRIDGE, 1, 2, 3, 4);
    pci_register_bus_slot(1, 0x08, PCI_CARD_NORMAL,      1, 2, 3, 4);
    pci_register_bus_slot(1, 0x09, PCI_CARD_NORMAL,      2, 3, 4, 1);
    pci_register_bus_slot(1, 0x0a, PCI_CARD_NORMAL,      3, 4, 1, 2);
    pci_register_bus_slot(1, 0x0b, PCI_CARD_NORMAL,      4, 1, 2, 3);
    pci_register_bus_slot(1, 0x0c, PCI_CARD_NORMAL,      1, 2, 3, 4);

    device_add(&via_k8t800pro_device);
    device_add(&via_vt8237_device);
    device_add(&w83627hf_no_port_92_device);
    device_add(&sst_flash_49lf004_device);

    return ret;
}
