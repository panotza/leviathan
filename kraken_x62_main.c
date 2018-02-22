/* Driver for 1e71:170e devices.
 */

#include "common.h"

#include <asm/byteorder.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/stringify.h>
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

static inline void percent_data_init(struct percent_data *data, u8 type_byte)
{
	memcpy(data->msg, PERCENT_MSG_HEADER, sizeof PERCENT_MSG_HEADER);
	data->msg[2] = type_byte;
	mutex_init(&data->mutex);
}

static inline void percent_data_set(struct percent_data *data, u8 percent)
{
	mutex_lock(&data->mutex);
	data->msg[4] = percent;
	data->update = true;
	mutex_unlock(&data->mutex);
}

#define LEDS_MSG_SIZE 32

const u8 LEDS_MSG_HEADER[] = {
	0x02, 0x4c,
};

static inline void leds_msg_init(u8 *msg)
{
	memcpy(msg, LEDS_MSG_HEADER, sizeof LEDS_MSG_HEADER);
}

enum leds_which {
	LEDS_WHICH_LOGO = 0b001,
	LEDS_WHICH_RING = 0b010,
};

static inline void leds_msg_which(u8 *msg, enum leds_which which)
{
	msg[2] &= ~0b111;
	msg[2] |= (u8) which;
}

static inline void leds_msg_moving(u8 *msg, bool moving)
{
	msg[2] &= ~(0b1 << 3);
	msg[2] |= ((u8) moving) << 3;
}

enum leds_direction {
	LEDS_DIRECTION_CLOCKWISE        = 0b0000,
	LEDS_DIRECTION_COUNTERCLOCKWISE = 0b0001,
};

static inline enum leds_direction leds_direction_from_str(const char *str)
{
	if (strcasecmp(str, "clockwise") == 0 ||
	    strcasecmp(str, "forward") == 0) {
		return LEDS_DIRECTION_CLOCKWISE;
	} else if (strcasecmp(str, "counterclockwise") == 0 ||
	           strcasecmp(str, "counter_clockwise") == 0 ||
	           strcasecmp(str, "anticlockwise") == 0 ||
	           strcasecmp(str, "anti_clockwise") == 0 ||
	           strcasecmp(str, "backward") == 0) {
		return LEDS_DIRECTION_COUNTERCLOCKWISE;
	} else {
		return -1;
	}

}

static inline void leds_msg_direction(u8 *msg, enum leds_direction direction)
{
	msg[2] &= ~(0b1111 << 4);
	msg[2] |= ((u8) direction) << 4;
}

enum leds_preset {
	LEDS_PRESET_FIXED            = 0x00,
	LEDS_PRESET_FADING           = 0x01,
	LEDS_PRESET_SPECTRUM_WAVE    = 0x02,
	LEDS_PRESET_MARQUEE          = 0x03,
	LEDS_PRESET_COVERING_MARQUEE = 0x04,
	LEDS_PRESET_ALTERNATING      = 0x05,
	LEDS_PRESET_BREATHING        = 0x06,
	LEDS_PRESET_PULSE            = 0x07,
	LEDS_PRESET_TAI_CHI          = 0x08,
	LEDS_PRESET_WATER_COOLER     = 0x09,
	LEDS_PRESET_LOAD             = 0x0a,
};

static inline enum leds_preset leds_preset_from_str(const char *str)
{
	if (strcasecmp(str, "fixed") == 0) {
		return LEDS_PRESET_FIXED;
	} else if (strcasecmp(str, "fading") == 0) {
		return LEDS_PRESET_FADING;
	} else if (strcasecmp(str, "spectrum_wave") == 0) {
		return LEDS_PRESET_SPECTRUM_WAVE;
	} else if (strcasecmp(str, "marquee") == 0) {
		return LEDS_PRESET_MARQUEE;
	} else if (strcasecmp(str, "covering_marquee") == 0) {
		return LEDS_PRESET_COVERING_MARQUEE;
	} else if (strcasecmp(str, "alternating") == 0) {
		return LEDS_PRESET_ALTERNATING;
	} else if (strcasecmp(str, "breathing") == 0) {
		return LEDS_PRESET_BREATHING;
	} else if (strcasecmp(str, "pulse") == 0) {
		return LEDS_PRESET_PULSE;
	} else if (strcasecmp(str, "tai_chi") == 0) {
		return LEDS_PRESET_TAI_CHI;
	} else if (strcasecmp(str, "water_cooler") == 0) {
		return LEDS_PRESET_WATER_COOLER;
	} else if (strcasecmp(str, "load") == 0) {
		return LEDS_PRESET_LOAD;
	} else {
		return -1;
	}
}

static inline void leds_msg_preset(u8 *msg, enum leds_preset preset)
{
	msg[3] = (u8) preset;
}

