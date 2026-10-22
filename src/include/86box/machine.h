/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Handling of the emulated machines.
 *
 * Authors: Sarah Walker, <https://pcem-emulator.co.uk/>
 *          Miran Grca, <mgrca8@gmail.com>
 *          Fred N. van Kempen, <decwiz@yahoo.com>
 *          Jasmine Iwanek, <jriwanek@gmail.com>
 *
 *          Copyright 2008-2020 Sarah Walker.
 *          Copyright 2016-2025 Miran Grca.
 *          Copyright 2017-2020 Fred N. van Kempen.
 *          Copyright 2025      Jasmine Iwanek.
 */
#ifndef EMU_MACHINE_H
#define EMU_MACHINE_H

/* Machine feature flags. */
#define MACHINE_BUS_NONE      0x00000000 /* sys has no bus */
/* Feature flags for BUS'es. */
#define MACHINE_BUS_CASSETTE  0x00000001 /* sys has cassette port */
#define MACHINE_BUS_SIDECAR   0x00000002 /* sys has PCjr sidecar bus */
#define MACHINE_BUS_ISA       0x00000004 /* sys has ISA bus */
#define MACHINE_BUS_XT_KBD    0x00000008 /* sys has an XT keyboard port */
#define MACHINE_BUS_CBUS      0x00000010 /* sys has C-BUS bus */
#define MACHINE_BUS_ISA16     0x00000020 /* sys has ISA16 bus - PC/AT architecture */
#define MACHINE_BUS_AT_KBD    0x00000040 /* sys has an AT keyboard port */
#define MACHINE_BUS_MCA       0x00000080 /* sys has MCA bus */
#define MACHINE_BUS_MCA32     0x00000100 /* sys has MCA32 bus */
#define MACHINE_BUS_PS2_PORTS 0x00000200 /* system has PS/2 keyboard and mouse ports */
#define MACHINE_BUS_PS2       MACHINE_BUS_PS2_PORTS
#define MACHINE_BUS_PCMCIA    0x00000400 /* sys has PCMCIA bus */
#define MACHINE_BUS_HIL       0x00000800 /* system has HP HIL keyboard and mouse ports */
#define MACHINE_BUS_EISA      0x00001000 /* sys has EISA bus */
#define MACHINE_BUS_AT32      0x00002000 /* sys has Mylex AT/32 local bus */
#define MACHINE_BUS_OLB       0x00004000 /* sys has OPTi local bus */
#define MACHINE_BUS_VLB       0x00008000 /* sys has VL bus */
#define MACHINE_BUS_PCI       0x00010000 /* sys has PCI bus */
#define MACHINE_BUS_CARDBUS   0x00020000 /* sys has CardBus bus */
#define MACHINE_BUS_USB       0x00040000 /* sys has USB bus */
#define MACHINE_BUS_AGP       0x00080000 /* sys has AGP bus */
#define MACHINE_BUS_AC97      0x00100000 /* sys has AC97 bus (ACR/AMR/CNR slot) */
/* Aliases. */
#define MACHINE_CASSETTE    (MACHINE_BUS_CASSETTE) /* sys has cassette port */
/* Combined flags. */
#define MACHINE_PC          (MACHINE_BUS_ISA)                     /* sys is PC/XT-compatible (ISA) */
#define MACHINE_AT          (MACHINE_BUS_ISA | MACHINE_BUS_ISA16) /* sys is AT-compatible (ISA + ISA16) */
#define MACHINE_PC98        (MACHINE_BUS_CBUS)                    /* sys is NEC PC-98x1 series */
#define MACHINE_EISA        (MACHINE_BUS_EISA | MACHINE_AT)       /* sys is AT-compatible with EISA */
#define MACHINE_VLB         (MACHINE_BUS_VLB | MACHINE_AT)        /* sys is AT-compatible with VLB */
#define MACHINE_VLB98       (MACHINE_BUS_VLB | MACHINE_PC98)      /* sys is NEC PC-98x1 series with VLB (did that even exist?) */
#define MACHINE_VLBE        (MACHINE_BUS_VLB | MACHINE_EISA)      /* sys is AT-compatible with EISA and VLB */
#define MACHINE_MCA         (MACHINE_BUS_MCA)                     /* sys is MCA */
#define MACHINE_PCI         (MACHINE_BUS_PCI | MACHINE_AT)        /* sys is AT-compatible with PCI */
#define MACHINE_PCI98       (MACHINE_BUS_PCI | MACHINE_PC98)      /* sys is NEC PC-98x1 series with PCI */
#define MACHINE_PCIE        (MACHINE_BUS_PCI | MACHINE_EISA)      /* sys is AT-compatible with PCI, and EISA */
#define MACHINE_PCIV        (MACHINE_BUS_PCI | MACHINE_VLB)       /* sys is AT-compatible with PCI and VLB */
#define MACHINE_PCIVE       (MACHINE_BUS_PCI | MACHINE_VLBE)      /* sys is AT-compatible with PCI, VLB, and EISA */
#define MACHINE_PCMCIA      (MACHINE_BUS_PCMCIA | MACHINE_AT)     /* sys is AT-compatible laptop with PCMCIA */
#define MACHINE_AGP         (MACHINE_BUS_AGP | MACHINE_PCI)       /* sys is AT-compatible with AGP  */
#define MACHINE_AGP98       (MACHINE_BUS_AGP | MACHINE_PCI98)     /* sys is NEC PC-98x1 series with AGP (did that even exist?) */

