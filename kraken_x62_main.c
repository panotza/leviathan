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

#define LEDS_MSG_SIZE 32

const u8 LEDS_MSG_HEADER[] = {
	0x02, 0x4c,
};

static void leds_msg_init(u8 *msg)
{
	memcpy(msg, LEDS_MSG_HEADER, sizeof LEDS_MSG_HEADER);
}

enum leds_which {
	LEDS_WHICH_LOGO = 0b001,
	LEDS_WHICH_RING = 0b010,
};

static void leds_msg_which(u8 *msg, enum leds_which which)
{
	msg[2] &= ~0b111;
	msg[2] |= (u8) which;
}

static void leds_msg_moving(u8 *msg, bool moving)
{
	msg[2] &= ~(0b1 << 3);
	msg[2] |= ((u8) moving) << 3;
}

enum leds_direction {
	LEDS_DIRECTION_CLOCKWISE        = 0b0000,
	LEDS_DIRECTION_COUNTERCLOCKWISE = 0b0001,
};

static int leds_direction_from_str(enum leds_direction *direction,
                                   const char *str)
{
	if (strcasecmp(str, "clockwise") == 0 ||
	    strcasecmp(str, "forward") == 0) {
		*direction = LEDS_DIRECTION_CLOCKWISE;
	} else if (strcasecmp(str, "counterclockwise") == 0 ||
	           strcasecmp(str, "counter_clockwise") == 0 ||
	           strcasecmp(str, "anticlockwise") == 0 ||
	           strcasecmp(str, "anti_clockwise") == 0 ||
	           strcasecmp(str, "backward") == 0) {
		*direction = LEDS_DIRECTION_COUNTERCLOCKWISE;
	} else {
		return 1;
	}
	return 0;
}

static void leds_msg_direction(u8 *msg, enum leds_direction direction)
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

static int leds_preset_from_str(enum leds_preset *preset, const char *str)
{
	if (strcasecmp(str, "fixed") == 0) {
		*preset = LEDS_PRESET_FIXED;
	} else if (strcasecmp(str, "fading") == 0) {
		*preset = LEDS_PRESET_FADING;
	} else if (strcasecmp(str, "spectrum_wave") == 0) {
		*preset = LEDS_PRESET_SPECTRUM_WAVE;
	} else if (strcasecmp(str, "marquee") == 0) {
		*preset = LEDS_PRESET_MARQUEE;
	} else if (strcasecmp(str, "covering_marquee") == 0) {
		*preset = LEDS_PRESET_COVERING_MARQUEE;
	} else if (strcasecmp(str, "alternating") == 0) {
		*preset = LEDS_PRESET_ALTERNATING;
	} else if (strcasecmp(str, "breathing") == 0) {
		*preset = LEDS_PRESET_BREATHING;
	} else if (strcasecmp(str, "pulse") == 0) {
		*preset = LEDS_PRESET_PULSE;
	} else if (strcasecmp(str, "tai_chi") == 0) {
		*preset = LEDS_PRESET_TAI_CHI;
	} else if (strcasecmp(str, "water_cooler") == 0) {
		*preset = LEDS_PRESET_WATER_COOLER;
	} else if (strcasecmp(str, "load") == 0) {
		*preset = LEDS_PRESET_LOAD;
	} else {
		return 1;
	}
	return 0;
}

static void leds_msg_preset(u8 *msg, enum leds_preset preset)
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

static int leds_interval_from_str(enum leds_interval *interval, const char *str)
{
	if (strcasecmp(str, "slowest") == 0) {
		*interval = LEDS_INTERVAL_SLOWEST;
	} else if (strcasecmp(str, "slower") == 0) {
		*interval = LEDS_INTERVAL_SLOWER;
	} else if (strcasecmp(str, "normal") == 0) {
		*interval = LEDS_INTERVAL_NORMAL;
	} else if (strcasecmp(str, "faster") == 0) {
		*interval = LEDS_INTERVAL_FASTER;
	} else if (strcasecmp(str, "fastest") == 0) {
		*interval = LEDS_INTERVAL_FASTEST;
	} else {
		return 1;
	}
	return 0;
}

static void leds_msg_interval(u8 *msg, enum leds_interval interval)
{
	msg[4] &= ~0b111;
	msg[4] |= (u8) interval;
}

static void leds_msg_group_size(u8 *msg, u8 group_size)
{
	group_size = (group_size - 3) & 0b11;
	msg[4] &= ~(0b11 << 3);
	msg[4] |= group_size << 3;
}

static void leds_msg_cycle(u8 *msg, u8 cycle)
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

static int led_color_from_str(struct led_color *color, const char *str)
{
	char hex[7];
	unsigned long long rgb;
	int ret;
	switch (strlen(str)) {
	case 3:
		// RGB, representing RRGGBB
		hex[0] = str[0];
		hex[1] = str[0];
		hex[2] = str[1];
		hex[3] = str[1];
		hex[4] = str[2];
		hex[5] = str[2];
		break;
	case 6:
		// RrGgBb
		memcpy(hex, str, 6);
		break;
	default:
		return 1;
	}
	hex[6] = '\0';

	ret = kstrtoull(hex, 16, &rgb);
	if (ret) {
		return ret;
	}
	color->red   = (rgb >> 16) & 0xff;
	color->green = (rgb >>  8) & 0xff;
	color->blue  = (rgb >>  0) & 0xff;
	return 0;
}

