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
	int ret = -ENOMEM;
	// NOTE: the data buffer of usb_*_msg() must be DMA capable, so data
	// cannot be stack allocated.
	//
	// Space for length byte, type-of-data byte, and serial number encoded
	// UTF-16.
	const size_t data_size = 2 + (DATA_SERIAL_NUMBER_SIZE - 1) * 2;
	u8 *data = kmalloc(data_size, GFP_KERNEL);
	if (data == NULL) {
		goto error_data;
	}

	ret = usb_control_msg(
		kraken->udev, usb_rcvctrlpipe(kraken->udev, 0),
		0x06, 0x80, 0x0303, 0x0409, data, data_size, 1000);
	if (ret < 0) {
		dev_err(&kraken->udev->dev,
		        "failed control message: %d\n", ret);
		goto error_control_msg;
	}
	len = data[0] - 2;
	if (ret < 2 || data[1] != 0x03 || len % 2 != 0) {
		dev_err(&kraken->udev->dev,
		        "data received is invalid: %d, %u, %#02x\n",
		        ret, data[0], data[1]);
		ret = 1;
		goto error_control_msg;
	}
	len /= 2;
	if (len > DATA_SERIAL_NUMBER_SIZE - 1) {
		dev_err(&kraken->udev->dev,
		        "data received is too long: %u\n", len);
		ret = 1;
		goto error_control_msg;
	}
	// convert UTF-16 serial to null-terminated ASCII string
	for (i = 0; i < len; i++) {
		const u8 index_low = 2 + 2 * i;
		serial_number[i] = data[index_low];
		if (data[index_low + 1] != 0x00) {
			dev_err(&kraken->udev->dev,
			        "serial number contains non-ASCII character: "
			        "UTF-16 %#02x%02x, at index %u\n",
			        data[index_low + 1], data[index_low],
			        index_low);
			ret = 1;
			goto error_control_msg;
		}
	}
	serial_number[i] = '\0';

	ret = 0;
error_control_msg:
	kfree(data);
error_data:
	return ret;
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
		dev_err(&interface->dev, "failed to initialize: %d\n", ret);
		goto error_init_message;
	}

	ret = kraken_x62_create_device_files(interface);
	if (ret) {
		dev_err(&interface->dev,
		        "failed to create device files: %d\n", ret);
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
