/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Implementation of Intel LGA775 machines (Pentium 4 EM64T,
 *          Pentium D and Core 2 with Intel 9xx chipsets).
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
machine_at_d915pbl_init(const machine_t *model)
{
    int ret;

    /* Intel Desktop Board D915PBL, Intel BIOS, 4 Mbit flash. */
    ret = bios_load_linear("roms/machines/d915pbl/PB91510A.86A.0075.BIN",
                           0x000c0000, 524288, 0);

    if (bios_only || !ret)
        return ret;

    machine_at_common_init(model);

    pci_init(PCI_CONFIG_TYPE_1);
    pci_register_bus_slot(0, 0x00, PCI_CARD_NORTHBRIDGE, 0, 0, 0, 0); /* 915P MCH */
    pci_register_bus_slot(0, 0x1f, PCI_CARD_SOUTHBRIDGE,  1, 2, 3, 4); /* ICH6 */
    pci_register_bus_slot(1, 0x01, PCI_CARD_NORMAL,       1, 2, 3, 4);
    pci_register_bus_slot(1, 0x02, PCI_CARD_NORMAL,       2, 3, 4, 1);
    pci_register_bus_slot(1, 0x03, PCI_CARD_NORMAL,       3, 4, 1, 2);
    pci_register_bus_slot(1, 0x04, PCI_CARD_NORMAL,       4, 1, 2, 3);

    device_add(&intel_915p_device);          /* Intel 82915P MCH */
    device_add(&intel_ich6_device);          /* Intel ICH6 */
    device_add(&w83627hf_no_port_92_device);
    device_add(&sst_flash_49lf004_device);

    return ret;
}

int
machine_at_p5gd2_init(const machine_t *model)
{
    int ret;

    /* ASUS P5GD2 Deluxe, AMI BIOS, 4 Mbit flash. */
    ret = bios_load_linear("roms/machines/p5gd2/P5GD2_1004.001",
                           0x000c0000, 524288, 0);

    if (bios_only || !ret)
        return ret;

    machine_at_common_init(model);

    pci_init(PCI_CONFIG_TYPE_1);
    pci_register_bus_slot(0, 0x00, PCI_CARD_NORTHBRIDGE, 0, 0, 0, 0);
    pci_register_bus_slot(0, 0x1f, PCI_CARD_SOUTHBRIDGE,  1, 2, 3, 4);
    pci_register_bus_slot(1, 0x01, PCI_CARD_NORMAL,       1, 2, 3, 4);
    pci_register_bus_slot(1, 0x02, PCI_CARD_NORMAL,       2, 3, 4, 1);
    pci_register_bus_slot(1, 0x03, PCI_CARD_NORMAL,       3, 4, 1, 2);
    pci_register_bus_slot(1, 0x04, PCI_CARD_NORMAL,       4, 1, 2, 3);
    pci_register_bus_slot(1, 0x05, PCI_CARD_NORMAL,       1, 2, 3, 4);

    device_add(&intel_915p_device);
    device_add(&intel_ich6r_device);
    device_add(&w83627hf_no_port_92_device);
    device_add(&sst_flash_49lf004_device);

    return ret;
}

int
machine_at_d945psn_init(const machine_t *model)
{
    int ret;

    /* Intel Desktop Board D945PSN, Intel BIOS, 4 Mbit flash. */
    ret = bios_load_linear("roms/machines/d945psn/PS94510J.86A.0207.BIN",
                           0x000c0000, 524288, 0);

    if (bios_only || !ret)
        return ret;

    machine_at_common_init(model);

    pci_init(PCI_CONFIG_TYPE_1);
    pci_register_bus_slot(0, 0x00, PCI_CARD_NORTHBRIDGE, 0, 0, 0, 0); /* 945P MCH */
    pci_register_bus_slot(0, 0x1f, PCI_CARD_SOUTHBRIDGE,  1, 2, 3, 4); /* ICH7 */
    pci_register_bus_slot(1, 0x01, PCI_CARD_NORMAL,       1, 2, 3, 4);
    pci_register_bus_slot(1, 0x02, PCI_CARD_NORMAL,       2, 3, 4, 1);
    pci_register_bus_slot(1, 0x03, PCI_CARD_NORMAL,       3, 4, 1, 2);

    device_add(&intel_945p_device);
    device_add(&intel_ich7_device);
    device_add(&w83627hf_no_port_92_device);
    device_add(&sst_flash_49lf004_device);

    return ret;
}
