all:
	$(MAKE) -C cdvdfsv all
	$(MAKE) -C cdvdman USE_USB=1 IOPCORE_DEBUG=1 all
	$(MAKE) -C iop all
	$(MAKE) -C ee all

clean:
	$(MAKE) -C cdvdfsv clean
	$(MAKE) -C cdvdman USE_USB=1 clean
	$(MAKE) -C iop clean
	$(MAKE) -C ee clean

copy:
	cp /usr/local/ps2dev/ps2sdk/iop/irx/*.irx modules/
	cp ../../open-ps2-loader/modules/iopcore/cdvdfsv/*.irx modules/
	cp ../../open-ps2-loader/modules/iopcore/cdvdman/*.irx modules/

test: all
	ps2client -h $(PS2_IP) execee host:ee/rw_speed.elf
