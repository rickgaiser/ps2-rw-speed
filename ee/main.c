#include <stdio.h>
#include <kernel.h>
#include <sifrpc.h>
#include <fileXio_rpc.h>
#include <loadfile.h>
#include <time.h>
#include <libcdvd.h>

#include "config.h"


#define CLOCKS_PER_MSEC (147456 / 256)


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
		printf("Could not find '%s'\n", filename);
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
            printf("Failed to read file.\n");
            return;
        }
        size_left -= read_size;
	}
    clk_end = clock();

	msec = (int)((clk_end - clk_start) / CLOCKS_PER_MSEC);
	printf("Read %dKiB in %dms, buf_size=%d, speed=%dKB/s\n", fd_size/1024, msec, buf_size, fd_size / msec);

	free(buffer);

	fclose(fp);
}

//--------------------------------------------------------------
void test_cdvd()
{
	sceCdRMode mode = {1, 0, 0, 0};
    char * buffer = NULL;

	printf("Start reading CDVD\n");

	if (sceCdInit(SCECdINIT) != 1)
		printf("ERROR: sceCdInit(SCECdINIT)\n");

	buffer = malloc(512*1024);

	if (sceCdReadDVDV(0, 1, buffer, &mode) != 1)
		printf("ERROR: sceCdReadDVDV\n");

	if (sceCdSync(0) != 0)
		printf("ERROR: sceCdSync\n");

	free(buffer);

	//if (sceCdInit(SCECdEXIT) != 1)
	//	printf("ERROR: sceCdInit(SCECdEXIT)\n");
}

