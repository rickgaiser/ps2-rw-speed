/*
 * usb_driver.c - USB Mass Storage Driver
 * See usbmass-ufi10.pdf and usbmassbulk_10.pdf
 */

#include <errno.h>
#include <intrman.h>
#include <sysclib.h>
#include <sysmem.h>
#include <thbase.h>
#include <thsemap.h>
#include <stdio.h>
#include <usbd.h>

//#define DEBUG  //comment out this line when not debugging

#include "smsutils.h"
#include "usbd_macro.h"
#include "mass_debug.h"
#include "mass_common.h"
#include "mass_stor.h"
#include "cdvd_config.h"

extern struct cdvdman_settings_usb cdvdman_settings;

#define getBI32(__buf) ((((u8 *)(__buf))[3] << 0) | (((u8 *)(__buf))[2] << 8) | (((u8 *)(__buf))[1] << 16) | (((u8 *)(__buf))[0] << 24))

#define USB_SUBCLASS_MASS_RBC 0x01
#define USB_SUBCLASS_MASS_ATAPI 0x02
#define USB_SUBCLASS_MASS_QIC 0x03
#define USB_SUBCLASS_MASS_UFI 0x04
#define USB_SUBCLASS_MASS_SFF_8070I 0x05
#define USB_SUBCLASS_MASS_SCSI 0x06

#define USB_PROTOCOL_MASS_CBI 0x00
#define USB_PROTOCOL_MASS_CBI_NO_CCI 0x01
#define USB_PROTOCOL_MASS_BULK_ONLY 0x50

#define TAG_TEST_UNIT_READY 0
#define TAG_REQUEST_SENSE 3
#define TAG_INQUIRY 18
#define TAG_READ_CAPACITY 37
#define TAG_READ 40
#define TAG_START_STOP_UNIT 33
#define TAG_WRITE 42

#define USB_BLK_EP_IN 0
#define USB_BLK_EP_OUT 1

#define USB_XFER_MAX_RETRIES 16
#define USB_IO_MAX_RETRIES 32

#define DEVICE_DETECTED 0x01
#define DEVICE_CONFIGURED 0x02
#define DEVICE_ERROR 0x80

#define CBW_TAG 0x43425355
#define CSW_TAG 0x53425355

typedef struct _cbw_packet
{
    unsigned int signature;
    unsigned int tag;
    unsigned int dataTransferLength;
    unsigned char flags; //80->data in,  00->out
    unsigned char lun;
    unsigned char comLength;   //command data length
    unsigned char comData[16]; //command data
} cbw_packet __attribute__((packed));

typedef struct _csw_packet
{
    unsigned int signature;
    unsigned int tag;
    unsigned int dataResidue;
    unsigned char status;
} csw_packet __attribute__((packed));

typedef struct _inquiry_data
{
    u8 peripheral_device_type; // 00h - Direct access (Floppy), 1Fh none (no FDD connected)
    u8 removable_media;        // 80h - removeable
    u8 iso_ecma_ansi;
    u8 response_data_format;
    u8 additional_length;
    u8 res[3];
    u8 vendor[8];
    u8 product[16];
    u8 revision[4];
} inquiry_data __attribute__((packed));

typedef struct _sense_data
{
    u8 error_code;
    u8 res1;
    u8 sense_key;
    u8 information[4];
    u8 add_sense_len;
    u8 res3[4];
    u8 add_sense_code;
    u8 add_sense_qual;
    u8 res4[4];
} sense_data __attribute__((packed));

typedef struct _read_capacity_data
{
    u8 last_lba[4];
    u8 block_length[4];
} read_capacity_data __attribute__((packed));

static UsbDriver driver;

#ifdef VMC_DRIVER
static int io_sema;

#define WAITIOSEMA(x) WaitSema(x)
#define SIGNALIOSEMA(x) SignalSema(x)
#else
#define WAITIOSEMA(x)
#define SIGNALIOSEMA(x)
#endif

