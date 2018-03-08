/* Handling of LED attributes.
 */

#include "led.h"

#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/usb.h>

const u8 LED_MSG_HEADER[] = {
	0x02, 0x4c,
};

static void led_msg_init(struct led_msg *msg)
{
	memcpy(msg->msg, LED_MSG_HEADER, sizeof(LED_MSG_HEADER));
}

static void led_msg_which(struct led_msg *msg, enum led_which which)
{
	msg->msg[2] &= ~0b111;
	msg->msg[2] |= (u8) which;
}

enum led_which led_msg_which_get(const struct led_msg *msg)
{
	const enum led_which which = msg->msg[2] & 0b111;
	return which;
}

int led_moving_from_str(bool *moving, const char *str)
{
	int ret;
	if (strcasecmp(str, "*") == 0) {
		*moving = LED_MOVING_DEFAULT;
		return 0;
	}
	ret = kstrtobool(str, moving);
	return ret;
}

void led_msg_moving(struct led_msg *msg, bool moving)
{
	msg->msg[2] &= ~(0b1 << 3);
	msg->msg[2] |= ((u8) moving) << 3;
}

bool led_msg_moving_is_legal(const struct led_msg *msg, bool moving) {
	if (moving == LED_MOVING_DEFAULT)
		return true;
	switch (led_msg_preset_get(msg)) {
	case LED_PRESET_ALTERNATING:
		return true;
	default:
		return false;
	}
}

int led_direction_from_str(enum led_direction *direction, const char *str)
{
	if (strcasecmp(str, "*") == 0) {
		*direction = LED_DIRECTION_DEFAULT;
		return 0;
	}
	if (strcasecmp(str, "clockwise") == 0 ||
	    strcasecmp(str, "forward") == 0)
		*direction = LED_DIRECTION_CLOCKWISE;
	else if (strcasecmp(str, "counterclockwise") == 0 ||
	           strcasecmp(str, "counter_clockwise") == 0 ||
	           strcasecmp(str, "anticlockwise") == 0 ||
	           strcasecmp(str, "anti_clockwise") == 0 ||
	           strcasecmp(str, "backward") == 0)
		*direction = LED_DIRECTION_COUNTERCLOCKWISE;
	else
		return 1;
	return 0;
}

void led_msg_direction(struct led_msg *msg, enum led_direction direction)
{
	msg->msg[2] &= ~(0b1111 << 4);
	msg->msg[2] |= ((u8) direction) << 4;
}

bool led_msg_direction_is_legal(const struct led_msg *msg,
                                enum led_direction direction)
{
	if (direction == LED_DIRECTION_DEFAULT)
		return true;
	switch (led_msg_preset_get(msg)) {
	case LED_PRESET_SPECTRUM_WAVE:
	case LED_PRESET_MARQUEE:
	case LED_PRESET_COVERING_MARQUEE:
		return true;
	default:
		return false;
	}
}

int led_preset_from_str(enum led_preset *preset, const char *str)
{
	if (strcasecmp(str, "fixed") == 0)
		*preset = LED_PRESET_FIXED;
	else if (strcasecmp(str, "fading") == 0)
		*preset = LED_PRESET_FADING;
	else if (strcasecmp(str, "spectrum_wave") == 0)
		*preset = LED_PRESET_SPECTRUM_WAVE;
	else if (strcasecmp(str, "marquee") == 0)
		*preset = LED_PRESET_MARQUEE;
	else if (strcasecmp(str, "covering_marquee") == 0)
		*preset = LED_PRESET_COVERING_MARQUEE;
	else if (strcasecmp(str, "alternating") == 0)
		*preset = LED_PRESET_ALTERNATING;
	else if (strcasecmp(str, "breathing") == 0)
		*preset = LED_PRESET_BREATHING;
	else if (strcasecmp(str, "pulse") == 0)
		*preset = LED_PRESET_PULSE;
	else if (strcasecmp(str, "tai_chi") == 0)
		*preset = LED_PRESET_TAI_CHI;
	else if (strcasecmp(str, "water_cooler") == 0)
		*preset = LED_PRESET_WATER_COOLER;
	else if (strcasecmp(str, "load") == 0)
		*preset = LED_PRESET_LOAD;
	else
		return 1;
	return 0;
}

