#ifndef CONFIG_H
#define CONFIG_H


#define READ_SIZE (1024*1024)
#define BLOCK_SIZE_MIN (512)
#define BLOCK_SIZE_MAX (32*1024)
//#define MASS_FILE_NAME "mass:zero.bin"
#define MASS_FILE_NAME "mass:DVD/SLES_549.45.DragonBall Z Budokai Tenkaichi 3.iso"
#define CDVD_FILE_NAME "cdrom:DATA/PZS3EU1.AFS"
#define HOST_FILE_NAME "host:zero.bin"
#define PFS_FILE_NAME  "pfs0:zero.bin" // Place 'zero.bin' inside __system partition of internal HDD (use uLE)

// Load one of these Block Disk Managers
#define LOAD_BDM
//#define LOAD_BDM_CDVD

// Load one of these Block Disks
//#define LOAD_BD_USB
//#define LOAD_BD_MC2SD
//#define LOAD_BD_IEEE
#define LOAD_BD_SMAP

// Load one or more File Systems
#define LOAD_FS_VFAT
//#define LOAD_FS_EXT2

// Other file systems
//#define LOAD_PFS
//#define LOAD_HOST

// Where to run the tests
//#define TEST_ON_EE
#define TEST_ON_IOP


#endif
