#include <thbase.h>
#include <bdm.h>

#include "internal.h"
#include "cdvd_config.h"


extern struct cdvdman_settings_usb cdvdman_settings;
volatile struct block_device* g_bd = NULL;
static u32 g_bd_sectors_per_sector = 4;

extern struct irx_export_table _exp_bdm;


void DeviceInit(void)
{
    DPRINTF("%s\n", __func__);

    RegisterLibraryEntries(&_exp_bdm);
}

void DeviceDeinit(void)
{
    DPRINTF("%s\n", __func__);
}

void bdm_connect_bd(struct block_device* bd)
{
    DPRINTF("connecting device %s%dp%d\n", bd->name, bd->devNr, bd->parNr);

    if (g_bd == NULL)
    {
        g_bd = bd;
        g_bd_sectors_per_sector = (2048 / bd->sectorSize);
    }
}

void bdm_disconnect_bd(struct block_device* bd)
{
    DPRINTF("disconnecting device %s%dp%d\n", bd->name, bd->devNr, bd->parNr);

    if (g_bd == bd)
        g_bd = NULL;
}

void DeviceFSInit(void)
{
    int i;
    DPRINTF("USB: NumParts = %d\n", cdvdman_settings.common.NumParts);
    for (i=0; i<cdvdman_settings.common.NumParts; i++)
        DPRINTF("USB: LBAs[%d] = %d\n", i, cdvdman_settings.LBAs[i]);

    while (g_bd == NULL)
    {
        DPRINTF("Waiting for device...\n");
        DelayThread(1000000);
    }
}

int DeviceReadSectors(u32 lsn, void *buffer, unsigned int sectors)
{
    u32 sector;
    u16 count;

    DPRINTF("%s\n", __func__);

    sector = cdvdman_settings.LBAs[0] + (lsn * g_bd_sectors_per_sector);
    count  = sectors * g_bd_sectors_per_sector;
    g_bd->read(g_bd, sector, buffer, count);

    return 0;
}
