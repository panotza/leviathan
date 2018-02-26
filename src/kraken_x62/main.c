/* Driver for 1e71:170e devices.
 */

#include "led.h"
#include "percent.h"
#include "status.h"
#include "../common.h"
#include "../util.h"

#include <asm/byteorder.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/usb.h>

#define DRIVER_NAME "kraken_x62"

#define DATA_SERIAL_NUMBER_SIZE 65

#define PERCENT_FAN_MIN      35
#define PERCENT_FAN_MAX     100
#define PERCENT_FAN_DEFAULT  35

#define PERCENT_PUMP_MIN      50
#define PERCENT_PUMP_MAX     100
#define PERCENT_PUMP_DEFAULT  60

struct kraken_driver_data {
	char serial_number[DATA_SERIAL_NUMBER_SIZE];

	struct status_data status;

	struct percent_data percent_fan;
	struct percent_data percent_pump;

	struct led_data led_logo;
	struct led_data leds_ring;
};

static void kraken_driver_data_init(struct kraken_driver_data *data)
{
	status_data_init(&data->status);
	percent_data_init(&data->percent_fan, 0x00);
	percent_data_set(&data->percent_fan, PERCENT_FAN_DEFAULT);
	percent_data_init(&data->percent_pump, 0x40);
	percent_data_set(&data->percent_pump, PERCENT_PUMP_DEFAULT);
	led_data_init(&data->led_logo, LED_WHICH_LOGO);
	led_data_init(&data->leds_ring, LED_WHICH_RING);
}

int kraken_driver_update(struct usb_kraken *kraken)
{
	struct kraken_driver_data *data = kraken->data;

	int ret;
	if ((ret = kraken_x62_update_status(kraken, &data->status)) ||
	    (ret = kraken_x62_update_percent(kraken, &data->percent_fan)) ||
	    (ret = kraken_x62_update_percent(kraken, &data->percent_pump)) ||
	    (ret = kraken_x62_update_led(kraken, &data->led_logo)) ||
	    (ret = kraken_x62_update_led(kraken, &data->leds_ring))) {
		return ret;
	}
	return 0;
}

static ssize_t serial_no_show(struct device *dev, struct device_attribute *attr,
                              char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	return scnprintf(buf, PAGE_SIZE, "%s\n", kraken->data->serial_number);
}

static DEVICE_ATTR_RO(serial_no);

static ssize_t temp_liquid_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct status_data *status = &kraken->data->status;
	return scnprintf(buf, PAGE_SIZE,
	                 "%u\n", status_data_temp_liquid(status));
}

static DEVICE_ATTR_RO(temp_liquid);

static ssize_t fan_rpm_show(struct device *dev, struct device_attribute *attr,
                            char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct status_data *status = &kraken->data->status;
	return scnprintf(buf, PAGE_SIZE, "%u\n", status_data_fan_rpm(status));
}

static DEVICE_ATTR_RO(fan_rpm);

static ssize_t pump_rpm_show(struct device *dev, struct device_attribute *attr,
                             char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct status_data *status = &kraken->data->status;
	return scnprintf(buf, PAGE_SIZE, "%u\n", status_data_pump_rpm(status));
}

static DEVICE_ATTR_RO(pump_rpm);

static ssize_t unknown_1_show(struct device *dev, struct device_attribute *attr,
                              char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct status_data *status = &kraken->data->status;
	return scnprintf(buf, PAGE_SIZE, "%u\n", status_data_unknown_1(status));
}

static DEVICE_ATTR_RO(unknown_1);

static ssize_t fan_percent_store(struct device *dev,
                                 struct device_attribute *attr, const char *buf,
                                 size_t count)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));

	int percent = percent_from(buf, PERCENT_FAN_MIN, PERCENT_FAN_MAX);
	if (percent < 0) {
		return percent;
	}
	percent_data_set(&kraken->data->percent_fan, percent);
	return count;
}

static DEVICE_ATTR(fan_percent, S_IWUSR | S_IWGRP, NULL, fan_percent_store);

static ssize_t pump_percent_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));

	int percent = percent_from(buf, PERCENT_PUMP_MIN, PERCENT_PUMP_MAX);
	if (percent < 0) {
		return percent;
	}
	percent_data_set(&kraken->data->percent_pump, percent);
	return count;
}

static DEVICE_ATTR(pump_percent, S_IWUSR | S_IWGRP, NULL, pump_percent_store);

