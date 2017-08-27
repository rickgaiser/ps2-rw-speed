EE_BIN = rw_speed.elf
EE_OBJS = main.o
EE_LIBS = -lfileXio

all: $(EE_BIN)

test: all
	ps2client -h $(PS2_IP) execee host:$(EE_BIN)

reset: clean
	ps2client -h $(PS2_IP) reset

clean:
	rm -f -r $(EE_OBJS) $(EE_BIN)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
