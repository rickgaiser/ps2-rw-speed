EE_BIN = rw_speed.elf
EE_OBJS = main.o
EE_LIBS = -lfileXio -lcdvd -lps2ips -ldebug

# Add embedded IRX files
EE_IRX_FILES=\
	rw_speed.irx \
	cdvdman.irx \
	cdvdfsv.irx \
	iomanX.irx \
	fileXio.irx \
	bdm.irx \
	netman.irx \
	smbman.irx \
	ps2ips.irx \
	smap.irx \
	udptty.irx \
	usbd.irx \
	usbmass_bd.irx \
	sio2man.irx \
	sio2sd_bd.irx \
	iLinkman.irx \
	IEEE1394_bd.irx \
	bdmfs_vfat.irx \
	bdmfs_ext2.irx \
	ps2dev9.irx \
	ps2atad.irx \
	ps2hdd.irx \
	ps2fs.irx
EE_IRX_OBJS = $(addsuffix _irx.o, $(basename $(EE_IRX_FILES)))
EE_OBJS += $(EE_IRX_OBJS)

# Where to find the IRX files
vpath %.irx $(PS2SDK)/iop/irx/
vpath %.irx ../iop/
vpath %.irx ../cdvdman/
vpath %.irx ../cdvdfsv/
vpath %.irx ../../open-ps2-loader/modules/debug/udptty/

# Rule to generate them
%_irx.o: %.irx
	bin2c $< $*_irx.c $*_irx
	ee-gcc -c $*_irx.c -o $*_irx.o

# The 'minus' sign is not handled well...
ps2ip_nm_irx.o: ps2ip-nm.irx
	bin2c $< $*.c $*
	ee-gcc -c $*.c -o $*.o
EE_OBJS += ps2ip_nm_irx.o

# This is for the sbv patch
SBVLITE = $(PS2SDK)/sbv
EE_INCS += -I$(SBVLITE)/include
EE_LDFLAGS += -L$(SBVLITE)/lib
EE_LIBS += -lpatches -ldebug

all: $(EE_BIN)

test: all
	ps2client -h $(PS2_IP) execee host:$(EE_BIN)

reset: clean
	ps2client -h $(PS2_IP) reset

clean:
	rm -f -r $(EE_OBJS) $(EE_BIN) *_irx.c

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