static enum led_parser_ret led_parser_key_color(struct led_parser *parser,
                                                const char **buf)
{
	struct led_color *cycle_colors = parser->custom;
	char word[WORD_LEN_MAX + 1];
	int ret;

	if (parser->cycles == LED_DATA_CYCLES_SIZE) {
		dev_err(parser->dev, "%s: more than %u cycles\n",
		        parser->attr->attr.name, LED_DATA_CYCLES_SIZE);
		return LED_PARSER_RET_INVALID;
	}
	ret = str_scan_word(buf, word);
	if (ret) {
		return LED_PARSER_RET_NO_VALUE;
	}
	ret = led_color_from_str(&cycle_colors[parser->cycles], word);
	if (ret) {
		return LED_PARSER_RET_INVALID;
	}
	parser->cycles++;
	return LED_PARSER_RET_OK;
}

static ssize_t led_logo_store(struct device *dev, struct device_attribute *attr,
                              const char *buf, size_t count)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct led_data *led_data = &kraken->data->led_logo;

	size_t i;
	int ret;
	const char *keys[] = {
		"color",              NULL,
	};
	led_parser_key_fn *key_fns[] = {
		led_parser_key_color, NULL,
	};
	struct led_color cycle_colors[LED_DATA_CYCLES_SIZE];
	struct led_parser parser;
	led_parser_init(&parser, dev, attr, cycle_colors);

	ret = led_parser_preset(&parser, &buf);
	if (ret) {
		return -EINVAL;
	}
	switch (parser.preset) {
	case LED_PRESET_FIXED:
	case LED_PRESET_FADING:
	case LED_PRESET_SPECTRUM_WAVE:
	case LED_PRESET_COVERING_MARQUEE:
	case LED_PRESET_BREATHING:
	case LED_PRESET_PULSE:
		break;
	default:
		dev_err(dev, "%s: illegal preset for logo LED\n",
		        attr->attr.name);
		return -EINVAL;
	}

	ret = led_parser_keys(&parser, &buf, keys, key_fns);
	if (ret) {
		return -EINVAL;
	}

	mutex_lock(&led_data->mutex);
	for (i = 0; i < parser.cycles; i++) {
		led_parser_to_msg(&parser, &led_data->cycles[i]);
		led_msg_color_logo(&led_data->cycles[i], &cycle_colors[i]);
	}
	led_data->len = parser.cycles;
	mutex_unlock(&led_data->mutex);
	return count;
}

static DEVICE_ATTR(led_logo, S_IWUSR | S_IWGRP, NULL, led_logo_store);

static enum led_parser_ret led_parser_key_colors(struct led_parser *parser,
                                                 const char **buf)
{
	// NOTE: this must be a pointer to arrays of size LED_MSG_COLORS_RING; a
	// pointer to pointer to struct doesn't work here because store->custom
	// points to a 2-dimensional array, not an array of pointers
	struct led_color (*cycle_colors)[LED_MSG_COLORS_RING] = parser->custom;
	char word[WORD_LEN_MAX + 1];
	struct led_color *colors;
	size_t i;
	int ret;

	if (parser->cycles == LED_DATA_CYCLES_SIZE) {
		dev_err(parser->dev, "%s: more than %u cycles\n",
		        parser->attr->attr.name, LED_DATA_CYCLES_SIZE);
		return LED_PARSER_RET_INVALID;
	}
	colors = cycle_colors[parser->cycles];
	for (i = 0; i < LED_MSG_COLORS_RING; i++) {
		ret = str_scan_word(buf, word);
		if (ret) {
			return (i == 0) ? LED_PARSER_RET_NO_VALUE
				: LED_PARSER_RET_INVALID;
		}
		ret = led_color_from_str(&colors[i], word);
		if (ret) {
			return LED_PARSER_RET_INVALID;
		}
	}
	parser->cycles++;
	return LED_PARSER_RET_OK;
}

