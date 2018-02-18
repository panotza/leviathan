/* Driver for 2433:b200 devices.
 */

#include "common.h"

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>

#define DRIVER_NAME "kraken"

struct kraken_driver_data {
	bool send_color;
	u8 *color_message, *pump_message, *fan_message, *status_message;
};

static int kraken_start_transaction(struct usb_kraken *kraken)
{
	return usb_control_msg(kraken->udev, usb_sndctrlpipe(kraken->udev, 0), 2, 0x40, 0x0001, 0, NULL, 0, 1000);
}

static int kraken_send_message(struct usb_kraken *kraken, u8 *message, int length)
{
	int sent;
	int retval = usb_bulk_msg(kraken->udev, usb_sndbulkpipe(kraken->udev, 2), message, length, &sent, 3000);
	if (retval != 0)
		return retval;
	if (sent != length)
		return -EIO;
	return 0;
}

static int kraken_receive_message(struct usb_kraken *kraken, u8 message[], int expected_length)
{
	int received;
	int retval = usb_bulk_msg(kraken->udev, usb_rcvbulkpipe(kraken->udev, 2), message, expected_length, &received, 3000);
	if (retval != 0)
		return retval;
	if (received != expected_length)
		return -EIO;
	return 0;
}

void kraken_driver_update(struct usb_kraken *kraken)
{
	int retval = 0;
	struct kraken_driver_data *data = kraken->data;
	if (data->send_color) {
		if (
			(retval = kraken_start_transaction(kraken)) ||
			(retval = kraken_send_message(kraken, data->color_message, 19)) ||
			(retval = kraken_receive_message(kraken, data->status_message, 32))
		   )
			dev_err(&kraken->udev->dev, "Failed to update: %d\n", retval);
		data->send_color = false;
	}
	else {
		if (
			(retval = kraken_start_transaction(kraken)) ||
			(retval = kraken_send_message(kraken, data->pump_message, 2)) ||
			(retval = kraken_send_message(kraken, data->fan_message, 2)) ||
			(retval = kraken_receive_message(kraken, data->status_message, 32))
		   )
			dev_err(&kraken->udev->dev, "Failed to update: %d\n", retval);
	}
}

static ssize_t show_speed(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	return scnprintf(buf, PAGE_SIZE, "%u\n", data->pump_message[1]);
}

static ssize_t set_speed(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	u8 speed;
	if (sscanf(buf, "%hhu", &speed) != 1 || speed < 30 || speed > 100)
		return -EINVAL;

	data->pump_message[1] = speed;
	data->fan_message[1] = speed;

	return count;
}

static DEVICE_ATTR(speed, S_IRUGO | S_IWUSR | S_IWGRP, show_speed, set_speed);

static ssize_t show_color(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	return scnprintf(buf, PAGE_SIZE, "%02x%02x%02x\n", data->color_message[1], data->color_message[2], data->color_message[3]);
}

static ssize_t set_color(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	u8 r, g, b;
	if (sscanf(buf, "%02hhx%02hhx%02hhx", &r, &g, &b) != 3)
		return -EINVAL;

	data->color_message[1] = r;
	data->color_message[2] = g;
	data->color_message[3] = b;

	data->send_color = true;

	return count;
}

static DEVICE_ATTR(color, S_IRUGO | S_IWUSR | S_IWGRP, show_color, set_color);

static ssize_t show_alternate_color(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	return scnprintf(buf, PAGE_SIZE, "%02x%02x%02x\n", data->color_message[4], data->color_message[5], data->color_message[6]);
}

static ssize_t set_alternate_color(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	u8 r, g, b;
	if (sscanf(buf, "%02hhx%02hhx%02hhx", &r, &g, &b) != 3)
		return -EINVAL;

	data->color_message[4] = r;
	data->color_message[5] = g;
	data->color_message[6] = b;

	data->send_color = true;

	return count;
}

static DEVICE_ATTR(alternate_color, S_IRUGO | S_IWUSR | S_IWGRP, show_alternate_color, set_alternate_color);

static ssize_t show_interval(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	return scnprintf(buf, PAGE_SIZE, "%u\n", data->color_message[11]);
}

static ssize_t set_interval(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	u8 interval;
	if (sscanf(buf, "%hhu", &interval) != 1 || interval == 0)
		return -EINVAL;

	data->color_message[11] = interval; data->color_message[12] = interval;

	data->send_color = true;

	return count;
}

static DEVICE_ATTR(interval, S_IRUGO | S_IWUSR | S_IWGRP, show_interval, set_interval);

static ssize_t show_mode(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	if (data->color_message[14] == 1)
		return scnprintf(buf, PAGE_SIZE, "alternating\n");
	else if (data->color_message[15] == 1)
		return scnprintf(buf, PAGE_SIZE, "blinking\n");
	else if (data->color_message[13] == 1)
		return scnprintf(buf, PAGE_SIZE, "normal\n");
	else
		return scnprintf(buf, PAGE_SIZE, "off\n");
}

static ssize_t set_mode(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	if (strncasecmp(buf, "normal", strlen("normal")) == 0) {
		data->color_message[13] = 1;
		data->color_message[14] = 0;
		data->color_message[15] = 0;
	}
	else if (strncasecmp(buf, "alternating", strlen("alternating")) == 0) {
		data->color_message[13] = 1;
		data->color_message[14] = 1;
		data->color_message[15] = 0;
	}
	else if (strncasecmp(buf, "blinking", strlen("blinking")) == 0) {
		data->color_message[13] = 1;
		data->color_message[14] = 0;
		data->color_message[15] = 1;
	}
	else if (strncasecmp(buf, "off", strlen("off")) == 0) {
		data->color_message[13] = 0;
		data->color_message[14] = 0;
		data->color_message[15] = 0;
	}
	else
		return -EINVAL;

	data->send_color = true;

	return count;
}

