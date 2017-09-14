/*
  Copyright 2009-2010, jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.
*/

#include "smsutils.h"
#include "mass_common.h"
#include "mass_stor.h"
#include "dev9.h"
#include "atad.h"
#include "ioplib_util.h"
#include "cdvdman.h"
#include "internal.h"
#include "cdvd_config.h"

#include <loadcore.h>
#include <stdio.h>
#include <sysclib.h>
#include <sysmem.h>
#include <thbase.h>
#include <thevent.h>
#include <intrman.h>
#include <ioman.h>
#include <thsemap.h>
#include <errno.h>
#include <io_common.h>
#include "ioman_add.h"

#include <errno.h>

#include "device.h"

extern struct cdvdman_settings_hdd cdvdman_settings;

extern struct irx_export_table _exp_dev9;
extern struct irx_export_table _exp_atad;

char lba_48bit = 0;
char atad_inited = 0;
static unsigned char CurrentPart = 0;
static unsigned char NumParts;

static hdl_partspecs_t cdvdman_partspecs[HDL_NUM_PART_SPECS];

static int cdvdman_get_part_specs(u32 lsn)
{
    register int i;
    hdl_partspecs_t *ps;

    for (ps = cdvdman_partspecs, i = 0; i < NumParts; i++, ps++) {
        if ((lsn >= ps->part_offset) && (lsn < (ps->part_offset + (ps->part_size >> 11)))) {
            CurrentPart = i;
            break;
        }
    }

    if (i >= NumParts)
        return -ENXIO;

    return 0;
}

void DeviceInit(void)
{
#ifdef HD_PRO
#ifdef __IOPCORE_DEBUG
    RegisterLibraryEntries(&_exp_dev9);
#endif
#else
    RegisterLibraryEntries(&_exp_dev9);
#endif
    RegisterLibraryEntries(&_exp_atad);

#ifdef HD_PRO
#ifdef __IOPCORE_DEBUG
    dev9d_init();
#endif
#else
    dev9d_init();
#endif
    atad_start();
    atad_inited = 1;

    lba_48bit = cdvdman_settings.common.media;

    hdl_apa_header apaHeader;
    int r;

    DPRINTF("fs_init: apa header LBA = %lu\n", cdvdman_settings.lba_start);

    while ((r = ata_device_sector_io(0, &apaHeader, cdvdman_settings.lba_start, 2, ATA_DIR_READ)) != 0) {
        DPRINTF("fs_init: failed to read apa header %d\n", r);
        DelayThread(2000);
    }

    mips_memcpy(cdvdman_partspecs, apaHeader.part_specs, sizeof(cdvdman_partspecs));

    cdvdman_settings.common.media = apaHeader.discType;
    cdvdman_settings.common.layer1_start = apaHeader.layer1_start;
    NumParts = apaHeader.num_partitions;
}

void DeviceDeinit(void)
{
}

void DeviceFSInit(void)
{
}

int DeviceReadSectors(u32 lsn, void *buffer, unsigned int sectors)
{
    u32 offset = 0;
    while (sectors) {
        if (!((lsn >= cdvdman_partspecs[CurrentPart].part_offset) && (lsn < (cdvdman_partspecs[CurrentPart].part_offset + (cdvdman_partspecs[CurrentPart].part_size >> 11)))))
            if (cdvdman_get_part_specs(lsn) != 0)
                return -ENXIO;

        u32 nsectors = (cdvdman_partspecs[CurrentPart].part_offset + (cdvdman_partspecs[CurrentPart].part_size >> 11)) - lsn;
        if (sectors < nsectors)
            nsectors = sectors;

        u32 lba = cdvdman_partspecs[CurrentPart].data_start + ((lsn - cdvdman_partspecs[CurrentPart].part_offset) << 2);
        if (ata_device_sector_io(0, (void *)(buffer + offset), lba, nsectors << 2, ATA_DIR_READ) != 0) {
            return -EIO;
        }
        offset += nsectors << 11;
        sectors -= nsectors;
        lsn += nsectors;
    }

    return 0;
}
