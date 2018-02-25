/* Driver for 1e71:170e devices.
 */

#include "leds.h"
#include "../common.h"
#include "../util.h"

#include <asm/byteorder.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/usb.h>

#define DRIVER_NAME "kraken_x62"

#define PERCENT_MSG_SIZE 5

const u8 PERCENT_MSG_HEADER[] = {
	0x02, 0x4d,
};

struct percent_data {
	u8 msg[PERCENT_MSG_SIZE];
	bool update;
	struct mutex mutex;
};

static void percent_data_init(struct percent_data *data, u8 type_byte)
{
	memcpy(data->msg, PERCENT_MSG_HEADER, sizeof PERCENT_MSG_HEADER);
	data->msg[2] = type_byte;
	mutex_init(&data->mutex);
}

static void percent_data_set(struct percent_data *data, u8 percent)
{
	mutex_lock(&data->mutex);
	data->msg[4] = percent;
	data->update = true;
	mutex_unlock(&data->mutex);
}

#define DATA_SERIAL_NUMBER_SIZE 65
#define DATA_STATUS_MSG_SIZE    17

const u8 DATA_STATUS_MSG_HEADER[] = {
	0x04,
};
const u8 DATA_STATUS_MSG_FOOTER[] = {
	0x00, 0x00, 0x00, 0x78, 0x02, 0x00, 0x01, 0x08, 0x1e, 0x00,
};

struct kraken_driver_data {
	char serial_number[DATA_SERIAL_NUMBER_SIZE];

	u8 status_msg[DATA_STATUS_MSG_SIZE];
	struct mutex status_mutex;

	struct percent_data percent_fan;
	struct percent_data percent_pump;

	struct led_cycles led_cycles_logo;
	struct led_cycles led_cycles_ring;
};

static void kraken_driver_data_init(struct kraken_driver_data *data)
{
	mutex_init(&data->status_mutex);
	percent_data_init(&data->percent_fan, 0x00);
	percent_data_init(&data->percent_pump, 0x40);
	led_cycles_init(&data->led_cycles_logo, LEDS_WHICH_LOGO);
	led_cycles_init(&data->led_cycles_ring, LEDS_WHICH_RING);
}

static int kraken_x62_update_status(struct usb_kraken *kraken,
                                    struct kraken_driver_data *data)
{
	int received;
	int ret;
	mutex_lock(&data->status_mutex);
	ret = usb_interrupt_msg(
		kraken->udev, usb_rcvctrlpipe(kraken->udev, 1),
		data->status_msg, DATA_STATUS_MSG_SIZE, &received, 1000);
	mutex_unlock(&data->status_mutex);

	if (ret || received != DATA_STATUS_MSG_SIZE) {
		dev_err(&kraken->udev->dev,
		        "failed status update: I/O error\n");
		return ret ? ret : 1;
	}
	if (memcmp(data->status_msg + 0, DATA_STATUS_MSG_HEADER,
	           sizeof DATA_STATUS_MSG_HEADER) != 0 ||
	    memcmp(data->status_msg +
	           DATA_STATUS_MSG_SIZE - sizeof DATA_STATUS_MSG_FOOTER,
	           DATA_STATUS_MSG_FOOTER,
	           sizeof DATA_STATUS_MSG_FOOTER) != 0) {
		char status_hex[DATA_STATUS_MSG_SIZE * 3 + 1];
		hex_dump_to_buffer(data->status_msg, DATA_STATUS_MSG_SIZE, 32,
		                   1, status_hex, sizeof status_hex, false);
		dev_err(&kraken->udev->dev,
		        "received invalid status message: %s\n", status_hex);
		return 1;
	}
	return 0;
}

static int kraken_x62_update_percent(struct usb_kraken *kraken,
                                     struct percent_data *data)
{
	int ret, sent;

	mutex_lock(&data->mutex);
	if (! data->update) {
		mutex_unlock(&data->mutex);
		return 0;
	}
	ret = usb_interrupt_msg(kraken->udev, usb_sndctrlpipe(kraken->udev, 1),
	                        data->msg, PERCENT_MSG_SIZE, &sent, 1000);
	data->update = false;
	mutex_unlock(&data->mutex);

	if (ret || sent != PERCENT_MSG_SIZE) {
		dev_err(&kraken->udev->dev,
		        "failed to set speed percent: I/O error\n");
		return ret ? ret : 1;
	}
	return 0;
}

