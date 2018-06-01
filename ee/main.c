#include <stdio.h>
#include <string.h>
#include <kernel.h>
#include <sbv_patches.h>
#include <debug.h>
#include <sifrpc.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <debug.h>
#include <ps2ips.h>
#include <ps2smb.h>
#include <fileXio_rpc.h>
#include <loadfile.h>
#include <time.h>
#include <libcdvd.h>
#include <usbhdfsd-common.h>

#include "config.h"


#define CLOCKS_PER_MSEC (147456 / 256)

#define PRINTF printf
//#define PRINTF scr_printf

#define IRX_DEFINE(mod) \
extern unsigned char mod##_irx[]; \
extern unsigned int size_##mod##_irx

#define IRX_LOAD(mod) \
    if (SifExecModuleBuffer(mod##_irx, size_##mod##_irx, 0, NULL, NULL) < 0) \
        PRINTF("Could not load ##mod##\n")


IRX_DEFINE(rw_speed);
IRX_DEFINE(cdvdman);
IRX_DEFINE(cdvdfsv);
IRX_DEFINE(iomanX);
IRX_DEFINE(fileXio);
IRX_DEFINE(bdm);
IRX_DEFINE(netman);
IRX_DEFINE(smbman);
IRX_DEFINE(smap);
IRX_DEFINE(ps2ip_nm);
IRX_DEFINE(ps2ips);
IRX_DEFINE(udptty);
IRX_DEFINE(usbd);
IRX_DEFINE(usbmass_bd);
IRX_DEFINE(sio2man);
IRX_DEFINE(sio2sd_bd);
IRX_DEFINE(iLinkman);
IRX_DEFINE(IEEE1394_bd);
IRX_DEFINE(bdmfs_vfat);
IRX_DEFINE(bdmfs_ext2);
IRX_DEFINE(ps2dev9);
IRX_DEFINE(ps2atad);
IRX_DEFINE(ps2hdd);
IRX_DEFINE(ps2fs);


static void ethPrintIPConfig(void)
{
	t_ip_info ip_info;
	u8 ip_address[4], netmask[4], gateway[4];

	//SMAP is registered as the "sm0" device to the TCP/IP stack.
	if (ps2ip_getconfig("sm0", &ip_info) >= 0)
	{
		ip_address[0] = ip4_addr1((struct ip4_addr *)&ip_info.ipaddr);
		ip_address[1] = ip4_addr2((struct ip4_addr *)&ip_info.ipaddr);
		ip_address[2] = ip4_addr3((struct ip4_addr *)&ip_info.ipaddr);
		ip_address[3] = ip4_addr4((struct ip4_addr *)&ip_info.ipaddr);

		netmask[0] = ip4_addr1((struct ip4_addr *)&ip_info.netmask);
		netmask[1] = ip4_addr2((struct ip4_addr *)&ip_info.netmask);
		netmask[2] = ip4_addr3((struct ip4_addr *)&ip_info.netmask);
		netmask[3] = ip4_addr4((struct ip4_addr *)&ip_info.netmask);

		gateway[0] = ip4_addr1((struct ip4_addr *)&ip_info.gw);
		gateway[1] = ip4_addr2((struct ip4_addr *)&ip_info.gw);
		gateway[2] = ip4_addr3((struct ip4_addr *)&ip_info.gw);
		gateway[3] = ip4_addr4((struct ip4_addr *)&ip_info.gw);

		scr_printf(	"IP:\t%d.%d.%d.%d\n"
                    "NM:\t%d.%d.%d.%d\n"
                    "GW:\t%d.%d.%d.%d\n",
					ip_address[0], ip_address[1], ip_address[2], ip_address[3],
					netmask[0], netmask[1], netmask[2], netmask[3],
					gateway[0], gateway[1], gateway[2], gateway[3]);
	}
	else
	{
		scr_printf("Unable to read IP address.\n");
	}
}

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
	PRINTF("Read %dKiB in %dms, blocksize=%d, speed=%dKB/s\n", fd_size/1024, msec, buf_size, fd_size / msec);

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

//--------------------------------------------------------------
int main()
{
    t_ip_info ipconfig;
#ifdef TEST_ON_EE
    unsigned int buf_size;
#endif

	init_scr();

    PRINTF("Reloading all IOP modules...\n");

	SifInitRpc(0);

	//Reboot IOP
	while(!SifIopReset("", 0)){};
	while(!SifIopSync()){};

	//Initialize SIF services
	SifInitRpc(0);
	SifLoadFileInit();
	SifInitIopHeap();
	sbv_patch_enable_lmb();

	// Load TCP/IP stack modules
    IRX_LOAD(ps2dev9);
    IRX_LOAD(netman);
    IRX_LOAD(bdm);
    IRX_LOAD(smap);
#if 1
    IRX_LOAD(ps2ip_nm);
    IRX_LOAD(ps2ips);

    strncpy(ipconfig.netif_name, "sm0", 3);
    IP4_ADDR((struct ip4_addr *)&ipconfig.ipaddr,  192, 168,   1,  10);
    IP4_ADDR((struct ip4_addr *)&ipconfig.netmask, 255, 255, 255,   0);
    IP4_ADDR((struct ip4_addr *)&ipconfig.gw,      192, 168,   1, 254);

    ps2ip_init();
    ps2ip_setconfig(&ipconfig);

	// Load UDPTTY so we can see debug output again
    IRX_LOAD(udptty);

    PRINTF("IOP ready!\n");
	ethPrintIPConfig();
#endif
	delay(25);

	/*
	 * Load IO modules
	 */
    IRX_LOAD(iomanX);
    IRX_LOAD(fileXio);

#ifdef LOAD_BD_SMB
    IRX_LOAD(smbman);
    {
        smbLogOn_in_t logon;
        smbEcho_in_t echo;
        smbOpenShare_in_t openshare;
        int result;
        const char * ethBase = "smb0:";

        strncpy(logon.serverIP, "192.168.1.198", sizeof(logon.serverIP));
        logon.serverPort = 445;
        strncpy(logon.User, "guest", sizeof(logon.User));
        logon.PasswordType = NO_PASSWORD;
        openshare.PasswordType = NO_PASSWORD;

        if ((result = fileXioDevctl(ethBase, SMB_DEVCTL_LOGON, (void *)&logon, sizeof(logon), NULL, 0)) >= 0) {
            PRINTF("SMB login OK!\n");
            // SMB server alive test
            strcpy(echo.echo, "ALIVE ECHO TEST");
            echo.len = strlen("ALIVE ECHO TEST");

            if (fileXioDevctl(ethBase, SMB_DEVCTL_ECHO, (void *)&echo, sizeof(echo), NULL, 0) >= 0) {
                PRINTF("SMB echo OK!\n");
                // connect to the share
                strcpy(openshare.ShareName, "PS2SMB");

                if (fileXioDevctl(ethBase, SMB_DEVCTL_OPENSHARE, (void *)&openshare, sizeof(openshare), NULL, 0) >= 0) {
                    PRINTF("SMB share connected!\n");
                }
            } else {
                PRINTF("SMB echo failed\n");
            }
        } else {
            PRINTF("SMB login failed\n");
        }
    }
#endif

#ifdef LOAD_BDM
    //IRX_LOAD(bdm);
#endif

#ifdef LOAD_BDM_CDVD
    IRX_LOAD(cdvdman);
    IRX_LOAD(cdvdfsv);
#endif

#ifdef LOAD_BD_USB
    IRX_LOAD(usbd);
    IRX_LOAD(usbmass_bd);
#endif

#ifdef LOAD_BD_MC2SD
    //IRX_LOAD(sio2man);
    IRX_LOAD(sio2sd_bd);
#endif

#ifdef LOAD_BD_IEEE
    IRX_LOAD(iLinkman);
    IRX_LOAD(IEEE1394_bd);
#endif

#ifdef LOAD_FS_VFAT
    IRX_LOAD(bdmfs_vfat);
#endif

#ifdef LOAD_FS_EXT2
    IRX_LOAD(bdmfs_ext2);
#endif

	// Give low level drivers some time to init
	delay(5);

#ifdef LOAD_PFS
	/*
	 * Load HDD modules
	 */
    //IRX_LOAD(ps2dev9);
    IRX_LOAD(ps2atad);
    IRX_LOAD(ps2hdd);
    IRX_LOAD(ps2fs);

	fileXioInit();
	if (fileXioMount("pfs0:", "hdd0:__system", FIO_MT_RDWR) == -ENOENT)
            PRINTF("Could not mount 'hdd0:__system'\n");
#endif

#ifdef TEST_ON_EE
	/*
	 * Start read/write speed test on the EE
	 */
    PRINTF("EE Read/Write speed test\n");
    PRINTF("------------------------\n");

#ifdef LOAD_BDM
    int fd = fileXioOpen(MASS_FILE_NAME, O_RDONLY);
    int LBA = fileXioIoctl(fd, USBMASS_IOCTL_GET_LBA, MASS_FILE_NAME);
    PRINTF("LBA = %d\n", LBA);
    fileXioClose(fd);

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
#endif

#ifdef TEST_ON_IOP
	/*
	 * Start read/write speed test on the IOP
	 */
    IRX_LOAD(rw_speed);
#endif

	PRINTF("Done, exit in 5\n");
	delay(5);

	//Deinitialize SIF services
	SifExitRpc();

	return 0;
}