#define MACHINE_PC5150      (MACHINE_CASSETTE | MACHINE_PC)          /* sys is IBM PC 5150 */
#define MACHINE_PCJR        (MACHINE_CASSETTE | MACHINE_BUS_SIDECAR) /* sys is PCjr */
#define MACHINE_PS2         (MACHINE_AT | MACHINE_BUS_PS2)           /* sys is PS/2 */
#define MACHINE_PS2_MCA     (MACHINE_MCA | MACHINE_BUS_PS2)          /* sys is MCA PS/2 */
#define MACHINE_PS2_VLB     (MACHINE_VLB | MACHINE_BUS_PS2)          /* sys is VLB PS/2 */
#define MACHINE_PS2_PCI     (MACHINE_PCI | MACHINE_BUS_PS2)          /* sys is PCI PS/2 */
#define MACHINE_PS2_PCIV    (MACHINE_PCIV | MACHINE_BUS_PS2)         /* sys is VLB/PCI PS/2 */
#define MACHINE_PS2_AGP     (MACHINE_AGP | MACHINE_BUS_PS2)          /* sys is AGP PS/2 */
#define MACHINE_PS2_A97     (MACHINE_PS2_AGP | MACHINE_BUS_AC97)     /* sys is AGP/AC97 PS/2 */
#define MACHINE_PS2_NOISA   (MACHINE_PS2_AGP & ~MACHINE_AT)          /* sys is AGP PS/2 without ISA */
#define MACHINE_PS2_PCIONLY (MACHINE_PS2_NOISA & ~MACHINE_BUS_AGP)   /* sys is PCI PS/2 without ISA */
#define MACHINE_PS2_NOI97   (MACHINE_PS2_A97 & ~MACHINE_AT)          /* sys is AGP/AC97 PS/2 without ISA */

/* Feature flags for miscellaneous internal devices. */
#define MACHINE_FLAGS_NONE        0x0000000000000000ULL /* sys has no int devices */
#define MACHINE_SOFTFLOAT_ONLY    0x0000000000000001ULL /* sys requires SoftFloat FPU */
#define MACHINE_VIDEO             0x0000000000000002ULL /* sys has int video */
#define MACHINE_VIDEO_EXT         0x0000000000000004ULL /* sys has ext display */
#define MACHINE_VIDEO_ONLY        0x0000000000000008ULL /* sys has fixed video */
#define MACHINE_KEYBOARD          0x0000000000000010ULL /* sys has int keyboard */
#define MACHINE_AX                0x0000000000000020ULL /* sys adheres to Japanese AX standard */
#define MACHINE_KEYBOARD_JIS      0x0000000000000020ULL /* sys has int keyboard which is Japanese (AX or PS/55) */
#define MACHINE_MOUSE             0x0000000000000040ULL /* sys has int mouse */
#define MACHINE_FDC               0x0000000000000080ULL /* sys has int FDC */
#define MACHINE_LPT_PRI           0x0000000000000100ULL /* sys has int pri LPT */
#define MACHINE_LPT_SEC           0x0000000000000200ULL /* sys has int sec LPT */
#define MACHINE_LPT_TER           0x0000000000000400ULL /* sys has int ter LPT */
#define MACHINE_PS2_KBC           0x0000000000000800ULL /* sys has a PS/2 keyboard controller */
                                                         /* this is separate from having PS/2 ports */
