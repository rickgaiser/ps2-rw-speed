#include <iomanX.h>
#include <stdio.h>
#include <sysmem.h>
#include <thbase.h>
#include <thsemap.h>
#include "bdm.h"

#include "../ee/config.h"


#define MODNAME "rw_speed"
IRX_ID(MODNAME, 1, 1);


//--------------------------------------------------------------
void read_test(const char * filename, unsigned int max_size)
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
	fd_size = max_size;
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

int sem;
//--------------------------------------------------------------
void read_test_bd_cb(void* arg)
{
	SignalSema(sem);
}

//--------------------------------------------------------------
void read_test_bd(struct block_device* bd, unsigned int sector_count, unsigned int size, unsigned int delay_ms)
{
    int fd_size = 0;
    unsigned int msec;
    char * buffer = NULL;
    iop_sys_clock_t clk_start, clk_end;
    u32 start_seconds, start_usecs;
    u32 end_seconds, end_usecs;
    u32 sector;

	buffer = AllocSysMemory(ALLOC_FIRST, bd->sectorSize*sector_count, NULL);

    GetSystemTime(&clk_start);
    for (sector = 0; sector < (size / bd->sectorSize); sector += sector_count) {
        fd_size += bd->read(bd, sector, buffer, sector_count) * bd->sectorSize;

        // Do something else
		if (delay_ms > 0)
			DelayThread(delay_ms * 1000);
    }
    GetSystemTime(&clk_end);

	SysClock2USec(&clk_start, &start_seconds, &start_usecs);
	SysClock2USec(&clk_end,   &end_seconds,   &end_usecs);
	msec = (end_seconds - start_seconds) * 1000;
	if (end_usecs < start_usecs)
        msec -= (start_usecs - end_usecs) / 1000;
    else
        msec += (end_usecs - start_usecs) / 1000;
	printf("Read %dKiB in %dms, blocksize=%d, delay=%dms, speed=%dKB/s\n", fd_size/1024, msec, bd->sectorSize*sector_count, delay_ms, fd_size / msec);

	FreeSysMemory(buffer);
}

//--------------------------------------------------------------
#define MAX_BD 10
int _start()
{
	iop_sema_t sema;
    struct block_device* bd[MAX_BD];
    unsigned int sector_count;
    int i;

    printf("IOP Read/Write speed test\n");
    printf("-------------------------\n");

	sema.attr = 0;
	sema.option = 0;
	sema.initial = 0;
	sema.max = 1;
	sem = CreateSema(&sema);

    // Get all block devices from BDM (Block Device Manager)
    bdm_get_bd(bd, MAX_BD);
	// Test them all (only partition 0 == entire device)
	for (i = 0; i < MAX_BD; i++) {
		if ((bd[i] != NULL) && (bd[i]->parNr == 0)) {
			printf("Start reading '%s%dp%d' block device:\n", bd[i]->name, bd[i]->devNr, bd[i]->parNr);
			for (sector_count = 4; sector_count <= (32*2); sector_count *= 2)
				read_test_bd(bd[i], sector_count, 4*1024*1024, 0);
			for (sector_count = 4; sector_count <= (32*2); sector_count *= 2)
				read_test_bd(bd[i], sector_count, 4*1024*1024, 2);
		}
	}

	// Place 'zero.bin' inside fat32 partition of drive
	read_test("mass0:zero.bin", 4*1024*1024);
	read_test("mass0:zero.bin", 4*1024*1024);
	read_test("mass0:zero.bin", 4*1024*1024);
	read_test("mass0:zero.bin", 4*1024*1024);

	read_test("mass1:zero.bin", 4*1024*1024);
	read_test("mass1:zero.bin", 4*1024*1024);
	read_test("mass1:zero.bin", 4*1024*1024);
	read_test("mass1:zero.bin", 4*1024*1024);

#ifdef TEST_HDD
	read_test("pfs0:zero.bin", 16*1024*1024);  // Place 'zero.bin' inside __system partition of internal HDD (use uLE)
#endif

	DeleteSema(sem);

	return 1;
}