typedef struct _usb_callback_data
{
    int semh;
    int returnCode;
    int returnSize;
} usb_callback_data;

static mass_dev g_mass_device;
static void *gSectorBuffer;

static void mass_stor_release(mass_dev *dev);

static void usb_callback(int resultCode, int bytes, void *arg)
{
    usb_callback_data *data = (usb_callback_data *)arg;
    data->returnCode = resultCode;
    data->returnSize = bytes;
    XPRINTF("USBHDFSD: callback: res %d, bytes %d, arg %p \n", resultCode, bytes, arg);
    SignalSema(data->semh);
}

static void usb_set_configuration(mass_dev *dev, int configNumber)
{
    int ret;
    usb_callback_data cb_data;

    cb_data.semh = dev->ioSema;

    XPRINTF("USBHDFSD: setting configuration controlEp=%i, confNum=%i \n", dev->controlEp, configNumber);
    ret = UsbSetDeviceConfiguration(dev->controlEp, configNumber, usb_callback, (void *)&cb_data);

    if (ret == USB_RC_OK) {
        WaitSema(cb_data.semh);
        ret = cb_data.returnCode;
    }
    if (ret != USB_RC_OK) {
        XPRINTF("USBHDFSD: Error - sending set_configuration %d\n", ret);
    }
}

static void usb_set_interface(mass_dev *dev, int interface, int altSetting)
{
    int ret;
    usb_callback_data cb_data;

    cb_data.semh = dev->ioSema;

    XPRINTF("USBHDFSD: setting interface controlEp=%i, interface=%i altSetting=%i\n", dev->controlEp, interface, altSetting);
    ret = UsbSetInterface(dev->controlEp, interface, altSetting, usb_callback, (void *)&cb_data);

    if (ret == USB_RC_OK) {
        WaitSema(cb_data.semh);
        ret = cb_data.returnCode;
    }
    if (ret != USB_RC_OK) {
        XPRINTF("USBHDFSD: Error - sending set_interface %d\n", ret);
    }
}

static int usb_bulk_clear_halt(mass_dev *dev, int endpoint)
{
    int ret;
    usb_callback_data cb_data;

    cb_data.semh = dev->ioSema;

    ret = UsbClearEndpointFeature(
        dev->controlEp, //Config pipe
        0,              //HALT feature
        (endpoint == USB_BLK_EP_IN) ? dev->bulkEpI : dev->bulkEpO,
        usb_callback,
        (void *)&cb_data);

    if (ret == USB_RC_OK) {
        WaitSema(cb_data.semh);
        ret = cb_data.returnCode;
    }
    if (ret != USB_RC_OK) {
        XPRINTF("USBHDFSD: Error - sending clear halt %d\n", ret);
    }

    return ret;
}

static int usb_bulk_get_max_lun(mass_dev *dev)
{
    int ret;
    usb_callback_data cb_data;
    char max_lun;

    cb_data.semh = dev->ioSema;

    //Call Bulk only mass storage reset
    ret = UsbControlTransfer(
        dev->controlEp, //default pipe
        0xA1,
        0xFE,
        0,
        dev->interfaceNumber, //interface number
        1,                    //length
        &max_lun,             //data
        usb_callback,
        (void *)&cb_data);

    if (ret == USB_RC_OK) {
        WaitSema(cb_data.semh);
        ret = cb_data.returnCode;
    }
    if (ret == USB_RC_OK) {
        ret = max_lun;
    } else {
        //Devices that do not support multiple LUNs may STALL this command.
        usb_bulk_clear_halt(dev, USB_BLK_EP_IN);
        usb_bulk_clear_halt(dev, USB_BLK_EP_OUT);

        ret = -ret;
    }

    return ret;
}

typedef void (*scsi_cb)(void* arg);

struct usbmass_cmd {
	mass_dev* dev;

	cbw_packet cbw;
	csw_packet csw;
	int cmd_count;

	scsi_cb cb;
	void* cb_arg;

