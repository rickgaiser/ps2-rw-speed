#include <ioman.h>
#include <stdio.h>
#include <sysmem.h>
#include <thbase.h>
#include <thsemap.h>
#include "bdm.h"

#include "../ee/config.h"


#define MODNAME "rw_speed"
IRX_ID(MODNAME, 1, 1);


//--------------------------------------------------------------
void read_test(const char * filename, unsigned int buf_size, unsigned int max_size)
{
    int fd_size, size_left;
    unsigned int msec;
    int fd;
    char * buffer = NULL;
    iop_sys_clock_t clk_start, clk_end;
    u32 start_seconds, start_usecs;
    u32 end_seconds, end_usecs;

	if ((fd = open(filename, O_RDONLY/*, 0644*/)) <= 0) {
		printf("Could not find '%s'\n", filename);
		return;
	}

	//lseek(fd,0,SEEK_END);
	//fd_size = ftell(fd);
	fd_size = max_size;
	//lseek(fd,0,SEEK_SET);

	buffer = AllocSysMemory(ALLOC_FIRST, buf_size, NULL);

    GetSystemTime(&clk_start);
	size_left = fd_size;
	while (size_left > 0) {
        int read_size = (size_left > buf_size) ? buf_size : size_left;
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
	printf("Read %dKiB in %dms, blocksize=%d, speed=%dKB/s\n", fd_size/1024, msec, buf_size, fd_size / msec);

	FreeSysMemory(buffer);

	close(fd);
}

//--------------------------------------------------------------
void read_test_bd(struct block_device* bd, unsigned int sector_count, unsigned int size)
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
    }
    GetSystemTime(&clk_end);

	SysClock2USec(&clk_start, &start_seconds, &start_usecs);
	SysClock2USec(&clk_end,   &end_seconds,   &end_usecs);
	msec = (end_seconds - start_seconds) * 1000;
	if (end_usecs < start_usecs)
        msec -= (start_usecs - end_usecs) / 1000;
    else
        msec += (end_usecs - start_usecs) / 1000;
	printf("Read %dKiB in %dms, blocksize=%d, speed=%dKB/s\n", fd_size/1024, msec, bd->sectorSize*sector_count, fd_size / msec);

	FreeSysMemory(buffer);
}

//--------------------------------------------------------------
#define MAX_BD 10
int _start()
{
    struct block_device* bd[MAX_BD];
    unsigned int sector_count;
    unsigned int buf_size;
    int i;

    printf("IOP Read/Write speed test\n");
    printf("-------------------------\n");

#ifdef LOAD_BDM
    // Get all block devices from BDM (Block Device Manager)
    bdm_get_bd(bd, MAX_BD);
	// Test them all (only partition 0 == entire device)
	for (i = 0; i < MAX_BD; i++) {
		if ((bd[i] != NULL) && (bd[i]->parNr == 0)) {
			printf("Start reading '%s%dp%d' block device:\n", bd[i]->name, bd[i]->devNr, bd[i]->parNr);
			for (sector_count = (BLOCK_SIZE_MIN/512); sector_count <= (BLOCK_SIZE_MAX/512); sector_count *= 2)
				read_test_bd(bd[i], sector_count, READ_SIZE);
		}
	}
#endif

#ifdef LOAD_BDM
	printf("Start reading file %s:\n", MASS_FILE_NAME);
	for (buf_size = BLOCK_SIZE_MIN; buf_size <= BLOCK_SIZE_MAX; buf_size *= 2)
        read_test(MASS_FILE_NAME, buf_size, READ_SIZE);
#endif
#ifdef LOAD_BDM_CDVD
	printf("Start reading file %s:\n", CDVD_FILE_NAME);
	for (buf_size = BLOCK_SIZE_MIN; buf_size <= BLOCK_SIZE_MAX; buf_size *= 2)
		read_test(CDVD_FILE_NAME, buf_size, READ_SIZE);
	//test_cdvd();
#endif
#ifdef LOAD_BD_SMB
	printf("Start reading file %s:\n", SMB_FILE_NAME);
	for (buf_size = BLOCK_SIZE_MIN; buf_size <= BLOCK_SIZE_MAX; buf_size *= 2)
        read_test(SMB_FILE_NAME, buf_size, READ_SIZE);
#endif
#ifdef LOAD_PFS
	printf("Start reading file %s:\n", PFS_FILE_NAME);
	for (buf_size = BLOCK_SIZE_MIN; buf_size <= BLOCK_SIZE_MAX; buf_size *= 2)
        read_test(PFS_FILE_NAME, buf_size, READ_SIZE);
#endif
#ifdef LOAD_HOST
    // Below a block size of 2K the speed is terrible:
    //  512 -> speed is   4KiB/s
    // 1024 -> speed is   8KiB/s
    // 2048 -> speed is 700KiB/s
    // ... weird
    buf_size = (BLOCK_SIZE_MIN > 2048) ? BLOCK_SIZE_MIN : 2048;

	printf("Start reading file %s:\n", HOST_FILE_NAME);
	for (; buf_size <= BLOCK_SIZE_MAX; buf_size *= 2)
        read_test(HOST_FILE_NAME, buf_size, READ_SIZE);
#endif

	return 1;
}
