#include <stdio.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <time.h>


#define CLOCKS_PER_MSEC (147456 / 256)
#define BUF_SIZE (256*1024)


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

	fseek(fp,0,SEEK_END);
	fd_size = ftell(fp);
	fseek(fp,0,SEEK_SET);

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
    printf("Read/Write speed test\n");
    printf("---------------------\n");

	if (SifLoadModule("host:usbd.irx", 0, NULL) < 0)
		printf("Could not find 'host:usbd.irx'\n");

	if (SifLoadModule("host:usbhdfsd.irx", 0, NULL) < 0)
		printf("Could not find 'host:usbhdfsd.irx'\n");

	if (SifLoadModule("host:iLinkman.irx", 0, NULL) < 0)
		printf("Could not find 'host:iLinkman.irx'\n");

	if (SifLoadModule("host:IEEE1394_disk.irx", 0, NULL) < 0)
		printf("Could not find 'host:IEEE1394_disk.irx'\n");

	delay(5); // some delay is required by usb mass storage driver

	read_test("mass:zero.bin");
	read_test("sd:zero.bin");

	return 0;
}