	int returnCode;
};

static void scsi_cmd_callback(int resultCode, int bytes, void *arg)
{
	struct usbmass_cmd* ucmd = (struct usbmass_cmd*)arg;
	mass_dev* dev = (mass_dev*)ucmd->dev;

	XPRINTF("scsi_cmd_callback, result=%d\n", resultCode);

	ucmd->cmd_count--;
	if ((resultCode != USB_RC_OK) || (ucmd->cmd_count == 0)) {
		// TODO: handle error in an async way (cannot block in usb callback)
		ucmd->returnCode = resultCode;
		if (ucmd->cb != NULL)
			ucmd->cb(ucmd->cb_arg);
		else
			SignalSema(dev->ioSema);
	}
}

#define MAX_SIZE (4*1024)
int mass_scsi_cmd(mass_dev* dev, const unsigned char *cmd, unsigned int cmd_len, unsigned char *data, unsigned int data_len, unsigned int data_wr, scsi_cb cb, void* cb_arg)
{
	static struct usbmass_cmd ucmd;
	int result;
	static unsigned int tag = 0;

	XPRINTF("mass_scsi_cmd\n");

	tag++;

	// Create USB command
	ucmd.dev = dev;
	ucmd.cmd_count = 0;
	ucmd.cb = cb;
	ucmd.cb_arg = cb_arg;

	// Create CBW
	ucmd.cbw.signature = CBW_TAG;
	ucmd.cbw.tag = tag;
	ucmd.cbw.dataTransferLength = data_len;
	ucmd.cbw.flags = 0x80;
	ucmd.cbw.lun = 0;
	ucmd.cbw.comLength = cmd_len;
	memcpy(ucmd.cbw.comData, cmd, cmd_len);

	// Create CSW
	ucmd.csw.signature = CSW_TAG;
	ucmd.csw.tag = tag;
	ucmd.csw.dataResidue = 0;
	ucmd.csw.status = 0;

	// Send the CBW (command)
	ucmd.cmd_count++;
	result = UsbBulkTransfer(dev->bulkEpO,	&ucmd.cbw, 31, scsi_cmd_callback, (void*)&ucmd);
	if (result != USB_RC_OK)
		return -EIO;

	// Send/Receive data
	while (data_len > 0) {
		unsigned int tr_len = (data_len < MAX_SIZE) ? data_len : MAX_SIZE;
		ucmd.cmd_count++;
		result = UsbBulkTransfer(data_wr ? dev->bulkEpO : dev->bulkEpI, data, tr_len, scsi_cmd_callback, (void*)&ucmd);
		if (result != USB_RC_OK)
			return -EIO;
		data_len -= tr_len;
		data += tr_len;
	}

	// Receive CSW (status)
	ucmd.cmd_count++;
	result = UsbBulkTransfer(dev->bulkEpI, &ucmd.csw, 13, scsi_cmd_callback, (void*)&ucmd);
	if (result != USB_RC_OK)
		return -EIO;

	if (cb == NULL) {
		// Wait for SCSI command to finish
		WaitSema(dev->ioSema);
		if (ucmd.returnCode != USB_RC_OK)
			return -EIO;
	}

	return 0;
}

