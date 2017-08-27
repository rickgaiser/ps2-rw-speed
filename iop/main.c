#include <iomanX.h>
#include <stdio.h>
#include <sysmem.h>
#include "thbase.h"
#include "usbhdfsd.h"

#include "../ee/config.h"


#define MODNAME "rw_speed"
IRX_ID(MODNAME, 1, 1);


//--------------------------------------------------------------
void read_test(const char * filename)
{
    int fd_size, size_left;
    unsigned int msec;
    int fd;
    char * buffer = NULL;
    iop_sys_clock_t clk_start, clk_end;
    u32 start_seconds, start_usecs;
    u32 end_seconds, end_usecs;

	if ((fd = open(filename, O_RDONLY, 0644)) <= 0) {
		printf("Could not find '%s'\n", filename);
		return;
	}

	//lseek(fd,0,SEEK_END);
	//fd_size = ftell(fd);
	fd_size = 16*1024*1024;
	//lseek(fd,0,SEEK_SET);

	buffer = AllocSysMemory(ALLOC_FIRST, BUF_SIZE, NULL);

	printf("Start reading file %s:\n", filename);

    GetSystemTime(&clk_start);
	size_left = fd_size;
	while (size_left > 0) {
        int read_size = (size_left > BUF_SIZE) ? BUF_SIZE : size_left;
        if (read(fd, buffer, read_size) != read_size) {
            printf("Failed to read file.\n");
            return;
        }
        size_left -= read_size;
	}
    GetSystemTime(&clk_end);

	SysClock2USec(&clk_start, &start_seconds, &start_usecs);
	SysClock2USec(&clk_end,   &end_seconds,   &end_usecs);
	msec  = (end_seconds - start_seconds) * 1000;
	if (end_usecs < start_usecs)
        msec -= (start_usecs - end_usecs) / 1000;
    else
        msec += (end_usecs - start_usecs) / 1000;
	printf("Done! %dKiB in %dms\n", fd_size/1024, msec);
	printf("Average speed: %dKB/s\n", fd_size / msec);

	FreeSysMemory(buffer);

	close(fd);
}

//--------------------------------------------------------------
#define SECTOR_SIZE 512
#define SECTOR_COUNT 8
void read_test_usb()
{
    int fd_size = 0;
    unsigned int msec;
    char * buffer = NULL;
    iop_sys_clock_t clk_start, clk_end;
    u32 start_seconds, start_usecs;
    u32 end_seconds, end_usecs;
    u32 sector;
    fat_driver *fatd = UsbMassFatGetData(0);

	buffer = AllocSysMemory(ALLOC_FIRST, SECTOR_SIZE*SECTOR_COUNT, NULL);

	printf("Start reading USB sectors:\n");

    GetSystemTime(&clk_start);
    for (sector = 0; sector < (16*1024*1024 / SECTOR_SIZE); sector += SECTOR_COUNT) {
        //fd_size += UsbMassReadSector(fatd, &buffer, sector);
        fd_size += mass_stor_readSector(fatd->dev, sector, buffer, SECTOR_COUNT) * SECTOR_SIZE;
    }
    GetSystemTime(&clk_end);

	SysClock2USec(&clk_start, &start_seconds, &start_usecs);
	SysClock2USec(&clk_end,   &end_seconds,   &end_usecs);
	msec  = (end_seconds - start_seconds) * 1000;
	if (end_usecs < start_usecs)
        msec -= (start_usecs - end_usecs) / 1000;
    else
        msec += (end_usecs - start_usecs) / 1000;
	printf("Done! %dKiB in %dms\n", fd_size/1024, msec);
	printf("Average speed: %dKB/s\n", fd_size / msec);

	FreeSysMemory(buffer);
}

//--------------------------------------------------------------
int _start()
{
    printf("IOP Read/Write speed test\n");
    printf("-------------------------\n");

#ifdef TEST_USB
	read_test("mass:zero.bin");  // Place 'zero.bin' inside fat32 partition of USB stick
	read_test_usb();
#endif
#ifdef TEST_IEEE
	read_test("sd:zero.bin");    // Place 'zero.bin' inside fat32 partition of IEEE1394 drive
#endif
#ifdef TEST_HDD
	read_test("pfs0:zero.bin");  // Place 'zero.bin' inside __system partition of internal HDD (use uLE)
#endif

	return 1;
}
