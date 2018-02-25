/* Handling of LED attributes.
 */

#include "leds.h"
#include "../common.h"
#include "../util.h"

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/usb.h>

const u8 LEDS_MSG_HEADER[] = {
	0x02, 0x4c,
};

static void leds_msg_init(u8 *msg)
{
	memcpy(msg, LEDS_MSG_HEADER, sizeof LEDS_MSG_HEADER);
}

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

int led_color_from_str(struct led_color *color, const char *str)
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

void leds_msg_color_logo(u8 *msg, const struct led_color *color)
{
	// NOTE: the logo color is in GRB format
	msg[5] = color->green;
	msg[6] = color->red;
	msg[7] = color->blue;
}

void leds_msg_colors_ring(u8 *msg, const struct led_color *colors)
{
	size_t i;
	for (i = 0; i < LEDS_MSG_RING_COLORS; i++) {
		u8 *start = msg + 8 + (i * 3);
		start[0] = colors[i].red;
		start[1] = colors[i].green;
		start[2] = colors[i].blue;
	}
}

void led_cycles_init(struct led_cycles *cycles, enum leds_which which)
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

int kraken_x62_update_led_cycles(struct usb_kraken *kraken,
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

void leds_store_init(struct leds_store *store, struct device *dev,
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

void leds_store_to_msg(struct leds_store *store, u8 *leds_msg)
{
	leds_msg_preset(leds_msg, store->preset);
	leds_msg_moving(leds_msg, store->moving);
	leds_msg_direction(leds_msg, store->direction);
	leds_msg_interval(leds_msg, store->interval);
	leds_msg_group_size(leds_msg, store->group_size);
}

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

static const char * const LEDS_STORE_KEYS_COMMON[] = {
	"moving",
	"direction",
	"interval",
	"group_size",
	NULL,
};

static leds_store_key_fun * const LEDS_STORE_KEY_FUNS_COMMON[] = {
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

int leds_store_keys(struct leds_store *store, const char **buf,
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

int leds_store_preset(struct leds_store *store, const char **buf)
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