static int cbw_scsi_cmd(mass_dev *dev, unsigned char cmd, void *buffer, int buf_size, int cmd_size)
{
	unsigned char comData[12] = {0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	comData[0] = cmd;
	comData[4] = cmd_size;

	return mass_scsi_cmd(dev, comData, 12, buffer, buf_size, 0, NULL, NULL);
}

static inline int cbw_scsi_test_unit_ready(mass_dev *dev)
{
    XPRINTF("USBHDFSD: cbw_scsi_test_unit_ready\n");

	return cbw_scsi_cmd(dev, 0x00, NULL, 0, 0);
}

static inline int cbw_scsi_request_sense(mass_dev *dev, void *buffer, int size)
{
    XPRINTF("USBHDFSD: cbw_scsi_request_sense\n");

	return cbw_scsi_cmd(dev, 0x03, buffer, size, size);
}

static inline int cbw_scsi_inquiry(mass_dev *dev, void *buffer, int size)
{
    XPRINTF("USBHDFSD: cbw_scsi_inquiry\n");

	return cbw_scsi_cmd(dev, 0x12, buffer, size, size);
}

static inline int cbw_scsi_start_stop_unit(mass_dev *dev)
{
    XPRINTF("USBHDFSD: cbw_scsi_start_stop_unit\n");

	return cbw_scsi_cmd(dev, 0x1b, NULL, 0, 1);
}

static inline int cbw_scsi_read_capacity(mass_dev *dev, void *buffer, int size)
{
    XPRINTF("USBHDFSD: cbw_scsi_read_capacity\n");

	return cbw_scsi_cmd(dev, 0x25, buffer, size, 0);
}

static int cbw_scsi_sector_io(mass_dev *dev, short int dir, unsigned int lba, void *buffer, unsigned short int sectorCount, scsi_cb cb, void* cb_arg)
{
	unsigned char comData[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    XPRINTF("USBHDFSD: cbw_scsi_sector_io (%s) - 0x%08x %p 0x%04x\n", dir == USB_BLK_EP_IN ? "READ" : "WRITE", lba, buffer, sectorCount);

	comData[0] = dir ? 0x2a : 0x28;
	comData[2] = (lba & 0xFF000000) >> 24;	//lba 1 (MSB)
	comData[3] = (lba & 0xFF0000) >> 16;	//lba 2
	comData[4] = (lba & 0xFF00) >> 8;		//lba 3
	comData[5] = (lba & 0xFF);			//lba 4 (LSB)
	comData[7] = (sectorCount & 0xFF00) >> 8;	//Transfer length MSB
	comData[8] = (sectorCount & 0xFF);		//Transfer length LSB

	return mass_scsi_cmd(dev, comData, 12, (void *)buffer, dev->sectorSize * sectorCount, dir, cb, cb_arg);
}

#define SCSI_READ_AHEAD
#define SCSI_RA_SECTORS (32*2)
#ifdef SCSI_READ_AHEAD
int io_sem;
void scsi_read_async_cb(void* arg)
{
	SignalSema(io_sem);
}

static u32 last_sector = 0xffffffff;
static u16 last_count = 0;
static char ra_buffer[SCSI_RA_SECTORS*512];
static char* ra_buffer_pointer = ra_buffer;
static u32 ra_sector = 0xffffffff;
static u16 ra_count = 0;
static u16 ra_active = 0;
#endif
void mass_stor_readSector(unsigned int sector, unsigned short int count, unsigned char *buffer)
{
    WAITIOSEMA(io_sema);

#ifdef SCSI_READ_AHEAD
	// Wait for any read-ahead actions to finish
	if (ra_active == 1) {
		WaitSema(io_sem);
		ra_active = 0;
	}

	if ((ra_sector == sector) && (ra_count >= count)) {
		//printf("using read-ahead sector=%d, count=%d\n", sector, count);
		// Read data from read-ahead buffer
		memcpy(buffer, ra_buffer_pointer, g_mass_device.sectorSize * count);
		ra_sector += count;
		ra_count -= count;
		ra_buffer_pointer += count*512;
	}
	else {
		//printf("using disk sector=%d, count=%d\n", sector, count);
		// Read data from disk
		if(cbw_scsi_sector_io(&g_mass_device, USB_BLK_EP_IN, sector, buffer, count, NULL, NULL) < 0)
			return;
	}

	// Sequential read detection -> do read-ahead
	if ((last_sector + last_count) == sector) {
		if ((ra_sector == (sector+count)) && (ra_count >= count)) {
			// We already have this in our buffer
			;
		}
		else if (count <= SCSI_RA_SECTORS) {
			//printf("read-ahead: sector=%d, count=%d\n", sector+count, count);
			if(cbw_scsi_sector_io(&g_mass_device, USB_BLK_EP_IN, sector+count, ra_buffer, count, scsi_read_async_cb, NULL) == 0) {
				ra_active = 1;
				ra_sector = sector+count;
				ra_count = count;
				ra_buffer_pointer = ra_buffer;
			}
		}
	}

	last_sector = sector;
	last_count = count;
#else
    cbw_scsi_sector_io(&g_mass_device, USB_BLK_EP_IN, lba, buffer, nsectors, NULL, NULL);
#endif

    SIGNALIOSEMA(io_sema);
}

#ifdef VMC_DRIVER
void mass_stor_writeSector(unsigned int lba, unsigned short int nsectors, const unsigned char *buffer)
{
    WAITIOSEMA(io_sema);

    cbw_scsi_sector_io(&g_mass_device, USB_BLK_EP_OUT, lba, (void *)buffer, nsectors, NULL, NULL);

    SIGNALIOSEMA(io_sema);
}
#endif

/* test that endpoint is bulk endpoint and if so, update device info */
static void usb_bulk_probeEndpoint(int devId, mass_dev *dev, UsbEndpointDescriptor *endpoint)
{
    if (endpoint->bmAttributes == USB_ENDPOINT_XFER_BULK) {
        /* out transfer */
        if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT && dev->bulkEpO < 0) {
            dev->bulkEpO = pUsbOpenEndpointAligned(devId, endpoint);
            dev->packetSzO = (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB;
            XPRINTF("USBHDFSD: register Output endpoint id =%i addr=%02X packetSize=%i\n", dev->bulkEpO, endpoint->bEndpointAddress, dev->packetSzO);
        } else
            /* in transfer */
            if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN && dev->bulkEpI < 0) {
            dev->bulkEpI = pUsbOpenEndpointAligned(devId, endpoint);
            dev->packetSzI = (unsigned short int)endpoint->wMaxPacketSizeHB << 8 | endpoint->wMaxPacketSizeLB;
            XPRINTF("USBHDFSD: register Input endpoint id =%i addr=%02X packetSize=%i\n", dev->bulkEpI, endpoint->bEndpointAddress, dev->packetSzI);
        }
    }
}

int mass_stor_probe(int devId)
{
    UsbDeviceDescriptor *device = NULL;
    UsbConfigDescriptor *config = NULL;
    UsbInterfaceDescriptor *intf = NULL;

    XPRINTF("USBHDFSD: probe: devId=%i\n", devId);

    mass_dev *mass_device = &g_mass_device;

    /* only one device supported */
    if ((mass_device != NULL) && (mass_device->status & DEVICE_DETECTED)) {
        XPRINTF("USBHDFSD: Error - only one mass storage device allowed ! \n");
        return 0;
    }

    /* get device descriptor */
    device = (UsbDeviceDescriptor *)pUsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);
    if (device == NULL) {
        XPRINTF("USBHDFSD: Error - Couldn't get device descriptor\n");
        return 0;
    }

    /* Check if the device has at least one configuration */
    if (device->bNumConfigurations < 1) {
        return 0;
    }

    /* read configuration */
    config = (UsbConfigDescriptor *)pUsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);
    if (config == NULL) {
        XPRINTF("USBHDFSD: Error - Couldn't get configuration descriptor\n");
        return 0;
    }
    /* check that at least one interface exists */
    XPRINTF("USBHDFSD: bNumInterfaces %d\n", config->bNumInterfaces);
    if ((config->bNumInterfaces < 1) ||
        (config->wTotalLength < (sizeof(UsbConfigDescriptor) + sizeof(UsbInterfaceDescriptor)))) {
        XPRINTF("USBHDFSD: Error - No interfaces available\n");
        return 0;
    }
    /* get interface */
    intf = (UsbInterfaceDescriptor *)((char *)config + config->bLength); /* Get first interface */
    XPRINTF("USBHDFSD: bInterfaceClass %X bInterfaceSubClass %X bInterfaceProtocol %X\n",
            intf->bInterfaceClass, intf->bInterfaceSubClass, intf->bInterfaceProtocol);

    if ((intf->bInterfaceClass != USB_CLASS_MASS_STORAGE) ||
        (intf->bInterfaceSubClass != USB_SUBCLASS_MASS_SCSI &&
         intf->bInterfaceSubClass != USB_SUBCLASS_MASS_SFF_8070I) ||
        (intf->bInterfaceProtocol != USB_PROTOCOL_MASS_BULK_ONLY) ||
        (intf->bNumEndpoints < 2)) { //one bulk endpoint is not enough because
        return 0;                    //we send the CBW to te bulk out endpoint
    }
    return 1;
}

