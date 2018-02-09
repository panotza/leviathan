/* Driver for 1e71:170e devices.
 */

#include "common.h"

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>

#define DRIVER_NAME "kraken_x62"

#define DATA_SERIAL_NUMBER_SIZE 65

struct kraken_driver_data {
	char *serial_number;
};

void kraken_driver_update(struct usb_kraken *kraken)
{
	struct kraken_driver_data *data = kraken->data;
	// TODO
	//dev_info(&kraken->udev->dev, "updating...\n");
}

static ssize_t serial_no_show(struct device *dev, struct device_attribute *attr,
                           char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	return sprintf(buf, "%s\n", data->serial_number);
}

static DEVICE_ATTR_RO(serial_no);


static int kraken_x62_create_device_files(struct usb_interface *interface)
{
	if (device_create_file(&interface->dev, &dev_attr_serial_no))
		goto error_serial_no;
	return 0;

error_serial_no:
	return 1;
}

static void kraken_x62_remove_device_files(struct usb_interface *interface)
{
	device_remove_file(&interface->dev, &dev_attr_serial_no);
}

static int kraken_x62_initialize(struct usb_kraken *kraken, char *serial_number)
{
	u8 len;
	u8 i;
	// space for length byte, type-of-data byte, and serial number encoded
	// UTF-16 (-1 is for terminating null byte)
	u8 data[2 + (DATA_SERIAL_NUMBER_SIZE - 1) * 2];
	// TODO why does the device answer -EAGAIN here?
	int ret = usb_control_msg(
		kraken->udev, usb_rcvctrlpipe(kraken->udev, 0),
		0x06, 0x80, 0x0303, 0x0409, data, sizeof data, 1000);
	if (ret < 0) {
		return ret;
	}
	if (ret < 2 || data[1] != 0x03) {
		return 1;
	}
	len = (data[0] - 2) / 2;
	if (len > DATA_SERIAL_NUMBER_SIZE - 1) {
		return 2;
	}
	// convert UTF-16 serial to null-terminated ASCII string
	for (i = 0; i < len; i += 2) {
		serial_number[i / 2] = data[2 * i + 2];
		if (data[2 * i + 3] != 0x00) {
			return 3;
		}
	}
	serial_number[i] = '\0';
	return 0;
}

int kraken_driver_probe(struct usb_interface *interface,
                        const struct usb_device_id *id)
{
	struct kraken_driver_data *data;
	struct usb_kraken *kraken = usb_get_intfdata(interface);
	int ret = -ENOMEM;
	kraken->data = kmalloc(sizeof *kraken->data, GFP_KERNEL);
	if (kraken->data == NULL) {
		goto error_data;
	}
	data = kraken->data;

	data->serial_number = kmalloc(DATA_SERIAL_NUMBER_SIZE, GFP_KERNEL);
	if (data->serial_number == NULL) {
		goto error_serial;
	}
	ret = kraken_x62_initialize(kraken, data->serial_number);
	if (ret) {
		dev_err(&kraken->udev->dev,
		        "failed to initialize device (%d)\n", ret);
		goto error_init_message;
	}

	ret = kraken_x62_create_device_files(interface);
	if (ret) {
		goto error_init_message;
	}

	dev_info(&interface->dev, "device connected\n");

	return 0;
error_init_message:
	kfree(data->serial_number);
error_serial:
	kfree(data);
error_data:
	return ret;
}

void kraken_driver_disconnect(struct usb_interface *interface)
{
	struct usb_kraken *kraken = usb_get_intfdata(interface);
	struct kraken_driver_data *data = kraken->data;

	kraken_x62_remove_device_files(interface);

	kfree(data->serial_number);
	kfree(data);

	dev_info(&interface->dev, "device disconnected\n");
}

static const struct usb_device_id kraken_x62_id_table[] = {
	{ USB_DEVICE(0x1e71, 0x170e) },
	{ },
};

MODULE_DEVICE_TABLE(usb, kraken_x62_id_table);

static struct usb_driver kraken_x62_driver = {
	.name       = DRIVER_NAME,
	.probe      = kraken_probe,
	.disconnect = kraken_disconnect,
	.id_table   = kraken_x62_id_table,
};

const char *kraken_driver_name = DRIVER_NAME;

module_usb_driver(kraken_x62_driver);

MODULE_DESCRIPTION("driver for 1e71:170e devices (NZXT Kraken X62)");
MODULE_LICENSE("GPL");