#define MACHINE_UART_PRI          0x0000000000010800ULL /* sys has int pri UART */
#define MACHINE_UART_SEC          0x0000000000002000ULL /* sys has int sec UART */
#define MACHINE_UART_TER          0x0000000000004000ULL /* sys has int ter UART */
#define MACHINE_UART_QUA          0x0000000000008000ULL /* sys has int qua UART */
#define MACHINE_GAMEPORT          0x0000000000010000ULL /* sys has int game port */
#define MACHINE_SOUND             0x0000000000020000ULL /* sys has int sound */
#define MACHINE_NIC_PRI           0x0000000000040000ULL /* sys has int pri NIC */
#define MACHINE_NIC_SEC           0x0000000400000000ULL /* sys has int sec NIC */
/* Feature flags for advanced devices. */
#define MACHINE_APM               0x0000000000080000ULL /* sys has APM */
#define MACHINE_ACPI              0x0000000000100000ULL /* sys has ACPI */
#define MACHINE_PCI_INTERNAL      0x0000000000200000ULL /* sys has only internal PCI */
#define MACHINE_AGP_INTERNAL      0x0000000200000000ULL /* sys has only internal AGP */
#define MACHINE_CARTRIDGE         0x0000000000400000ULL /* sys has cartridge bays */
/* Feature flags for internal storage controllers. */
#define MACHINE_MFM               0x0000000000800000ULL /* sys has int MFM/RLL */
#define MACHINE_XTA               0x0000000001000000ULL /* sys has int XTA */
#define MACHINE_ESDI              0x0000000002000000ULL /* sys has int ESDI */
#define MACHINE_IDE_PRI           0x0000000004000000ULL /* sys has int pri IDE/ATAPI */
#define MACHINE_IDE_SEC           0x0000000008000000ULL /* sys has int sec IDE/ATAPI */
#define MACHINE_IDE_TER           0x0000000010000000ULL /* sys has int ter IDE/ATAPI */
#define MACHINE_IDE_QUA           0x0000000020000000ULL /* sys has int qua IDE/ATAPI */
#define MACHINE_SCSI              0x0000000040000000ULL /* sys has int SCSI */
#define MACHINE_USB               0x0000000080000000ULL /* sys has int USB */
#define MACHINE_ZENITH            0x0000000100000000ULL /* sys is Zenith */
#define MACHINE_COREBOOT          0x0000000200000000ULL /* sys has coreboot BIOS */
#define MACHINE_IDE_INTERNAL      0x0000000800000000ULL /* sys int IDE only with the Internal controller */
/* Combined flags. */
#define MACHINE_LPT               (MACHINE_LPT_PRI | MACHINE_LPT_SEC | \
                                    MACHINE_LPT_TER | MACHINE_LPT_QUA)
#define MACHINE_UART              (MACHINE_UART_PRI | MACHINE_UART_SEC | \
                                    MACHINE_UART_TER | MACHINE_UART_QUA)
#define MACHINE_VIDEO_FIXED       (MACHINE_VIDEO | MACHINE_VIDEO_ONLY) /* sys has fixed int video */
#define MACHINE_VIDEO_PANEL       (MACHINE_VIDEO_FIXED | MACHINE_VIDEO_EXT) /* sys can drive ext display */
#define MACHINE_SUPER_IO          (MACHINE_FDC | MACHINE_LPT_PRI | MACHINE_UART_PRI | MACHINE_UART_SEC)
#define MACHINE_SUPER_IO_GAME     (MACHINE_SUPER_IO | MACHINE_GAMEPORT)
#define MACHINE_SUPER_IO_DUAL     (MACHINE_SUPER_IO | MACHINE_LPT_SEC | \
                                    MACHINE_UART_TER | MACHINE_UART_QUA)