int mass_stor_connect(int devId)
{
    int i;
    int epCount;
    UsbDeviceDescriptor *device;
    UsbConfigDescriptor *config;
    UsbInterfaceDescriptor *interface;
    UsbEndpointDescriptor *endpoint;
    iop_sema_t SemaData;
    mass_dev *dev;

    XPRINTF("USBHDFSD: connect: devId=%i\n", devId);
    dev = &g_mass_device;

    if (dev == NULL) {
        XPRINTF("USBHDFSD: Error - unable to allocate space!\n");
        return 1;
    }

    /* only one mass device allowed */
    if (dev->devId != -1) {
        XPRINTF("USBHDFSD: Error - only one mass storage device allowed !\n");
        return 1;
    }

    dev->status = 0;
    dev->sectorSize = 0;

    dev->bulkEpI = -1;
    dev->bulkEpO = -1;

    /* open the config endpoint */
    dev->controlEp = pUsbOpenEndpoint(devId, NULL);

    device = (UsbDeviceDescriptor *)pUsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_DEVICE);

    config = (UsbConfigDescriptor *)pUsbGetDeviceStaticDescriptor(devId, device, USB_DT_CONFIG);

    interface = (UsbInterfaceDescriptor *)((char *)config + config->bLength); /* Get first interface */

    // store interface numbers
    dev->interfaceNumber = interface->bInterfaceNumber;
    dev->interfaceAlt = interface->bAlternateSetting;

    epCount = interface->bNumEndpoints;
    endpoint = (UsbEndpointDescriptor *)pUsbGetDeviceStaticDescriptor(devId, NULL, USB_DT_ENDPOINT);
    usb_bulk_probeEndpoint(devId, dev, endpoint);

    for (i = 1; i < epCount; i++) {
        endpoint = (UsbEndpointDescriptor *)((char *)endpoint + endpoint->bLength);
        usb_bulk_probeEndpoint(devId, dev, endpoint);
    }

    // Bail out if we do NOT have enough bulk endpoints.
    if (dev->bulkEpI < 0 || dev->bulkEpO < 0) {
        mass_stor_release(dev);
        XPRINTF("USBHDFSD: Error - connect failed: not enough bulk endpoints! \n");
        return -1;
    }

    SemaData.initial = 0;
    SemaData.max = 1;
    SemaData.option = 0;
    SemaData.attr = 0;
    if ((dev->ioSema = CreateSema(&SemaData)) < 0) {
        XPRINTF("USBHDFSD: Failed to allocate I/O semaphore.\n");
        return -1;
    }

    /*store current configuration id - can't call set_configuration here */
    dev->devId = devId;
    dev->configId = config->bConfigurationValue;
    dev->status = DEVICE_DETECTED;
    XPRINTF("USBHDFSD: connect ok: epI=%i, epO=%i \n", dev->bulkEpI, dev->bulkEpO);

    return 0;
}

