all:
	$(MAKE) -C iop all
	$(MAKE) -C ee all

clean:
	$(MAKE) -C iop clean
	$(MAKE) -C ee clean

copy:
	cp /usr/local/ps2dev/ps2sdk/iop/irx/fileXio.irx .
	cp /usr/local/ps2dev/ps2sdk/iop/irx/IEEE1394_disk.irx .
	cp /usr/local/ps2dev/ps2sdk/iop/irx/iLinkman.irx .
	cp /usr/local/ps2dev/ps2sdk/iop/irx/iomanX.irx .
	cp /usr/local/ps2dev/ps2sdk/iop/irx/ps2atad.irx .
	cp /usr/local/ps2dev/ps2sdk/iop/irx/ps2dev9.irx .
	cp /usr/local/ps2dev/ps2sdk/iop/irx/ps2fs.irx .
	cp /usr/local/ps2dev/ps2sdk/iop/irx/ps2hdd.irx .
	cp /usr/local/ps2dev/ps2sdk/iop/irx/usbd.irx .
	cp /usr/local/ps2dev/ps2sdk/iop/irx/usbhdfsd.irx .

test: all
	ps2client -h $(PS2_IP) execee host:ee/rw_speed.elf