#define MACHINE_AV                (MACHINE_VIDEO | MACHINE_SOUND)    /* sys has video and sound */
#define MACHINE_AG                (MACHINE_SOUND | MACHINE_GAMEPORT) /* sys has sound and game port */
#define MACHINE_NIC               (MACHINE_NIC_PRI)
/* Combined flag for internal storage controllerss. */
#define MACHINE_IDE               (MACHINE_IDE_PRI) /* sys has int single IDE/ATAPI - mark as pri IDE/ATAPI */
#define MACHINE_IDE_DUAL          (MACHINE_IDE_PRI | MACHINE_IDE_SEC) /* sys has int dual IDE/ATAPI - mark as both pri and sec IDE/ATAPI */
#define MACHINE_IDE_DUALTQ        (MACHINE_IDE_TER | MACHINE_IDE_QUA)
#define MACHINE_IDE_QUAD          (MACHINE_IDE_DUAL | MACHINE_IDE_DUALTQ) /* sys has int quad IDE/ATAPI - mark as dual + both ter and and qua IDE/ATAPI */
#define MACHINE_HDC               (MACHINE_MFM | MACHINE_XTA | \
                                    MACHINE_ESDI | MACHINE_IDE_QUAD | \
                                    MACHINE_SCSI | MACHINE_USB)
/* Special combined flags. */
#define MACHINE_PIIX              (MACHINE_IDE_DUAL)
#define MACHINE_PIIX3             (MACHINE_PIIX | MACHINE_USB)
#define MACHINE_PIIX4             (MACHINE_PIIX3 | MACHINE_ACPI)

#define MACHINE_DMA_0             0x00000001
#define MACHINE_DMA_1             0x00000002
#define MACHINE_DMA_2             0x00000004
#define MACHINE_DMA_3             0x00000008
#define MACHINE_DMA_DISABLED      0x00000010
#define MACHINE_DMA_5             0x00000020
#define MACHINE_DMA_6             0x00000040
#define MACHINE_DMA_7             0x00000080
#define MACHINE_DMA_JUMPERS_MASK  (MACHINE_DMA_0 | MACHINE_DMA_1 | MACHINE_DMA_2 | MACHINE_DMA_3 | \
                                    MACHINE_DMA_DISABLED | MACHINE_DMA_5 | MACHINE_DMA_6 | MACHINE_DMA_7)
#define MACHINE_DMA_USE_MBDMA     0x00000100
#define MACHINE_DMA_USE_CONFIG    0x00000200
#define MACHINE_DMA_EXT_CONFIG    (MACHINE_DMA_USE_MBDMA | MACHINE_DMA_USE_CONFIG)

#define DMA_DISABLED                       4
#define DMA_NONE                          -1
#define DMA_ANY                           -1

#define IS_ARCH(m, a) ((machines[m].bus_flags & (a)) ? 1 : 0)
#define IS_AT(m)      (((machines[m].bus_flags & (MACHINE_BUS_ISA16 | MACHINE_BUS_EISA | MACHINE_BUS_VLB | MACHINE_BUS_MCA | MACHINE_BUS_PCI | MACHINE_BUS_PCMCIA | MACHINE_BUS_AGP | MACHINE_BUS_AC97)) != 0) ? 1 : 0)
#define IS_MCA(m)     ((machines[m].bus_flags & (MACHINE_BUS_MCA | MACHINE_BUS_MCA32)) ? 1 : 0)

#define CPU_BLOCK(...) \
    (const uint8_t[])  \
    {                  \
        __VA_ARGS__, 0 \
    }
#define MACHINE_MULTIPLIER_FIXED -1

#define CPU_BLOCK_NONE           0

/* Make sure it's always an invalid value to avoid misdetections. */
#define MACHINE_AVAILABLE 0xffffffffffffffffULL