static void mass_stor_release(mass_dev *dev)
{
    int OldState;

    if (dev->bulkEpI >= 0) {
        pUsbCloseEndpoint(dev->bulkEpI);
    }

    if (dev->bulkEpO >= 0) {
        pUsbCloseEndpoint(dev->bulkEpO);
    }

    dev->bulkEpI = -1;
    dev->bulkEpO = -1;
    dev->controlEp = -1;
    dev->status = 0;

    if (gSectorBuffer != NULL) {
        CpuSuspendIntr(&OldState);
        FreeSysMemory(gSectorBuffer);
        CpuResumeIntr(OldState);
        gSectorBuffer = NULL;
    }
}

int mass_stor_disconnect(int devId)
{
    mass_dev *dev;
    dev = &g_mass_device;

    XPRINTF("USBHDFSD: disconnect: devId=%i\n", devId);

    if ((dev->status & DEVICE_DETECTED) && devId == dev->devId) {
        mass_stor_release(dev);
        dev->devId = -1;

        DeleteSema(dev->ioSema);
    }
    return 0;
}

static int mass_stor_warmup(mass_dev *dev)
{
    inquiry_data id;
    sense_data sd;
    read_capacity_data rcd;
    int stat, OldState;

    XPRINTF("USBHDFSD: mass_stor_warmup\n");

    if (!(dev->status & DEVICE_DETECTED)) {
        XPRINTF("USBHDFSD: Error - no mass storage device found!\n");
        return -1;
    }

    stat = usb_bulk_get_max_lun(dev);
    XPRINTF("USBHDFSD: usb_bulk_get_max_lun %d\n", stat);

    mips_memset(&id, 0, sizeof(inquiry_data));
    if ((stat = cbw_scsi_inquiry(dev, &id, sizeof(inquiry_data))) < 0) {
        XPRINTF("USBHDFSD: Error - cbw_scsi_inquiry %d\n", stat);
        return -1;
    }

    XPRINTF("USBHDFSD: Vendor: %.8s\n", id.vendor);
    XPRINTF("USBHDFSD: Product: %.16s\n", id.product);
    XPRINTF("USBHDFSD: Revision: %.4s\n", id.revision);

    while ((stat = cbw_scsi_test_unit_ready(dev)) != 0) {
        XPRINTF("USBHDFSD: Error - cbw_scsi_test_unit_ready %d\n", stat);

        stat = cbw_scsi_request_sense(dev, &sd, sizeof(sense_data));
        if (stat != 0)
            XPRINTF("USBHDFSD: Error - cbw_scsi_request_sense %d\n", stat);

        if ((sd.error_code == 0x70) && (sd.sense_key != 0x00)) {
            XPRINTF("USBHDFSD: Sense Data key: %02X code: %02X qual: %02X\n", sd.sense_key, sd.add_sense_code, sd.add_sense_qual);

            if ((sd.sense_key == 0x02) && (sd.add_sense_code == 0x04) && (sd.add_sense_qual == 0x02)) {
                XPRINTF("USBHDFSD: Error - Additional initalization is required for this device!\n");
                if ((stat = cbw_scsi_start_stop_unit(dev)) != 0) {
                    XPRINTF("USBHDFSD: Error - cbw_scsi_start_stop_unit %d\n", stat);
                    return -1;
                }
            }
        }
    }

    if ((stat = cbw_scsi_read_capacity(dev, &rcd, sizeof(read_capacity_data))) != 0) {
        XPRINTF("USBHDFSD: Error - cbw_scsi_read_capacity %d\n", stat);
        return -1;
    }

    dev->sectorSize = getBI32(&rcd.block_length);
    dev->maxLBA = getBI32(&rcd.last_lba);
    XPRINTF("USBHDFSD: sectorSize %u maxLBA %u\n", dev->sectorSize, dev->maxLBA);

    if (dev->sectorSize > 2048) {
        CpuSuspendIntr(&OldState);
        gSectorBuffer = AllocSysMemory(ALLOC_FIRST, dev->sectorSize, NULL);
        CpuResumeIntr(OldState);
    } else {
        gSectorBuffer = NULL;
    }

    return 0;
}

