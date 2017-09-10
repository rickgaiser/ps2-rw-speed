#include <stdio.h>
#include <kernel.h>
#include <sifrpc.h>
#include <fileXio_rpc.h>
#include <loadfile.h>
#include <time.h>

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
void read_test(const char * filename)
{
    int fd_size, size_left, size_sec;
    int msec;
    FILE *fp;
    char * buffer = NULL;
    clock_t clk_start, clk_end, clk_sec;

	if ((fp = fopen(filename, "rb")) == NULL) {
		printf("Could not find '%s'\n", filename);
		return;
	}

	//fseek(fp,0,SEEK_END);
	//fd_size = ftell(fp);
	fd_size = 16*1024*1024;
	//fseek(fp,0,SEEK_SET);

	buffer = malloc(BUF_SIZE);

	printf("Start reading file %s:\n", filename);

	clk_start = clk_end = clk_sec = clock();
	size_left = fd_size;
	size_sec = 0;
	while (size_left > 0) {
        int read_size = (size_left > BUF_SIZE) ? BUF_SIZE : size_left;
        if (fread(buffer, 1, read_size, fp) != read_size) {
            printf("Failed to read file.\n");
            return;
        }
        size_left -= read_size;
        size_sec += read_size;
        clk_end = clock();

        msec = (int)((clk_end - clk_sec) / CLOCKS_PER_MSEC);
        if (msec >= 1000) {
            printf("Speed: %dKB/s\n", size_sec / msec);

            clk_sec = clk_end;
            size_sec = 0;
        }
	}

	printf("Done!\n");
	msec = (int)((clk_end - clk_start) / CLOCKS_PER_MSEC);
	printf("Average speed: %dKB/s\n", fd_size / msec);

	free(buffer);

	fclose(fp);
}

//--------------------------------------------------------------
int main()
{
    printf("EE Read/Write speed test\n");
    printf("------------------------\n");

	/*
	 * Load IO modules
	 */
	if (SifLoadModule("host:iomanX.irx", 0, NULL) < 0)
		printf("Could not load 'host:iomanX.irx'\n");

	if (SifLoadModule("host:fileXio.irx", 0, NULL) < 0)
		printf("Could not load 'host:fileXio.irx'\n");

#ifdef USE_BDM
	if (SifLoadModule("host:bdm.irx", 0, NULL) < 0)
		printf("Could not load 'host:bdm.irx'\n");
	if (SifLoadModule("host:bdmfs_mbr.irx", 0, NULL) < 0)
		printf("Could not load 'host:bdmfs_mbr.irx'\n");
#endif

#ifdef TEST_USB
	/*
	 * Load USB modules
	 */
	if (SifLoadModule("host:usbd.irx", 0, NULL) < 0)
	//if (SifLoadModule("host:USBD.IRX", 0, NULL) < 0)
		printf("Could not load 'host:usbd.irx'\n");

#ifdef USE_BDM
	if (SifLoadModule("host:usbmass_bd.irx", 0, NULL) < 0)
		printf("Could not load 'host:usbmass_bd.irx'\n");
#else
	if (SifLoadModule("host:usbhdfsd.irx", 0, NULL) < 0)
		printf("Could not load 'host:usbhdfsd.irx'\n");
#endif
#endif

#ifdef TEST_IEEE
	/*
	 * Load IEEE1394 modules
	 */
	if (SifLoadModule("host:iLinkman.irx", 0, NULL) < 0)
		printf("Could not load 'host:iLinkman.irx'\n");

#ifdef USE_BDM
	if (SifLoadModule("host:IEEE1394_bd.irx", 0, NULL) < 0)
		printf("Could not load 'host:IEEE1394_bd.irx'\n");
#else
	if (SifLoadModule("host:IEEE1394_disk.irx", 0, NULL) < 0)
		printf("Could not load 'host:IEEE1394_disk.irx'\n");
#endif
#endif

	// Give low level drivers some time to init before starting the FS
	delay(5);

#ifdef USE_BDM
	if (SifLoadModule("host:bdmfs_vfat.irx", 0, NULL) < 0)
		printf("Could not load 'host:bdmfs_vfat.irx'\n");
#endif

#ifdef TEST_HDD
	/*
	 * Load HDD modules
	 */
	//if (SifLoadModule("host:ps2dev9.irx", 0, NULL) < 0)
	//	printf("Could not load 'host:ps2dev9.irx'\n");

	if (SifLoadModule("host:ps2atad.irx", 0, NULL) < 0)
		printf("Could not load 'host:ps2atad.irx'\n");

	if (SifLoadModule("host:ps2hdd.irx", 0, NULL) < 0)
		printf("Could not load 'host:ps2hdd.irx'\n");

	if (SifLoadModule("host:ps2fs.irx", 0, NULL) < 0)
		printf("Could not load 'host:ps2fs.irx'\n");

	fileXioInit();
	if (fileXioMount("pfs0:", "hdd0:__system", FIO_MT_RDWR) == -ENOENT)
            printf("Could not mount 'hdd0:__system'\n");
#endif

#ifdef TEST_USB
	//read_test("mass:zero.bin");  // Place 'zero.bin' inside fat32 partition of USB stick
#endif
#ifdef TEST_IEEE
	//read_test("sd:zero.bin");    // Place 'zero.bin' inside fat32 partition of IEEE1394 drive
#endif
#ifdef TEST_HDD
	read_test("pfs0:zero.bin");  // Place 'zero.bin' inside __system partition of internal HDD (use uLE)
#endif

	/*
	 * Start read/write speed test on the IOP
	 */
	if (SifLoadModule("host:iop/rw_speed.irx", 0, NULL) < 0)
		printf("Could not load 'host:iop/rw_speed.irx'\n");

	return 0;
}