void led_msg_preset(struct led_msg *msg, enum led_preset preset)
{
	msg->msg[3] = (u8) preset;
}

enum led_preset led_msg_preset_get(const struct led_msg *msg)
{
	const enum led_preset preset = msg->msg[3];
	return preset;
}

bool led_msg_preset_is_legal(const struct led_msg *msg, enum led_preset preset)
{
	// ring leds accept any preset
	if (led_msg_which_get(msg) == LED_WHICH_RING)
		return true;
	// logo led accepts only the following presets:
	switch (preset) {
	case LED_PRESET_FIXED:
	case LED_PRESET_FADING:
	case LED_PRESET_SPECTRUM_WAVE:
	case LED_PRESET_COVERING_MARQUEE:
	case LED_PRESET_BREATHING:
	case LED_PRESET_PULSE:
		return true;
	default:
		return false;
	}
}

int led_interval_from_str(enum led_interval *interval, const char *str)
{
	if (strcasecmp(str, "*") == 0) {
		*interval = LED_INTERVAL_DEFAULT;
		return 0;
	}
	if (strcasecmp(str, "slowest") == 0)
		*interval = LED_INTERVAL_SLOWEST;
	else if (strcasecmp(str, "slower") == 0)
		*interval = LED_INTERVAL_SLOWER;
	else if (strcasecmp(str, "normal") == 0)
		*interval = LED_INTERVAL_NORMAL;
	else if (strcasecmp(str, "faster") == 0)
		*interval = LED_INTERVAL_FASTER;
	else if (strcasecmp(str, "fastest") == 0)
		*interval = LED_INTERVAL_FASTEST;
	else
		return 1;
	return 0;
}

void led_msg_interval(struct led_msg *msg, enum led_interval interval)
{
	msg->msg[4] &= ~0b111;
	msg->msg[4] |= (u8) interval;
}

bool led_msg_interval_is_legal(const struct led_msg *msg,
                               enum led_interval interval)
{
	if (interval == LED_INTERVAL_DEFAULT)
		return true;
	switch (led_msg_preset_get(msg)) {
	case LED_PRESET_FADING:
	case LED_PRESET_SPECTRUM_WAVE:
	case LED_PRESET_MARQUEE:
	case LED_PRESET_COVERING_MARQUEE:
	case LED_PRESET_ALTERNATING:
	case LED_PRESET_BREATHING:
	case LED_PRESET_PULSE:
	case LED_PRESET_TAI_CHI:
	case LED_PRESET_WATER_COOLER:
		return true;
	default:
		return false;
	}
}

int led_group_size_from_str(u8 *group_size, const char *str)
{
	int ret;
	unsigned int n;
	if (strcasecmp(str, "*") == 0) {
		*group_size = LED_GROUP_SIZE_DEFAULT;
		return 0;
	}
	ret = kstrtouint(str, 0, &n);
	if (ret || ret > U8_MAX)
		return ret ? ret : 1;
	*group_size = ret;
	return 0;
}

void led_msg_group_size(struct led_msg *msg, u8 group_size)
{
	group_size = (group_size - LED_GROUP_SIZE_DEFAULT) & 0b11;
	msg->msg[4] &= ~(0b11 << 3);
	msg->msg[4] |= group_size << 3;
}

bool led_msg_group_size_is_legal(const struct led_msg *msg, u8 group_size)
{
	if (group_size == LED_GROUP_SIZE_DEFAULT)
		return true;
	switch (led_msg_preset_get(msg)) {
	case LED_PRESET_MARQUEE:
		return true;
	default:
		return false;
	}
}