int mass_stor_configureDevice(void)
{
    XPRINTF("USBHDFSD: configuring device... \n");

    mass_dev *dev = &g_mass_device;
    if (dev->devId != -1 && (dev->status & DEVICE_DETECTED) && !(dev->status & DEVICE_CONFIGURED)) {
        int ret;
        usb_set_configuration(dev, dev->configId);
        usb_set_interface(dev, dev->interfaceNumber, dev->interfaceAlt);
        dev->status |= DEVICE_CONFIGURED;

        ret = mass_stor_warmup(dev);
        if (ret < 0) {
            XPRINTF("USBHDFSD: Error - failed to warmup device %d\n", dev->devId);
            mass_stor_release(dev);
            return -1;
        }

        return 1;
    }

    return 0;
}

int mass_stor_init(void)
{
#ifdef SCSI_READ_AHEAD
	iop_sema_t sema;
#endif
    register int ret;

    g_mass_device.devId = -1;

#ifdef SCSI_READ_AHEAD
	sema.attr = 0;
	sema.option = 0;
	sema.initial = 0;
	sema.max = 1;
	io_sem = CreateSema(&sema);
#endif

#ifdef VMC_DRIVER
    iop_sema_t smp;
    smp.initial = 1;
    smp.max = 1;
    smp.option = 0;
    smp.attr = SA_THPRI;
    io_sema = CreateSema(&smp);
#endif

    driver.next = NULL;
    driver.prev = NULL;
    driver.name = "mass-stor";
    driver.probe = mass_stor_probe;
    driver.connect = mass_stor_connect;
    driver.disconnect = mass_stor_disconnect;

    ret = pUsbRegisterDriver(&driver);
    XPRINTF("mass_driver: registerDriver=%i \n", ret);
    if (ret < 0) {
        XPRINTF("mass_driver: register driver failed! ret=%d\n", ret);
        return -1;
    }

    return 0;
}