enum {
    MACHINE_TYPE_NONE       = 0,
    MACHINE_TYPE_8088,
    MACHINE_TYPE_8086,
    MACHINE_TYPE_286,
    MACHINE_TYPE_386SX,
    MACHINE_TYPE_M6117,
    MACHINE_TYPE_486SLC,
    MACHINE_TYPE_386DX,
    MACHINE_TYPE_386DX_486,
    MACHINE_TYPE_SOCKET1,
    MACHINE_TYPE_SOCKET2,
    MACHINE_TYPE_SOCKET3,
    MACHINE_TYPE_SOCKET3_PCI,
    MACHINE_TYPE_SOCKET3_4,
    MACHINE_TYPE_STPC,
    MACHINE_TYPE_SOCKET4,
    MACHINE_TYPE_SOCKET4_5,
    MACHINE_TYPE_SOCKET5,
    MACHINE_TYPE_SOCKET7_3V,
    MACHINE_TYPE_SOCKET7,
    MACHINE_TYPE_SOCKETS7,
    MACHINE_TYPE_SOCKET8,
    MACHINE_TYPE_SLOT1,
    MACHINE_TYPE_SLOT1_2,
    MACHINE_TYPE_SLOT1_370,
    MACHINE_TYPE_SLOT2,
    MACHINE_TYPE_SOCKET370,
    MACHINE_TYPE_SOCKET423,
    MACHINE_TYPE_SOCKET478,
    MACHINE_TYPE_SOCKET754,
    MACHINE_TYPE_SOCKET939,
    MACHINE_TYPE_SOCKETAM2,
    MACHINE_TYPE_LGA775,
    MACHINE_TYPE_MISC,
    MACHINE_TYPE_MAX
};