enum leds_interval {
	LEDS_INTERVAL_SLOWEST = 0b000,
	LEDS_INTERVAL_SLOWER  = 0b001,
	LEDS_INTERVAL_NORMAL  = 0b010,
	LEDS_INTERVAL_FASTER  = 0b011,
	LEDS_INTERVAL_FASTEST = 0b100,
};

static inline enum leds_interval leds_interval_from_str(const char *str)
{
	if (strcasecmp(str, "slowest") == 0) {
		return LEDS_INTERVAL_SLOWEST;
	} else if (strcasecmp(str, "slower") == 0) {
		return LEDS_INTERVAL_SLOWER;
	} else if (strcasecmp(str, "normal") == 0) {
		return LEDS_INTERVAL_NORMAL;
	} else if (strcasecmp(str, "faster") == 0) {
		return LEDS_INTERVAL_FASTER;
	} else if (strcasecmp(str, "fastest") == 0) {
		return LEDS_INTERVAL_FASTEST;
	} else {
		return -1;
	}
}

static inline void leds_msg_interval(u8 *msg, enum leds_interval interval)
{
	msg[4] &= ~0b111;
	msg[4] |= (u8) interval;
}

static inline void leds_msg_group_size(u8 *msg, u8 group_size)
{
	group_size = (group_size - 3) & 0b11;
	msg[4] &= ~(0b11 << 3);
	msg[4] |= group_size << 3;
}

static inline void leds_msg_cycle(u8 *msg, u8 cycle)
{
	cycle &= 0b111;
	msg[4] &= ~(0b111 << 5);
	msg[4] |= cycle << 5;
}

struct led_color {
	u8 red;
	u8 green;
	u8 blue;
};

static inline void leds_msg_color_logo(u8 *msg, const struct led_color *color)
{
	// NOTE: the logo color is in GRB format
	msg[5] = color->green;
	msg[6] = color->red;
	msg[7] = color->blue;
}

static inline void leds_msg_color_ring(u8 *msg, u8 nr,
                                       const struct led_color *color)
{
	u8 *start = msg + 8 + (nr * 3);
	start[0] = color->red;
	start[1] = color->green;
	start[2] = color->blue;
}

#define LED_CYCLES_MAX 8

struct led_cycles {
	u8 msgs[LED_CYCLES_MAX][LEDS_MSG_SIZE];
	// first len messages in msgs are to be sent when updating (0 means do
	// not update the LEDs)
	u8 len;
	struct mutex mutex;
};

static inline void led_cycles_init(struct led_cycles *cycles,
                                   enum leds_which which)
{
	u8 cycle;
	for (cycle = 0; cycle < LED_CYCLES_MAX; cycle++) {
		leds_msg_init(cycles->msgs[cycle]);
		leds_msg_which(cycles->msgs[cycle], which);
		leds_msg_cycle(cycles->msgs[cycle], cycle);
	}
	cycles->len = 0;
	mutex_init(&cycles->mutex);
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

static inline void kraken_driver_data_init(struct kraken_driver_data *data)
{
	mutex_init(&data->status_mutex);
	percent_data_init(&data->percent_fan, 0x00);
	percent_data_init(&data->percent_pump, 0x40);
	led_cycles_init(&data->led_cycles_logo, LEDS_WHICH_LOGO);
	led_cycles_init(&data->led_cycles_ring, LEDS_WHICH_RING);
}

static inline int kraken_x62_update_status(struct usb_kraken *kraken,
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
		hex_dump_to_buffer(data->status_msg, DATA_STATUS_MSG_SIZE, 32, 1,
		                   status_hex, sizeof status_hex, false);
		dev_err(&kraken->udev->dev,
		        "received invalid status message: %s\n", status_hex);
		return 1;
	}
	return 0;
}

