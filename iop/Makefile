IOP_BIN  = rw_speed.irx
IOP_OBJS = main.o imports.o

IOP_CFLAGS += -Wall

all: $(IOP_BIN)

clean:
	rm -f -r $(IOP_OBJS) $(IOP_BIN)

install: $(IOP_BIN)
	cp $(IOP_BIN) $(PS2SDK)/iop/irx

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
