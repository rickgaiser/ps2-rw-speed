#ifndef CONFIG_H
#define CONFIG_H


// Load one of these Block Disk Managers
#define LOAD_BDM
//#define LOAD_BDM_CDVD

// Load one of these Block Disks
#define LOAD_BD_USB
//#define LOAD_BD_MC2SD
//#define LOAD_BD_IEEE

// Load one or more File Systems
//#define LOAD_FS_VFAT
#define LOAD_FS_EXT2

// Other file systems
//#define LOAD_PFS

// Where to run the tests
#define TEST_ON_EE
#define TEST_ON_IOP


#endif