int kraken_driver_update(struct usb_kraken *kraken)
{
	struct kraken_driver_data *data = kraken->data;

	int ret;
	if ((ret = kraken_x62_update_status(kraken, data)) ||
	    (ret = kraken_x62_update_percent(kraken, &data->percent_fan)) ||
	    (ret = kraken_x62_update_percent(kraken, &data->percent_pump)) ||
	    (ret = kraken_x62_update_led_cycles(kraken,
	                                        &data->led_cycles_logo)) ||
	    (ret = kraken_x62_update_led_cycles(kraken,
	                                        &data->led_cycles_ring))) {
		return ret;
	}
	return 0;
}

static u8 data_temp_liquid(struct kraken_driver_data *data)
{
	u8 temp;
	mutex_lock(&data->status_mutex);
	temp = data->status_msg[1];
	mutex_unlock(&data->status_mutex);

	return temp;
}

static u16 data_fan_rpm(struct kraken_driver_data *data)
{
	u16 rpm_be;
	mutex_lock(&data->status_mutex);
	rpm_be = *((u16 *) (data->status_msg + 3));
	mutex_unlock(&data->status_mutex);

	return be16_to_cpu(rpm_be);
}

static u16 data_pump_rpm(struct kraken_driver_data *data)
{
	u16 rpm_be;
	mutex_lock(&data->status_mutex);
	rpm_be = *((u16 *) (data->status_msg + 5));
	mutex_unlock(&data->status_mutex);

	return be16_to_cpu(rpm_be);
}

// TODO figure out what this is
static u8 data_unknown_1(struct kraken_driver_data *data)
{
	u8 unknown_1;
	mutex_lock(&data->status_mutex);
	unknown_1 = data->status_msg[2];
	mutex_unlock(&data->status_mutex);

	return unknown_1;
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
	return scnprintf(buf, PAGE_SIZE,
	                 "%u\n", data_temp_liquid(kraken->data));
}

static DEVICE_ATTR_RO(temp_liquid);

static ssize_t fan_rpm_show(struct device *dev, struct device_attribute *attr,
                            char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	return scnprintf(buf, PAGE_SIZE, "%u\n", data_fan_rpm(kraken->data));
}

static DEVICE_ATTR_RO(fan_rpm);

static ssize_t pump_rpm_show(struct device *dev, struct device_attribute *attr,
                             char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	return scnprintf(buf, PAGE_SIZE, "%u\n", data_pump_rpm(kraken->data));
}

static DEVICE_ATTR_RO(pump_rpm);

static ssize_t unknown_1_show(struct device *dev, struct device_attribute *attr,
                              char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	return scnprintf(buf, PAGE_SIZE, "%u\n", data_unknown_1(kraken->data));
}

static DEVICE_ATTR_RO(unknown_1);

static int percent_from(const char *buf, unsigned int min, unsigned int max)
{
	unsigned int percent;
	int ret = kstrtouint(buf, 0, &percent);
	if (ret) {
		return ret;
	}
	if (percent < min) {
		return min;
	}
	if (percent > max) {
		return max;
	}
	return percent;
}

#define PERCENT_FAN_MIN      35
#define PERCENT_FAN_MAX     100
#define PERCENT_FAN_DEFAULT  35

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

#define PERCENT_PUMP_MIN      50
#define PERCENT_PUMP_MAX     100
#define PERCENT_PUMP_DEFAULT  60

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

static enum leds_store_err led_logo_store_color(struct leds_store *store,
                                                const char **buf)
{
	struct led_color *cycle_colors = store->rest;
	char word[WORD_LEN_MAX + 1];
	int ret;

	if (store->cycles == LED_CYCLES_MAX) {
		dev_err(store->dev, "%s: more than %u cycles\n",
		        store->attr->attr.name, LED_CYCLES_MAX);
		return LEDS_STORE_ERR_INVALID;
	}
	ret = str_scan_word(buf, word);
	if (ret) {
		return LEDS_STORE_ERR_NO_VALUE;
	}
	ret = led_color_from_str(&cycle_colors[store->cycles], word);
	if (ret) {
		return LEDS_STORE_ERR_INVALID;
	}
	store->cycles++;
	return LEDS_STORE_OK;
}