enum {
    MACHINE_CHIPSET_NONE = 0,
    MACHINE_CHIPSET_DISCRETE,
    MACHINE_CHIPSET_PROPRIETARY,
    MACHINE_CHIPSET_GC100A,
    MACHINE_CHIPSET_GC103,
    MACHINE_CHIPSET_HT18,
    MACHINE_CHIPSET_ACC_2036,
    MACHINE_CHIPSET_ACC_2168,
    MACHINE_CHIPSET_ALI_M1217,
    MACHINE_CHIPSET_ALI_M6117,
    MACHINE_CHIPSET_ALI_M1409,
    MACHINE_CHIPSET_ALI_M1429,
    MACHINE_CHIPSET_ALI_M1429G,
    MACHINE_CHIPSET_ALI_M1489,
    MACHINE_CHIPSET_ALI_ALADDIN_IV_PLUS,
    MACHINE_CHIPSET_ALI_ALADDIN_V,
    MACHINE_CHIPSET_ALI_ALADDIN_PRO_II,
    MACHINE_CHIPSET_AMD_750,
    MACHINE_CHIPSET_SCAT,
    MACHINE_CHIPSET_SCAT_SX,
    MACHINE_CHIPSET_NEAT,
    MACHINE_CHIPSET_NEAT_SX,
    MACHINE_CHIPSET_CT_AT,
    MACHINE_CHIPSET_CT_386,
    MACHINE_CHIPSET_CT_CS4031,
    MACHINE_CHIPSET_CONTAQ_82C596,
    MACHINE_CHIPSET_CONTAQ_82C597,
    MACHINE_CHIPSET_IMS_8848,
    MACHINE_CHIPSET_INTEL_82335,
    MACHINE_CHIPSET_INTEL_420TX,
    MACHINE_CHIPSET_INTEL_420ZX,
    MACHINE_CHIPSET_INTEL_420EX,
    MACHINE_CHIPSET_INTEL_430LX,
    MACHINE_CHIPSET_INTEL_430NX,
    MACHINE_CHIPSET_INTEL_430FX,
    MACHINE_CHIPSET_INTEL_430HX,
    MACHINE_CHIPSET_INTEL_430VX,
    MACHINE_CHIPSET_INTEL_430TX,
    MACHINE_CHIPSET_INTEL_450KX,
    MACHINE_CHIPSET_INTEL_450GX,
    MACHINE_CHIPSET_INTEL_440FX,
    MACHINE_CHIPSET_INTEL_440EX,
    MACHINE_CHIPSET_INTEL_440LX,
    MACHINE_CHIPSET_INTEL_440BX,
    MACHINE_CHIPSET_INTEL_440ZX,
    MACHINE_CHIPSET_INTEL_440GX,
    MACHINE_CHIPSET_INTEL_I815EP,
    MACHINE_CHIPSET_INTEL_I845,
    MACHINE_CHIPSET_OPTI_283,
    MACHINE_CHIPSET_OPTI_291,
    MACHINE_CHIPSET_OPTI_381,
    MACHINE_CHIPSET_OPTI_391,
    MACHINE_CHIPSET_OPTI_481,
    MACHINE_CHIPSET_OPTI_493,
    MACHINE_CHIPSET_OPTI_495SLC,
    MACHINE_CHIPSET_OPTI_495SX,
    MACHINE_CHIPSET_OPTI_496,
    MACHINE_CHIPSET_OPTI_498,
    MACHINE_CHIPSET_OPTI_499,
    MACHINE_CHIPSET_OPTI_895_802G,
    MACHINE_CHIPSET_OPTI_547,
    MACHINE_CHIPSET_OPTI_571,
    MACHINE_CHIPSET_OPTI_597,
    MACHINE_CHIPSET_OPTI_VIPER,
    MACHINE_CHIPSET_SARC_RC2016A,
    MACHINE_CHIPSET_SIS_310,
    MACHINE_CHIPSET_SIS_401,
    MACHINE_CHIPSET_SIS_411,
    MACHINE_CHIPSET_SIS_460,
    MACHINE_CHIPSET_SIS_461,
    MACHINE_CHIPSET_SIS_471,
    MACHINE_CHIPSET_SIS_496,
    MACHINE_CHIPSET_SIS_501,
    MACHINE_CHIPSET_SIS_5501,
    MACHINE_CHIPSET_SIS_5511,
    MACHINE_CHIPSET_SIS_5571,
    MACHINE_CHIPSET_SIS_5581,
    MACHINE_CHIPSET_SIS_530,
    MACHINE_CHIPSET_SIS_5591,
    MACHINE_CHIPSET_SIS_5600,
    MACHINE_CHIPSET_SMSC_VICTORYBX_66,
    MACHINE_CHIPSET_STPC_CLIENT,
    MACHINE_CHIPSET_STPC_CONSUMER_II,
    MACHINE_CHIPSET_STPC_ELITE,
    MACHINE_CHIPSET_STPC_ATLAS,
    MACHINE_CHIPSET_SYMPHONY_SL82C460,
    MACHINE_CHIPSET_UMC_UM82C480,
    MACHINE_CHIPSET_UMC_UM82C491,
    MACHINE_CHIPSET_UMC_UM8881,
    MACHINE_CHIPSET_UMC_UM8890BF,
    MACHINE_CHIPSET_UTRON_UT85C50X,
    MACHINE_CHIPSET_VIA_VT82C495,
    MACHINE_CHIPSET_VIA_VT82C496G,
    MACHINE_CHIPSET_VIA_APOLLO_VPX,
    MACHINE_CHIPSET_VIA_APOLLO_VP3,
    MACHINE_CHIPSET_VIA_APOLLO_MVP3,
    MACHINE_CHIPSET_VIA_APOLLO_PRO,
    MACHINE_CHIPSET_VIA_APOLLO_PRO_133,
    MACHINE_CHIPSET_VIA_APOLLO_PRO_133A,
    MACHINE_CHIPSET_VLSI_SCAMP,
    MACHINE_CHIPSET_VLSI_VL82C480,
    MACHINE_CHIPSET_VLSI_VL82C481,
    MACHINE_CHIPSET_VLSI_VL82C486,
    MACHINE_CHIPSET_VLSI_SUPERCORE,
    MACHINE_CHIPSET_VLSI_WILDCAT,
    MACHINE_CHIPSET_WD76C10,
    MACHINE_CHIPSET_ZYMOS_POACH,
    MACHINE_CHIPSET_INTEL_I845E,
    MACHINE_CHIPSET_MAX
};

typedef struct _machine_filter_ {
    const char *name;
    const char  id;
} machine_filter_t;

typedef struct _machine_cpu_ {
    uint32_t       package;
    const uint8_t *block;
    uint32_t       min_bus;
    uint32_t       max_bus;
    uint16_t       min_voltage;
    uint16_t       max_voltage;
    float          min_multi;
    float          max_multi;
} machine_cpu_t;