static ssize_t leds_ring_store(struct device *dev,
                               struct device_attribute *attr, const char *buf,
                               size_t count)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct led_data *led_data = &kraken->data->leds_ring;

	size_t i;
	int ret;
	const char *keys[] = {
		"colors",              NULL,
	};
	led_parser_key_fn *key_fns[] = {
		led_parser_key_colors, NULL,
	};
	struct led_color
		cycle_colors[LED_DATA_CYCLES_SIZE][LED_MSG_COLORS_RING];
	struct led_parser parser;
	led_parser_init(&parser, dev, attr, cycle_colors);

	ret = led_parser_preset(&parser, &buf);
	if (ret) {
		return -EINVAL;
	}
	// ring LEDs may be set to any of the presets

	ret = led_parser_keys(&parser, &buf, keys, key_fns);
	if (ret) {
		return -EINVAL;
	}

	mutex_lock(&led_data->mutex);
	for (i = 0; i < parser.cycles; i++) {
		led_parser_to_msg(&parser, &led_data->cycles[i]);
		led_msg_colors_ring(&led_data->cycles[i], cycle_colors[i]);
	}
	led_data->len = parser.cycles;
	mutex_unlock(&led_data->mutex);
	return count;
}

static DEVICE_ATTR(leds_ring, S_IWUSR | S_IWGRP, NULL, leds_ring_store);

int kraken_driver_create_device_files(struct usb_interface *interface)
{
	int ret;
	if ((ret = device_create_file(&interface->dev, &dev_attr_serial_no)))
		goto error_serial_no;
	if ((ret = device_create_file(&interface->dev, &dev_attr_temp_liquid)))
		goto error_temp_liquid;
	if ((ret = device_create_file(&interface->dev, &dev_attr_fan_rpm)))
		goto error_fan_rpm;
	if ((ret = device_create_file(&interface->dev, &dev_attr_pump_rpm)))
		goto error_pump_rpm;
	if ((ret = device_create_file(&interface->dev, &dev_attr_unknown_1)))
		goto error_unknown_1;
	if ((ret = device_create_file(&interface->dev, &dev_attr_fan_percent)))
		goto error_fan_percent;
	if ((ret = device_create_file(&interface->dev, &dev_attr_pump_percent)))
		goto error_pump_percent;
	if ((ret = device_create_file(&interface->dev, &dev_attr_led_logo)))
		goto error_led_logo;
	if ((ret = device_create_file(&interface->dev, &dev_attr_leds_ring)))
		goto error_leds_ring;

	return 0;
error_leds_ring:
	device_remove_file(&interface->dev, &dev_attr_led_logo);
error_led_logo:
	device_remove_file(&interface->dev, &dev_attr_pump_percent);
error_pump_percent:
	device_remove_file(&interface->dev, &dev_attr_fan_percent);
error_fan_percent:
	device_remove_file(&interface->dev, &dev_attr_unknown_1);
error_unknown_1:
	device_remove_file(&interface->dev, &dev_attr_pump_rpm);
error_pump_rpm:
	device_remove_file(&interface->dev, &dev_attr_fan_rpm);
error_fan_rpm:
	device_remove_file(&interface->dev, &dev_attr_temp_liquid);
error_temp_liquid:
	device_remove_file(&interface->dev, &dev_attr_serial_no);
error_serial_no:
	return ret;
}

void kraken_driver_remove_device_files(struct usb_interface *interface)
{
	device_remove_file(&interface->dev, &dev_attr_leds_ring);
	device_remove_file(&interface->dev, &dev_attr_led_logo);
	device_remove_file(&interface->dev, &dev_attr_pump_percent);
	device_remove_file(&interface->dev, &dev_attr_fan_percent);
	device_remove_file(&interface->dev, &dev_attr_unknown_1);
	device_remove_file(&interface->dev, &dev_attr_pump_rpm);
	device_remove_file(&interface->dev, &dev_attr_fan_rpm);
	device_remove_file(&interface->dev, &dev_attr_temp_liquid);
	device_remove_file(&interface->dev, &dev_attr_serial_no);
}

static int kraken_x62_initialize(struct usb_kraken *kraken,
                                 char serial_number[])
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
	u8 *data = kmalloc(data_size, GFP_KERNEL | GFP_DMA);
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
	kraken->data = kzalloc(sizeof *kraken->data, GFP_KERNEL | GFP_DMA);
	if (kraken->data == NULL) {
		goto error_data;
	}
	data = kraken->data;

	kraken_driver_data_init(data);

	ret = kraken_x62_initialize(kraken, data->serial_number);
	if (ret) {
		dev_err(&interface->dev, "failed to initialize: %d\n", ret);
		goto error_init_message;
	}

	dev_info(&interface->dev, "device connected\n");

	return 0;
error_init_message:
	kfree(data);
error_data:
	return ret;
}

void kraken_driver_disconnect(struct usb_interface *interface)
{
	struct usb_kraken *kraken = usb_get_intfdata(interface);
	struct kraken_driver_data *data = kraken->data;

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
