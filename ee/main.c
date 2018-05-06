#include <stdio.h>
#include <kernel.h>
#include <debug.h>
#include <sifrpc.h>
#include <fileXio_rpc.h>
#include <loadfile.h>
#include <time.h>
#include <libcdvd.h>
#include <usbhdfsd-common.h>

#include "config.h"


#define CLOCKS_PER_MSEC (147456 / 256)

#define PRINTF printf
//#define PRINTF scr_printf


//--------------------------------------------------------------
void delay(int count)
{
    int i;
    int ret;
    for (i = 0; i < count; i++) {
        ret = 0x01000000;
        while (ret--)
            asm("nop\nnop\nnop\nnop");
    }
}

//--------------------------------------------------------------
void read_test(const char * filename, unsigned int buf_size, unsigned int max_size)
{
    int fd_size, size_left;
    int msec;
    FILE *fp;
    char * buffer = NULL;
    clock_t clk_start, clk_end;

	if ((fp = fopen(filename, "rb")) == NULL) {
		PRINTF("Could not find '%s'\n", filename);
		return;
	}

	//fseek(fp,0,SEEK_END);
	//fd_size = ftell(fp);
	fd_size = max_size;
	//fseek(fp,0,SEEK_SET);

	buffer = malloc(buf_size);

	clk_start = clock();
	size_left = fd_size;
	while (size_left > 0) {
        int read_size = (size_left > buf_size) ? buf_size : size_left;
        if (fread(buffer, 1, read_size, fp) != read_size) {
            PRINTF("Failed to read file.\n");
            return;
        }
        size_left -= read_size;
	}
    clk_end = clock();

	msec = (int)((clk_end - clk_start) / CLOCKS_PER_MSEC);
	PRINTF("Read %dKiB in %dms, buf_size=%d, speed=%dKB/s\n", fd_size/1024, msec, buf_size, fd_size / msec);

	free(buffer);

	fclose(fp);
}

//--------------------------------------------------------------
void test_cdvd()
{
	sceCdRMode mode = {1, 0, 0, 0};
    char * buffer = NULL;

	PRINTF("Start reading CDVD\n");

	if (sceCdInit(SCECdINIT) != 1)
		PRINTF("ERROR: sceCdInit(SCECdINIT)\n");

	buffer = malloc(512*1024);

	if (sceCdReadDVDV(0, 1, buffer, &mode) != 1)
		PRINTF("ERROR: sceCdReadDVDV\n");

	if (sceCdSync(0) != 0)
		PRINTF("ERROR: sceCdSync\n");

	free(buffer);

	//if (sceCdInit(SCECdEXIT) != 1)
	//	PRINTF("ERROR: sceCdInit(SCECdEXIT)\n");
}

#define SIF_LOAD(mod) \
    if (SifLoadModule("host:" mod, 0, NULL) < 0) \
        PRINTF("Could not load 'host:" mod "'\n")

//--------------------------------------------------------------
int main()
{
#ifdef TEST_ON_EE
    unsigned int buf_size;
#endif

    PRINTF("EE Read/Write speed test\n");
    PRINTF("------------------------\n");

	SifInitRpc(0);

	/*
	 * Load IO modules
	 */
    SIF_LOAD("modules/iomanX.irx");
    SIF_LOAD("modules/fileXio.irx");

#ifdef LOAD_BDM
    SIF_LOAD("modules/bdm.irx");
#endif

#ifdef LOAD_BDM_CDVD
    SIF_LOAD("cdvdman/cdvdman.irx");
    SIF_LOAD("cdvdfsv/cdvdfsv.irx");
#endif

#ifdef LOAD_BD_USB
    SIF_LOAD("modules/usbd.irx");
    SIF_LOAD("modules/usbmass_bd.irx");
#endif

#ifdef LOAD_BD_MC2SD
    //SIF_LOAD("modules/sio2man.irx");
    SIF_LOAD("modules/mc2sd_bd.irx");
#endif

#ifdef LOAD_BD_IEEE
    SIF_LOAD("modules/iLinkman.irx");
    SIF_LOAD("modules/IEEE1394_bd.irx");
#endif

#ifdef LOAD_FS_VFAT
    SIF_LOAD("modules/bdmfs_vfat.irx");
#endif

#ifdef LOAD_FS_EXT2
    SIF_LOAD("modules/bdmfs_ext2.irx");
#endif

	// Give low level drivers some time to init
	delay(5);

#ifdef LOAD_PFS
	/*
	 * Load HDD modules
	 */
    //SIF_LOAD("modules/ps2dev9.irx");
    SIF_LOAD("modules/ps2atad.irx");
    SIF_LOAD("modules/ps2hdd.irx");
    SIF_LOAD("modules/ps2fs.irx");

	fileXioInit();
	if (fileXioMount("pfs0:", "hdd0:__system", FIO_MT_RDWR) == -ENOENT)
            PRINTF("Could not mount 'hdd0:__system'\n");
#endif

#ifdef TEST_ON_EE
	/*
	 * Start read/write speed test on the EE
	 */
#ifdef LOAD_BDM
    int fd = fileXioOpen(MASS_FILE_NAME, O_RDONLY);
    int LBA = fileXioIoctl(fd, USBMASS_IOCTL_GET_LBA, MASS_FILE_NAME);
    PRINTF("LBA = %d\n", LBA);
    fileXioClose(fd);

	for (buf_size = BLOCK_SIZE_MIN; buf_size <= BLOCK_SIZE_MAX; buf_size *= 2)
        read_test(MASS_FILE_NAME, buf_size, READ_SIZE);
#endif
#ifdef LOAD_BDM_CDVD
	for (buf_size = BLOCK_SIZE_MIN; buf_size <= BLOCK_SIZE_MAX; buf_size *= 2)
		read_test("cdrom:DATA/PZS3EU1.AFS", buf_size, READ_SIZE);
	//test_cdvd();
#endif
#ifdef LOAD_PFS
	for (buf_size = BLOCK_SIZE_MIN; buf_size <= BLOCK_SIZE_MAX; buf_size *= 2)
        read_test("pfs0:zero.bin", buf_size, READ_SIZE);  // Place 'zero.bin' inside __system partition of internal HDD (use uLE)
#endif
#endif

#ifdef TEST_ON_IOP
	/*
	 * Start read/write speed test on the IOP
	 */
    SIF_LOAD("iop/rw_speed.irx");
#endif

	PRINTF("Done, exit in 5\n");
	delay(5);

	return 0;
}