static ssize_t led_logo_store(struct device *dev, struct device_attribute *attr,
                              const char *buf, size_t count)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct led_cycles *cycles = &kraken->data->led_cycles_logo;

	size_t i;
	int ret;
	const char *keys[] = {
		"color",              NULL,
	};
	leds_store_key_fun *key_funs[] = {
		led_logo_store_color, NULL,
	};
	struct led_color cycle_colors[LED_CYCLES_MAX];
	struct leds_store store;
	leds_store_init(&store, dev, attr, cycle_colors);

	ret = leds_store_preset(&store, &buf);
	if (ret) {
		return -EINVAL;
	}
	switch (store.preset) {
	case LEDS_PRESET_FIXED:
	case LEDS_PRESET_FADING:
	case LEDS_PRESET_SPECTRUM_WAVE:
	case LEDS_PRESET_COVERING_MARQUEE:
	case LEDS_PRESET_BREATHING:
	case LEDS_PRESET_PULSE:
		break;
	default:
		dev_err(dev, "%s: illegal preset for logo LED\n",
		        attr->attr.name);
		return -EINVAL;
	}

	ret = leds_store_keys(&store, &buf, keys, key_funs);
	if (ret) {
		return -EINVAL;
	}

	mutex_lock(&cycles->mutex);
	for (i = 0; i < store.cycles; i++) {
		leds_store_to_msg(&store, cycles->msgs[i]);
		leds_msg_color_logo(cycles->msgs[i], &cycle_colors[i]);
	}
	cycles->len = store.cycles;
	mutex_unlock(&cycles->mutex);
	return count;
}

static DEVICE_ATTR(led_logo, S_IWUSR | S_IWGRP, NULL, led_logo_store);

static enum leds_store_err leds_ring_store_colors(struct leds_store *store,
                                                  const char **buf)
{
	// NOTE: this is a pointer to arrays of size LEDS_MSG_RING_COLORS; a
	// pointer to pointer to struct doesn't work here because store->rest
	// points to a 2-dimensional array, not an array of pointers
	struct led_color (*cycle_colors)[LEDS_MSG_RING_COLORS] = store->rest;
	char word[WORD_LEN_MAX + 1];
	size_t i;
	int ret;

	if (store->cycles == LED_CYCLES_MAX) {
		dev_err(store->dev, "%s: more than %u cycles\n",
		        store->attr->attr.name, LED_CYCLES_MAX);
		return LEDS_STORE_ERR_INVALID;
	}
	for (i = 0; i < LEDS_MSG_RING_COLORS; i++) {
		ret = str_scan_word(buf, word);
		if (ret) {
			return (i == 0) ? LEDS_STORE_ERR_NO_VALUE
				: LEDS_STORE_ERR_INVALID;
		}
		ret = led_color_from_str(&cycle_colors[store->cycles][i], word);
		if (ret) {
			return LEDS_STORE_ERR_INVALID;
		}
	}
	store->cycles++;
	return LEDS_STORE_OK;
}

static ssize_t leds_ring_store(struct device *dev,
                               struct device_attribute *attr, const char *buf,
                               size_t count)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct led_cycles *cycles = &kraken->data->led_cycles_ring;

	size_t i;
	int ret;
	const char *keys[] = {
		"colors",               NULL,
	};
	leds_store_key_fun *key_funs[] = {
		leds_ring_store_colors, NULL,
	};
	struct led_color cycle_colors[LED_CYCLES_MAX][LEDS_MSG_RING_COLORS];
	struct leds_store store;
	leds_store_init(&store, dev, attr, cycle_colors);

	ret = leds_store_preset(&store, &buf);
	if (ret) {
		return -EINVAL;
	}
	// ring LEDs may be set to any of the presets

	ret = leds_store_keys(&store, &buf, keys, key_funs);
	if (ret) {
		return -EINVAL;
	}

	mutex_lock(&cycles->mutex);
	for (i = 0; i < store.cycles; i++) {
		leds_store_to_msg(&store, cycles->msgs[i]);
		leds_msg_colors_ring(cycles->msgs[i], cycle_colors[i]);
	}
	cycles->len = store.cycles;
	mutex_unlock(&cycles->mutex);
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

	percent_data_set(&data->percent_fan, PERCENT_FAN_DEFAULT);
	percent_data_set(&data->percent_pump, PERCENT_PUMP_DEFAULT);

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
