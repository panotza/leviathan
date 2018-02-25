#ifndef LEVIATHAN_X62_LEDS_H_INCLUDED
#define LEVIATHAN_X62_LEDS_H_INCLUDED

#include "../common.h"

#include <linux/mutex.h>

#define LEDS_MSG_SIZE 32

enum leds_which {
	LEDS_WHICH_LOGO = 0b001,
	LEDS_WHICH_RING = 0b010,
};

enum leds_direction {
	LEDS_DIRECTION_CLOCKWISE        = 0b0000,
	LEDS_DIRECTION_COUNTERCLOCKWISE = 0b0001,
};

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

enum leds_interval {
	LEDS_INTERVAL_SLOWEST = 0b000,
	LEDS_INTERVAL_SLOWER  = 0b001,
	LEDS_INTERVAL_NORMAL  = 0b010,
	LEDS_INTERVAL_FASTER  = 0b011,
	LEDS_INTERVAL_FASTEST = 0b100,
};

struct led_color {
	u8 red;
	u8 green;
	u8 blue;
};

int led_color_from_str(struct led_color *color, const char *str);

#define LEDS_MSG_RING_COLORS 8

void leds_msg_color_logo(u8 *msg, const struct led_color *color);
void leds_msg_colors_ring(u8 *msg, const struct led_color *colors);

#define LED_CYCLES_MAX 8

struct led_cycles {
	u8 msgs[LED_CYCLES_MAX][LEDS_MSG_SIZE];
	// first len messages in msgs are to be sent when updating (0 means do
	// not update the LEDs)
	u8 len;
	struct mutex mutex;
};

void led_cycles_init(struct led_cycles *cycles, enum leds_which which);

int kraken_x62_update_led_cycles(struct usb_kraken *kraken,
                                 struct led_cycles *cycles);

struct leds_store {
	struct device *dev;
	struct device_attribute *attr;

	u8 cycles;
	enum leds_preset preset;
	bool moving;
	enum leds_direction direction;
	enum leds_interval interval;
	u8 group_size;

	void *rest;
};

void leds_store_init(struct leds_store *store, struct device *dev,
                     struct device_attribute *attr, void *rest);
void leds_store_to_msg(struct leds_store *store, u8 *leds_msg);

enum leds_store_err {
	LEDS_STORE_OK,
	LEDS_STORE_ERR_PRESET,
	LEDS_STORE_ERR_NO_VALUE,
	LEDS_STORE_ERR_INVALID,
};

typedef enum leds_store_err leds_store_key_fun(struct leds_store *store,
                                               const char **buf);

int leds_store_keys(struct leds_store *store, const char **buf,
                    const char **keys, leds_store_key_fun **key_funs);
int leds_store_preset(struct leds_store *store, const char **buf);

#endif  /* LEVIATHAN_X62_LEDS_H_INCLUDED */