/*	There are many times of sector sizes for the many devices out there. But because:
	1. USB transfers have a limit of 4096 bytes (2 CD/DVD sectors).
	2. we need to keep things simple to save IOP clock cycles and RAM
	...devices with sector sizes that are either a multiple or factor of the CD/DVD sector will be supported.

	i.e. 512 (typical), 1024, 2048 and 4096 bytes can be supported, while 3072 bytes (if there's even such a thing) will be unsupported.
	Devices with sector sizes above 4096 bytes are unsupported.	*/
int mass_stor_ReadCD(unsigned int lsn, unsigned int nsectors, void *buf, int part_num)
{
    u32 sectors, nbytes, DiskSectorsToRead, lba;
    u8 *p = (u8 *)buf;

    if (g_mass_device.sectorSize > 2048) {
        //4096-byte sectors
        lba = cdvdman_settings.LBAs[part_num] + (lsn / 2);
        if ((lsn % 2) > 0) {
            //Start sector is in the middle of a physical sector.
            mass_stor_readSector(lba, 1, gSectorBuffer);
            mips_memcpy(p, &((unsigned char *)gSectorBuffer)[2048], 2048);
            lba++;
            lsn++;
            nsectors--;
            p += 2048;
        }
    } else {
        lba = cdvdman_settings.LBAs[part_num] + (lsn * (2048 / g_mass_device.sectorSize));
    }

    while (nsectors > 0) {
        sectors = nsectors;
        if (sectors > MAX_USB_SECTORS)
            sectors = MAX_USB_SECTORS;

        nbytes = sectors * 2048;
        DiskSectorsToRead = nbytes / g_mass_device.sectorSize;

        if (DiskSectorsToRead == 0) {
            // nsectors should be 1 at this point.
            mass_stor_readSector(lba, 1, gSectorBuffer);
            mips_memcpy(p, gSectorBuffer, 2048);
        } else {
            mass_stor_readSector(lba, DiskSectorsToRead, p);
        }

        lba += DiskSectorsToRead;
        lsn += sectors;
        p += nbytes;
        nsectors -= sectors;
    }

    return 1;
}
