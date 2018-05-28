all:
	$(MAKE) -C cdvdfsv all
	$(MAKE) -C cdvdman IOPCORE_DEBUG=0 SPEED_TESTING=1 all
	$(MAKE) -C iop all
	$(MAKE) -C ee all

clean:
	$(MAKE) -C cdvdfsv clean
	$(MAKE) -C cdvdman clean
	$(MAKE) -C iop clean
	$(MAKE) -C ee clean

test: all
	ps2client -h $(PS2_IP) execee host:ee/rw_speed.elf
