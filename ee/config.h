#ifndef CONFIG_H
#define CONFIG_H


#define READ_SIZE (1024*1024)
#define BLOCK_SIZE_MIN (512)
#define BLOCK_SIZE_MAX (32*1024)
//#define MASS_FILE_NAME "mass:zero.bin"
#define MASS_FILE_NAME "mass:DVD/SLES_549.45.DragonBall Z Budokai Tenkaichi 3.iso"

// Load one of these Block Disk Managers
#define LOAD_BDM
//#define LOAD_BDM_CDVD

// Load one of these Block Disks
#define LOAD_BD_USB
//#define LOAD_BD_MC2SD
//#define LOAD_BD_IEEE

// Load one or more File Systems
#define LOAD_FS_VFAT
#define LOAD_FS_EXT2

// Other file systems
//#define LOAD_PFS

// Where to run the tests
#define TEST_ON_EE
#define TEST_ON_IOP


#endif
