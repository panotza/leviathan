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

void led_msg_moving(struct led_msg *msg, bool moving)
{
	msg->msg[2] &= ~(0b1 << 3);
	msg->msg[2] |= ((u8) moving) << 3;
}

int led_direction_from_str(enum led_direction *direction, const char *str)
{
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

int led_interval_from_str(enum led_interval *interval, const char *str)
{
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

void led_msg_group_size(struct led_msg *msg, u8 group_size)
{
	group_size = (group_size - 3) & 0b11;
	msg->msg[4] &= ~(0b11 << 3);
	msg->msg[4] |= group_size << 3;
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

void led_data_init(struct led_data *data, enum led_which which)
{
	u8 i;
	for (i = 0; i < LED_DATA_CYCLES_SIZE; i++) {
		led_msg_init(&data->cycles[i]);
		led_msg_which(&data->cycles[i], which);
		led_msg_cycle(&data->cycles[i], i);
	}
	data->len = 0;
	mutex_init(&data->mutex);
}

int kraken_x62_update_led(struct usb_kraken *kraken, struct led_data *data)
{
	u8 i;
	int ret, sent;

	mutex_lock(&data->mutex);
	for (i = 0; i < data->len; i++) {
		ret = usb_interrupt_msg(
			kraken->udev, usb_sndctrlpipe(kraken->udev, 1),
			data->cycles[i].msg, LED_MSG_SIZE, &sent, 1000);
		if (ret || sent != LED_MSG_SIZE) {
			data->len = 0;
			mutex_unlock(&data->mutex);
			dev_err(&kraken->udev->dev,
			        "failed to set LED cycle %u\n", i);
			return ret ? ret : 1;
		}
	}
	data->len = 0;
	mutex_unlock(&data->mutex);
	return 0;
}