void led_msg_all_default(struct led_msg *msg)
{
	led_msg_moving(msg, LED_MOVING_DEFAULT);
	led_msg_direction(msg, LED_DIRECTION_DEFAULT);
	led_msg_interval(msg, LED_INTERVAL_DEFAULT);
	led_msg_group_size(msg, LED_GROUP_SIZE_DEFAULT);
}

static void led_msg_cycle(struct led_msg *msg, u8 cycle)
{
	cycle &= 0b111;
	msg->msg[4] &= ~(0b111 << 5);
	msg->msg[4] |= cycle << 5;
}

int led_color_from_str(struct led_color *color, const char *str)
{
	char hex[7];
	unsigned long rgb;
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

	ret = kstrtoul(hex, 16, &rgb);
	if (ret)
		return ret;
	color->red   = (rgb >> 16) & 0xff;
	color->green = (rgb >>  8) & 0xff;
	color->blue  = (rgb >>  0) & 0xff;
	return 0;
}

void led_msg_color_logo(struct led_msg *msg, const struct led_color *color)
{
	// NOTE: the logo color is in GRB format
	msg->msg[5] = color->green;
	msg->msg[6] = color->red;
	msg->msg[7] = color->blue;
}

void led_msg_colors_ring(struct led_msg *msg, const struct led_color *colors)
{
	size_t i;
	for (i = 0; i < LED_MSG_COLORS_RING; i++) {
		u8 *start = msg->msg + 8 + i * 3;
		start[0] = colors[i].red;
		start[1] = colors[i].green;
		start[2] = colors[i].blue;
	}
}


static void led_batch_init(struct led_batch *batch, enum led_which which)
{
	u8 i;
	for (i = 0; i < ARRAY_SIZE(batch->cycles); i++) {
		struct led_msg *msg = &batch->cycles[i];
		led_msg_init(msg);
		led_msg_which(msg, which);
		led_msg_cycle(msg, i);
	}
}

void led_data_init(struct led_data *data, enum led_which which)
{
	size_t i;
	data->update = LED_DATA_UPDATE_NONE;
	for (i = 0; i < ARRAY_SIZE(data->batches); i++) {
		led_batch_init(&data->batches[i], which);
	}
	mutex_init(&data->mutex);
}

static int led_batch_update(struct led_batch *batch, struct usb_kraken *kraken)
{
	int ret, sent;
	u8 i;
	for (i = 0; i < batch->len; i++) {
		ret = usb_interrupt_msg(
			kraken->udev, usb_sndctrlpipe(kraken->udev, 1),
			batch->cycles[i].msg, sizeof(batch->cycles[i].msg),
			&sent, 1000);
		if (ret || sent != sizeof(batch->cycles[i].msg)) {
			dev_err(&kraken->udev->dev,
			        "failed to set LED cycle %u\n", i);
			return ret ? ret : 1;
		}
	}
	return 0;
}

int kraken_x62_update_led(struct usb_kraken *kraken, struct led_data *data)
{
	struct led_batch *batch;
	s8 value;
	int ret = 0;

	mutex_lock(&data->mutex);
	switch (data->update) {
	case LED_DATA_UPDATE_NONE:
		goto error;
	case LED_DATA_UPDATE_STATIC:
		data->update = LED_DATA_UPDATE_NONE;
		break;
	case LED_DATA_UPDATE_DYNAMIC:
		break;
	}

	value = data->value.get(data->value.state, kraken->data);
	if (value < 0) {
		dev_err(&kraken->udev->dev,
		        "error getting value for dynamic LED update: %d\n",
		        value);
		ret = value;
		goto error;
	}
	// if same value as previously, no update necessary
	if (value == data->value_prev)
		goto error;
	batch = &data->batches[value];
	// if same message as previously, no update necessary
	if (data->batch_prev != NULL &&
	    memcmp(batch, data->batch_prev, sizeof(*batch)) == 0)
		goto error;

	ret = led_batch_update(batch, kraken);
	if (ret)
		goto error;
	data->value_prev = value;
	data->batch_prev = batch;

error:
	mutex_unlock(&data->mutex);
	return ret;
}