static DEVICE_ATTR(mode, S_IRUGO | S_IWUSR | S_IWGRP, show_mode, set_mode);

static ssize_t show_temp(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	return scnprintf(buf, PAGE_SIZE, "%u\n", data->status_message[10]);
}

static DEVICE_ATTR(temp, S_IRUGO, show_temp, NULL);

static ssize_t show_pump(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	return scnprintf(buf, PAGE_SIZE, "%u\n", 256 * data->status_message[8] + data->status_message[9]);
}

static DEVICE_ATTR(pump, S_IRUGO, show_pump, NULL);

static ssize_t show_fan(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct kraken_driver_data *data = kraken->data;

	return scnprintf(buf, PAGE_SIZE, "%u\n", 256 * data->status_message[0] + data->status_message[1]);
}

static DEVICE_ATTR(fan, S_IRUGO, show_fan, NULL);

static void kraken_remove_device_files(struct usb_interface *interface)
{
	device_remove_file(&interface->dev, &dev_attr_speed);
	device_remove_file(&interface->dev, &dev_attr_color);
	device_remove_file(&interface->dev, &dev_attr_alternate_color);
	device_remove_file(&interface->dev, &dev_attr_interval);
	device_remove_file(&interface->dev, &dev_attr_mode);
	device_remove_file(&interface->dev, &dev_attr_temp);
	device_remove_file(&interface->dev, &dev_attr_pump);
	device_remove_file(&interface->dev, &dev_attr_fan);
}

int kraken_driver_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	struct kraken_driver_data *data;
	struct usb_kraken *kraken = usb_get_intfdata(interface);
	int retval = -ENOMEM;
	kraken->data = kzalloc(sizeof *kraken->data, GFP_KERNEL);
	if (!kraken->data)
		goto error_data;
	data = kraken->data;

	if (
		(data->color_message = kmalloc(19*sizeof(u8), GFP_KERNEL)) == NULL ||
		(data->pump_message = kmalloc(2*sizeof(u8), GFP_KERNEL)) == NULL ||
		(data->fan_message = kmalloc(2*sizeof(u8), GFP_KERNEL)) == NULL ||
		(data->status_message = kmalloc(32*sizeof(u8), GFP_KERNEL)) == NULL
	) {
		goto error_messages;
	}

	data->color_message[0] = 0x10;
	data->color_message[1] = 0x00; data->color_message[2] = 0x00; data->color_message[3] = 0xff;
	data->color_message[4] = 0x00; data->color_message[5] = 0xff; data->color_message[6] = 0x00;
	data->color_message[7] = 0x00; data->color_message[8] = 0x00; data->color_message[9] = 0x00; data->color_message[10] = 0x3c;
	data->color_message[11] = 1; data->color_message[12] = 1;
	data->color_message[13] = 1; data->color_message[14] = 0; data->color_message[15] = 0;
	data->color_message[16] = 0x00; data->color_message[17] = 0x00; data->color_message[18] = 0x01;

	data->pump_message[0] = 0x13;
	data->pump_message[1] = 50;

	data->fan_message[0] = 0x12;
	data->fan_message[1] = 50;

	if (
		(retval = device_create_file(&interface->dev, &dev_attr_speed)) ||
		(retval = device_create_file(&interface->dev, &dev_attr_color)) ||
		(retval = device_create_file(&interface->dev, &dev_attr_alternate_color)) ||
		(retval = device_create_file(&interface->dev, &dev_attr_interval)) ||
		(retval = device_create_file(&interface->dev, &dev_attr_mode)) ||
		(retval = device_create_file(&interface->dev, &dev_attr_temp)) ||
		(retval = device_create_file(&interface->dev, &dev_attr_pump)) ||
		(retval = device_create_file(&interface->dev, &dev_attr_fan))
	)
		goto error;

	retval = usb_control_msg(kraken->udev, usb_sndctrlpipe(kraken->udev, 0), 2, 0x40, 0x0002, 0, NULL, 0, 1000);
	if (retval)
		goto error;

	dev_info(&interface->dev, "Kraken connected\n");
	data->send_color = true;

	return 0;
error:
	kraken_remove_device_files(interface);
error_messages:
	kfree(data->status_message);
	kfree(data->fan_message);
	kfree(data->pump_message);
	kfree(data->color_message);

	kfree(data);
error_data:
	return retval;
}

void kraken_driver_disconnect(struct usb_interface *interface)
{
	struct usb_kraken *kraken = usb_get_intfdata(interface);
	struct kraken_driver_data *data = kraken->data;

	kraken_remove_device_files(interface);

	kfree(data->status_message);
	kfree(data->fan_message);
	kfree(data->pump_message);
	kfree(data->color_message);

	kfree(data);

	dev_info(&interface->dev, "Kraken disconnected\n");
}

static const struct usb_device_id kraken_x61_id_table[] = {
	{ USB_DEVICE(0x2433, 0xb200) },
	{ },
};

MODULE_DEVICE_TABLE(usb, kraken_x61_id_table);

static struct usb_driver kraken_x61_driver = {
	.name       = DRIVER_NAME,
	.probe      = kraken_probe,
	.disconnect = kraken_disconnect,
	.id_table   = kraken_x61_id_table,
};

const char *kraken_driver_name = DRIVER_NAME;

module_usb_driver(kraken_x61_driver);

MODULE_LICENSE("GPL");
