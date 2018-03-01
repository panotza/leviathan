#ifndef LEVIATHAN_X62_LED_H_INCLUDED
#define LEVIATHAN_X62_LED_H_INCLUDED

#include "../common.h"

#include <linux/mutex.h>

#define LED_MSG_SIZE        32
#define LED_MSG_COLORS_RING 8

struct led_msg {
	u8 msg[LED_MSG_SIZE];
};

enum led_which {
	LED_WHICH_LOGO = 0b001,
	LED_WHICH_RING = 0b010,
};

void led_msg_moving(struct led_msg *msg, bool moving);

enum led_direction {
	LED_DIRECTION_CLOCKWISE        = 0b0000,
	LED_DIRECTION_COUNTERCLOCKWISE = 0b0001,
};

int led_direction_from_str(enum led_direction *direction, const char *str);
void led_msg_direction(struct led_msg *msg, enum led_direction direction);

enum led_preset {
	LED_PRESET_FIXED            = 0x00,
	LED_PRESET_FADING           = 0x01,
	LED_PRESET_SPECTRUM_WAVE    = 0x02,
	LED_PRESET_MARQUEE          = 0x03,
	LED_PRESET_COVERING_MARQUEE = 0x04,
	LED_PRESET_ALTERNATING      = 0x05,
	LED_PRESET_BREATHING        = 0x06,
	LED_PRESET_PULSE            = 0x07,
	LED_PRESET_TAI_CHI          = 0x08,
	LED_PRESET_WATER_COOLER     = 0x09,
	LED_PRESET_LOAD             = 0x0a,
};

int led_preset_from_str(enum led_preset *preset, const char *str);
void led_msg_preset(struct led_msg *msg, enum led_preset preset);

enum led_interval {
	LED_INTERVAL_SLOWEST = 0b000,
	LED_INTERVAL_SLOWER  = 0b001,
	LED_INTERVAL_NORMAL  = 0b010,
	LED_INTERVAL_FASTER  = 0b011,
	LED_INTERVAL_FASTEST = 0b100,
};

int led_interval_from_str(enum led_interval *interval, const char *str);
void led_msg_interval(struct led_msg *msg, enum led_interval interval);

void led_msg_group_size(struct led_msg *msg, u8 group_size);

struct led_color {
	u8 red;
	u8 green;
	u8 blue;
};

int led_color_from_str(struct led_color *color, const char *str);

void led_msg_color_logo(struct led_msg *msg, const struct led_color *color);
void led_msg_colors_ring(struct led_msg *msg, const struct led_color *colors);

#define LED_DATA_CYCLES_SIZE 8

struct led_data {
	struct led_msg cycles[LED_DATA_CYCLES_SIZE];
	// first len messages in `cycles` are to be sent when updating (0 means
	// do not update the LEDs)
	u8 len;
	struct mutex mutex;
};

void led_data_init(struct led_data *data, enum led_which which);

int kraken_x62_update_led(struct usb_kraken *kraken, struct led_data *data);

#endif  /* LEVIATHAN_X62_LED_H_INCLUDED */