static void leds_msg_color_logo(u8 *msg, const struct led_color *color)
{
	// NOTE: the logo color is in GRB format
	msg[5] = color->green;
	msg[6] = color->red;
	msg[7] = color->blue;
}

#define LEDS_MSG_RING_COLORS 8

static void leds_msg_colors_ring(u8 *msg, const struct led_color *colors)
{
	size_t i;
	for (i = 0; i < LEDS_MSG_RING_COLORS; i++) {
		u8 *start = msg + 8 + (i * 3);
		start[0] = colors[i].red;
		start[1] = colors[i].green;
		start[2] = colors[i].blue;
	}
}

#define LED_CYCLES_MAX 8

struct led_cycles {
	u8 msgs[LED_CYCLES_MAX][LEDS_MSG_SIZE];
	// first len messages in msgs are to be sent when updating (0 means do
	// not update the LEDs)
	u8 len;
	struct mutex mutex;
};

static void led_cycles_init(struct led_cycles *cycles, enum leds_which which)
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
		hex_dump_to_buffer(data->status_msg, DATA_STATUS_MSG_SIZE, 32, 1,
		                   status_hex, sizeof status_hex, false);
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

static int kraken_x62_update_led_cycles(struct usb_kraken *kraken,
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

#define WORD_LEN_MAX 64

static int str_scan_word(const char **buf, char *word)
{
	// NOTE: linux's vsscanf() currently contains a bug where conversion
	// specification 'n' does not take into account length modifiers (such
	// as in "%zn") when assigning to the corresponding pointer, so we must
	// use an int in conjunction with "%n" to store the number of scanned
	// characters
	int scanned;
	int ret = sscanf(*buf, "%" __stringify(WORD_LEN_MAX) "s%n",
	                 word, &scanned);
	*buf += scanned;
	// NOTE: linux's vsscanf() currently contains a bug where conversion
	// specification 's' accepts a buffer with only whitespace in it and
	// parses it as the empty string; we have to check that the parsed word
	// is not empty
	return ret != 1 || word[0] == '\0';
}

struct leds_store {
	struct device *dev;
	struct device_attribute *attr;

	enum leds_preset preset;
	u8 cycles;
	bool moving;
	enum leds_direction direction;
	enum leds_interval interval;
	u8 group_size;

	void *rest;
};

static void leds_store_init(struct leds_store *store, struct device *dev,
                            struct device_attribute *attr, void *rest)
{
	store->dev = dev;
	store->attr = attr;

	store->cycles = 0;
	store->moving = false;
	store->direction = LEDS_DIRECTION_CLOCKWISE;
	store->interval = LEDS_INTERVAL_NORMAL;
	store->group_size = 3;

	store->rest = rest;
}

static void leds_store_to_msg(struct leds_store *store, u8 *leds_msg)
{
	leds_msg_preset(leds_msg, store->preset);
	leds_msg_moving(leds_msg, store->moving);
	leds_msg_direction(leds_msg, store->direction);
	leds_msg_interval(leds_msg, store->interval);
	leds_msg_group_size(leds_msg, store->group_size);
}

enum leds_store_err {
	LEDS_STORE_OK,
	LEDS_STORE_ERR_PRESET,
	LEDS_STORE_ERR_NO_VALUE,
	LEDS_STORE_ERR_INVALID,
};

typedef enum leds_store_err leds_store_key_fun(struct leds_store *store,
                                               const char **buf);

static enum leds_store_err leds_store_moving(struct leds_store *store,
                                             const char **buf)
{
	char word[WORD_LEN_MAX + 1];
	int ret;

	switch (store->preset) {
	case LEDS_PRESET_ALTERNATING:
		break;
	default:
		return LEDS_STORE_ERR_PRESET;
	}

	ret = str_scan_word(buf, word);
	if (ret) {
		return LEDS_STORE_ERR_NO_VALUE;
	}
	ret = kstrtobool(word, &store->moving);
	return ret ? LEDS_STORE_ERR_INVALID : LEDS_STORE_OK;
}

static enum leds_store_err leds_store_direction(struct leds_store *store,
                                                const char **buf)
{
	char word[WORD_LEN_MAX + 1];
	int ret;

	switch (store->preset) {
	case LEDS_PRESET_SPECTRUM_WAVE:
	case LEDS_PRESET_MARQUEE:
	case LEDS_PRESET_COVERING_MARQUEE:
		break;
	default:
		return LEDS_STORE_ERR_PRESET;
	}

	ret = str_scan_word(buf, word);
	if (ret) {
		return LEDS_STORE_ERR_NO_VALUE;
	}
	ret = leds_direction_from_str(&store->direction, word);
	return ret ? LEDS_STORE_ERR_INVALID : LEDS_STORE_OK;
}

static enum leds_store_err leds_store_interval(struct leds_store *store,
                                               const char **buf)
{
	char word[WORD_LEN_MAX + 1];
	int ret;

	switch (store->preset) {
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
		return LEDS_STORE_ERR_PRESET;
	}

	ret = str_scan_word(buf, word);
	if (ret) {
		return LEDS_STORE_ERR_NO_VALUE;
	}
	ret = leds_interval_from_str(&store->interval, word);
	return ret ? LEDS_STORE_ERR_INVALID : LEDS_STORE_OK;
}

static enum leds_store_err leds_store_group_size(struct leds_store *store,
                                                 const char **buf)
{
	int scanned;
	int ret;

	switch (store->preset) {
	case LEDS_PRESET_MARQUEE:
		break;
	default:
		return LEDS_STORE_ERR_PRESET;
	}

	ret = sscanf(*buf, "%hhu%n", &store->group_size, &scanned);
	*buf += scanned;
	if (ret != 1) {
		return LEDS_STORE_ERR_INVALID;
	}
	return LEDS_STORE_OK;
}

const char * const LEDS_STORE_KEYS_COMMON[] = {
	"moving",
	"direction",
	"interval",
	"group_size",
	NULL,
};

leds_store_key_fun * const LEDS_STORE_KEY_FUNS_COMMON[] = {
	leds_store_moving,
	leds_store_direction,
	leds_store_interval,
	leds_store_group_size,
	NULL,
};

static int leds_store_check_cycles_with_preset(struct leds_store *store)
{
	bool ok = false;
	switch (store->preset) {
	case LEDS_PRESET_FIXED:
	case LEDS_PRESET_SPECTRUM_WAVE:
	case LEDS_PRESET_MARQUEE:
	case LEDS_PRESET_WATER_COOLER:
	case LEDS_PRESET_LOAD:
		ok = store->cycles == 1;
		break;
	case LEDS_PRESET_ALTERNATING:
	case LEDS_PRESET_TAI_CHI:
		ok = store->cycles == 2;
		break;
	case LEDS_PRESET_FADING:
	case LEDS_PRESET_COVERING_MARQUEE:
	case LEDS_PRESET_BREATHING:
	case LEDS_PRESET_PULSE:
		ok = store->cycles >= 1 && store->cycles <= LED_CYCLES_MAX;
		break;
	}
	if (! ok) {
		dev_err(store->dev,
		        "%s: invalid number of cycles for given preset: %d\n",
		        store->attr->attr.name, store->cycles);
	}
	return !ok;
}

static int leds_store_keys(struct leds_store *store, const char **buf,
                           const char **keys, leds_store_key_fun **key_funs)
{
	char key[WORD_LEN_MAX + 1];
	size_t i;
	int ret;

	while (! str_scan_word(buf, key)) {
		leds_store_key_fun *key_fun = NULL;

		for (i = 0; LEDS_STORE_KEYS_COMMON[i] != NULL; i++) {
			if (strcmp(LEDS_STORE_KEYS_COMMON[i], key) == 0) {
				key_fun = LEDS_STORE_KEY_FUNS_COMMON[i];
				break;
			}
		}
		if (key_fun == NULL) {
			for (i = 0; keys[i] != NULL; i++) {
				if (strcmp(keys[i], key) == 0) {
					key_fun = key_funs[i];
					break;
				}
			}
		}
		if (key_fun == NULL) {
			dev_err(store->dev, "%s: unknown key: %s\n",
			        store->attr->attr.name, key);
			return 1;
		}

		ret = key_fun(store, buf);
		switch (ret) {
		case LEDS_STORE_OK:
			continue;
		case LEDS_STORE_ERR_PRESET:
			dev_err(store->dev,
			        "%s: illegal key for given preset: %s\n",
			        store->attr->attr.name, key);
			break;
		case LEDS_STORE_ERR_NO_VALUE:
			dev_err(store->dev, "%s: no value for key %s\n",
			        store->attr->attr.name, key);
			break;
		case LEDS_STORE_ERR_INVALID:
			dev_err(store->dev, "%s: invalid value for key %s\n",
			        store->attr->attr.name, key);
			break;
		}
		return 1;
	}
	ret = leds_store_check_cycles_with_preset(store);
	return ret;
}

static int leds_store_preset(struct leds_store *store, const char **buf)
{
	char preset_str[WORD_LEN_MAX + 1];
	int ret = str_scan_word(buf, preset_str);
	if (ret) {
		dev_err(store->dev, "%s: no preset\n", store->attr->attr.name);
		return ret;
	}
	ret = leds_preset_from_str(&store->preset, preset_str);
	if (ret) {
		dev_err(store->dev, "%s: invalid preset: %s\n",
		        store->attr->attr.name, preset_str);
		return ret;
	}
	return 0;
}

static enum leds_store_err led_logo_store_color(struct leds_store *store,
                                                const char **buf)
{
	struct led_color *cycle_colors = store->rest;
	char word[WORD_LEN_MAX + 1];

	int ret = str_scan_word(buf, word);
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