//--------------------------------------------------------------
int main()
{
#ifdef TEST_USB_CDVD
    unsigned int buf_size;
#endif

    printf("EE Read/Write speed test\n");
    printf("------------------------\n");

	SifInitRpc(0);

	/*
	 * Load IO modules
	 */
	if (SifLoadModule("host:modules/iomanX.irx", 0, NULL) < 0)
		printf("Could not load 'host:modules/iomanX.irx'\n");

	if (SifLoadModule("host:modules/fileXio.irx", 0, NULL) < 0)
		printf("Could not load 'host:modules/fileXio.irx'\n");

	if (SifLoadModule("host:modules/usbd.irx", 0, NULL) < 0)
		printf("Could not load 'host:modules/usbd.irx'\n");

	if (SifLoadModule("host:modules/iLinkman.irx", 0, NULL) < 0)
		printf("Could not load 'host:modules/iLinkman.irx'\n");

#ifdef TEST_USB
#ifdef USE_BDM
	if (SifLoadModule("host:modules/bdm.irx", 0, NULL) < 0)
		printf("Could not load 'host:modules/bdm.irx'\n");

	if (SifLoadModule("host:modules/usbmass_bd.irx", 0, NULL) < 0)
		printf("Could not load 'host:modules/usbmass_bd.irx'\n");

	if (SifLoadModule("host:modules/bdmfs_vfat.irx", 0, NULL) < 0)
		printf("Could not load 'host:modules/bdmfs_vfat.irx'\n");
#else
	if (SifLoadModule("host:modules/usbhdfsd.irx", 0, NULL) < 0)
		printf("Could not load 'host:modules/usbhdfsd.irx'\n");
#endif
#endif

#ifdef TEST_MC2SD
	if (SifLoadModule("host:modules/bdm.irx", 0, NULL) < 0)
		printf("Could not load 'host:modules/bdm.irx'\n");

	//if (SifLoadModule("host:modules/sio2man.irx", 0, NULL) < 0)
	//	printf("Could not load 'host:modules/sio2man.irx'\n");

	if (SifLoadModule("host:modules/mc2sd_bd.irx", 0, NULL) < 0)
		printf("Could not load 'host:modules/mc2sd_bd.irx'\n");

	if (SifLoadModule("host:modules/bdmfs_vfat.irx", 0, NULL) < 0)
		printf("Could not load 'host:modules/bdmfs_vfat.irx'\n");
#endif

#ifdef TEST_IEEE
#ifdef USE_BDM
	if (SifLoadModule("host:modules/bdm.irx", 0, NULL) < 0)
		printf("Could not load 'host:modules/bdm.irx'\n");

	if (SifLoadModule("host:modules/IEEE1394_bd.irx", 0, NULL) < 0)
		printf("Could not load 'host:modules/IEEE1394_bd.irx'\n");

	if (SifLoadModule("host:modules/bdmfs_vfat.irx", 0, NULL) < 0)
		printf("Could not load 'host:modules/bdmfs_vfat.irx'\n");
#else
	if (SifLoadModule("host:modules/IEEE1394_disk.irx", 0, NULL) < 0)
		printf("Could not load 'host:modules/IEEE1394_disk.irx'\n");
#endif
#endif

	// Give low level drivers some time to init before starting the FS
	delay(5);

#ifdef TEST_HDD
	/*
	 * Load HDD modules
	 */
	//if (SifLoadModule("host:modules/ps2dev9.irx", 0, NULL) < 0)
	//	printf("Could not load 'host:modules/ps2dev9.irx'\n");

	if (SifLoadModule("host:modules/ps2atad.irx", 0, NULL) < 0)
		printf("Could not load 'host:modules/ps2atad.irx'\n");

	if (SifLoadModule("host:modules/ps2hdd.irx", 0, NULL) < 0)
		printf("Could not load 'host:modules/ps2hdd.irx'\n");

	if (SifLoadModule("host:modules/ps2fs.irx", 0, NULL) < 0)
		printf("Could not load 'host:modules/ps2fs.irx'\n");

	fileXioInit();
	if (fileXioMount("pfs0:", "hdd0:__system", FIO_MT_RDWR) == -ENOENT)
            printf("Could not mount 'hdd0:__system'\n");
#endif

#ifdef TEST_USB_CDVD
	if (SifLoadModule("host:cdvdman/cdvdman.irx", 0, NULL) < 0)
		printf("Could not load 'host:cdvdman/cdvdman.irx'\n");

	if (SifLoadModule("host:cdvdfsv/cdvdfsv.irx", 0, NULL) < 0)
		printf("Could not load 'host:cdvdfsv/cdvdfsv.irx'\n");

	if (SifLoadModule("host:modules/usbmass_bd.irx", 0, NULL) < 0)
		printf("Could not load 'host:modules/usbmass_bd.irx'\n");

	//if (SifLoadModule("host:modules/IEEE1394_bd.irx", 0, NULL) < 0)
	//	printf("Could not load 'host:modules/IEEE1394_bd.irx'\n");
#endif


#ifdef TEST_USB
	//read_test("mass:zero.bin");  // Place 'zero.bin' inside fat32 partition of USB stick
#endif
#ifdef TEST_MC2SD
	//read_test("sdc:zero.bin");  // Place 'zero.bin' inside fat32 partition of SD card
#endif
#ifdef TEST_IEEE
	//read_test("sd:zero.bin");    // Place 'zero.bin' inside fat32 partition of IEEE1394 drive
#endif
#ifdef TEST_HDD
	read_test("pfs0:zero.bin");  // Place 'zero.bin' inside __system partition of internal HDD (use uLE)
#endif

#ifdef TEST_USB_CDVD
	for (buf_size = 512; buf_size <= (512*1024); buf_size *= 2)
		read_test("cdrom:DATA/PZS3EU1.AFS", buf_size, 1*1024*1024);
	//test_cdvd();
#endif

#ifdef TEST_ON_IOP
	/*
	 * Start read/write speed test on the IOP
	 */
	if (SifLoadModule("host:iop/rw_speed.irx", 0, NULL) < 0)
		printf("Could not load 'host:iop/rw_speed.irx'\n");
#endif

	printf("Done, exit in 5\n");
	delay(5);

	return 0;
}