static inline int kraken_x62_update_percent(struct usb_kraken *kraken,
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

static inline int kraken_x62_update_led_cycles(struct usb_kraken *kraken,
                                               struct led_cycles *cycles)
{
	u8 cycle;
	int ret, sent;

	mutex_lock(&cycles->mutex);
	for (cycle = 0; cycle < cycles->len; cycle++) {
		ret = usb_interrupt_msg(
			kraken->udev, usb_sndctrlpipe(kraken->udev, 1),
			cycles->msgs[cycle], LEDS_MSG_SIZE, &sent, 1000);
		if (ret || sent != LEDS_MSG_SIZE) {
			cycles->len = 0;
			mutex_unlock(&cycles->mutex);
			dev_err(&kraken->udev->dev,
			        "failed to set LED cycle %u\n", cycle);
			return ret ? ret : 1;
		}
	}
	cycles->len = 0;
	mutex_unlock(&cycles->mutex);
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

static inline u8 data_temp_liquid(struct kraken_driver_data *data)
{
	u8 temp;
	mutex_lock(&data->status_mutex);
	temp = data->status_msg[1];
	mutex_unlock(&data->status_mutex);

	return temp;
}

static inline u16 data_fan_rpm(struct kraken_driver_data *data)
{
	u16 rpm_be;
	mutex_lock(&data->status_mutex);
	rpm_be = *((u16 *) (data->status_msg + 3));
	mutex_unlock(&data->status_mutex);

	return be16_to_cpu(rpm_be);
}

static inline u16 data_pump_rpm(struct kraken_driver_data *data)
{
	u16 rpm_be;
	mutex_lock(&data->status_mutex);
	rpm_be = *((u16 *) (data->status_msg + 5));
	mutex_unlock(&data->status_mutex);

	return be16_to_cpu(rpm_be);
}

// TODO figure out what this is
static inline u8 data_unknown_1(struct kraken_driver_data *data)
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

static inline int
percent_from(const char *buf, unsigned int min, unsigned int max)
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

static ssize_t
fan_percent_store(struct device *dev, struct device_attribute *attr,
                  const char *buf, size_t count)
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

static ssize_t
pump_percent_store(struct device *dev, struct device_attribute *attr,
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

#define WORD_LEN_MAX 64

static inline int leds_store_moving(const char *buf, size_t *pos,
                                    enum leds_preset preset, bool *moving)
{
	char word[WORD_LEN_MAX + 1];
	size_t scanned;
	int ret;

	switch (preset) {
	case LEDS_PRESET_ALTERNATING:
		break;
	default:
		return -EINVAL;
	}

	ret = sscanf(buf + *pos, "%" __stringify(WORD_LEN_MAX) "s%zn",
	                 word, &scanned);
	*pos += scanned;
	if (ret != 1) {
		return ret ? ret : -EINVAL;
	}
	ret = kstrtobool(word, moving);
	return ret;
}

static inline int
leds_store_direction(const char *buf, size_t *pos, enum leds_preset preset,
                     enum leds_direction *direction)
{
	char word[WORD_LEN_MAX + 1];
	size_t scanned;
	int ret;

	switch (preset) {
	case LEDS_PRESET_SPECTRUM_WAVE:
	case LEDS_PRESET_MARQUEE:
	case LEDS_PRESET_COVERING_MARQUEE:
		break;
	default:
		return -EINVAL;
	}

	ret = sscanf(buf + *pos, "%" __stringify(WORD_LEN_MAX) "s%zn",
	             word, &scanned);
	*pos += scanned;
	if (ret != 1) {
		return ret ? ret : -EINVAL;
	}
	*direction = leds_direction_from_str(word);
	if (*direction < 0) {
		return -EINVAL;
	}
	return 0;
}

static inline int
leds_store_interval(const char *buf, size_t *pos, enum leds_preset preset,
                    enum leds_interval *interval)
{
	char word[WORD_LEN_MAX + 1];
	size_t scanned;
	int ret;

	switch (preset) {
	case LEDS_PRESET_FADING:
	case LEDS_PRESET_SPECTRUM_WAVE:
	case LEDS_PRESET_MARQUEE:
	case LEDS_PRESET_COVERING_MARQUEE:
	case LEDS_PRESET_ALTERNATING:
	case LEDS_PRESET_BREATHING:
	case LEDS_PRESET_PULSE:
	case LEDS_PRESET_TAI_CHI:
	case LEDS_PRESET_WATER_COOLER:
		break;
	default:
		return -EINVAL;
	}

	ret = sscanf(buf + *pos, "%" __stringify(WORD_LEN_MAX) "s%zn",
	             word, &scanned);
	*pos += scanned;
	if (ret != 1) {
		return ret ? ret : -EINVAL;
	}
	*interval = leds_interval_from_str(word);
	if (*interval < 0) {
		return -EINVAL;
	}
	return 0;
}

static inline int leds_store_group_size(const char *buf, size_t *pos,
                                        enum leds_preset preset, u8 *group_size)
{
	size_t scanned;
	int ret;

	switch (preset) {
	case LEDS_PRESET_MARQUEE:
		break;
	default:
		return -EINVAL;
	}

	ret = sscanf(buf + *pos, "%hhu%zn", group_size, &scanned);
	*pos += scanned;
	if (ret != 1) {
		return ret ? ret : -EINVAL;
	}
	return 0;
}

static inline int leds_store_color(const char *buf, size_t *pos,
                                   struct led_color *color)
{
	u64 rgb;
	char word[WORD_LEN_MAX + 1];
	size_t scanned;
	int ret = sscanf(buf + *pos, "%" __stringify(WORD_LEN_MAX) "s%zn",
	                 word, &scanned);
	*pos += scanned;
	if (ret != 1) {
		return ret ? ret : -EINVAL;
	}
	switch (strlen(word)) {
	case 3:
		word[6] = '\0';
		word[5] = word[2];
		word[4] = word[2];
		word[3] = word[1];
		word[2] = word[1];
		word[1] = word[0];
		break;
	case 6:
		break;
	default:
		return -EINVAL;
	}
	ret = kstrtoull(word, 16, &rgb);
	if (ret) {
		return ret;
	}
	color->red   = (rgb >> 16) & 0xff;
	color->green = (rgb >>  8) & 0xff;
	color->blue  = (rgb >>  0) & 0xff;
	return 0;
}

static inline int leds_store_preset_check_cycles(enum leds_preset preset,
                                                 u8 cycles)
{
	switch (preset) {
	case LEDS_PRESET_FIXED:
	case LEDS_PRESET_SPECTRUM_WAVE:
	case LEDS_PRESET_MARQUEE:
	case LEDS_PRESET_WATER_COOLER:
	case LEDS_PRESET_LOAD:
		return cycles != 1;
		break;
	case LEDS_PRESET_ALTERNATING:
	case LEDS_PRESET_TAI_CHI:
		return cycles != 2;
		break;
	case LEDS_PRESET_FADING:
	case LEDS_PRESET_COVERING_MARQUEE:
	case LEDS_PRESET_BREATHING:
	case LEDS_PRESET_PULSE:
		return cycles < 1 || cycles > 8;
		break;
	default:
		return 1;
	}
}

static ssize_t led_logo_store(struct device *dev, struct device_attribute *attr,
                              const char *buf, size_t count)
{
	enum leds_preset preset;
	bool moving;
	enum leds_direction direction;
	enum leds_interval interval;
	struct led_color colors[LED_CYCLES_MAX];
	char key[WORD_LEN_MAX + 1];
	u8 group_size, colors_len, i;

	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	struct led_cycles *cycles = &kraken->data->led_cycles_logo;

	size_t pos = 0;
	size_t scanned;
	char preset_str[WORD_LEN_MAX + 1];
	int ret = sscanf(buf + pos, "%" __stringify(WORD_LEN_MAX) "s%zn",
	                 preset_str, &scanned);
	pos += scanned;
	if (ret != 1) {
		return -EINVAL;
	}
	preset = leds_preset_from_str(preset_str);
	if (preset < 0) {
		return -EINVAL;
	}
	switch (preset) {
	case LEDS_PRESET_FIXED:
	case LEDS_PRESET_FADING:
	case LEDS_PRESET_SPECTRUM_WAVE:
	case LEDS_PRESET_COVERING_MARQUEE:
	case LEDS_PRESET_BREATHING:
	case LEDS_PRESET_PULSE:
		break;
	default:
		return -EINVAL;
	}

	moving = false;
	direction = LEDS_DIRECTION_CLOCKWISE;
	interval = LEDS_INTERVAL_NORMAL;
	group_size = 3;
	colors_len = 0;

	while ((ret = sscanf(buf + pos, "%" __stringify(WORD_LEN_MAX) "s%zn",
	                     key, &scanned)) >= 0) {
		pos += scanned;
		if (ret != 1) {
			return -EINVAL;
		}
		if (strcasecmp(key, "moving") == 0) {
			ret = leds_store_moving(buf, &pos, preset, &moving);
		} else if (strcasecmp(key, "direction") == 0) {
			ret = leds_store_direction(buf, &pos, preset,
			                           &direction);
		} else if (strcasecmp(key, "interval") == 0) {
			ret = leds_store_interval(buf, &pos, preset, &interval);
		} else if (strcasecmp(key, "group_size") == 0) {
			ret = leds_store_group_size(buf, &pos, preset,
			                            &group_size);
		} else if (strcasecmp(key, "color") == 0) {
			if (colors_len >= LED_CYCLES_MAX) {
				return -EINVAL;
			}
			ret = leds_store_color(buf, &pos,
			                       &colors[colors_len++]);
		} else {
			ret = -EINVAL;
		}
		if (ret) {
			return ret;
		}
	}
	ret = leds_store_preset_check_cycles(preset, colors_len);
	if (ret) {
		return ret;
	}

	mutex_lock(&cycles->mutex);
	for (i = 0; i < colors_len; i++) {
		leds_msg_moving(cycles->msgs[i], moving);
		leds_msg_direction(cycles->msgs[i], direction);
		leds_msg_preset(cycles->msgs[i], preset);
		leds_msg_interval(cycles->msgs[i], interval);
		leds_msg_group_size(cycles->msgs[i], group_size);
		leds_msg_color_logo(cycles->msgs[i], &colors[i]);
	}
	cycles->len = colors_len;
	mutex_unlock(&cycles->mutex);
	return 0;
}

static DEVICE_ATTR(led_logo, S_IWUSR | S_IWGRP, NULL, led_logo_store);

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

	return 0;
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
