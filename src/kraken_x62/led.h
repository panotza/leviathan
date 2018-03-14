#ifndef LEVIATHAN_X62_LED_H_INCLUDED
#define LEVIATHAN_X62_LED_H_INCLUDED

#include "dynamic.h"
#include "../common.h"

#include <linux/mutex.h>

#define LED_MSG_SIZE        ((size_t) 32)
#define LED_MSG_COLORS_RING  ((size_t) 8)

struct led_msg {
	u8 msg[LED_MSG_SIZE];
};

enum led_which {
	LED_WHICH_SYNC = 0b000,
	LED_WHICH_LOGO = 0b001,
	LED_WHICH_RING = 0b010,
};

enum led_which led_msg_which_get(const struct led_msg *msg);

#define LED_MOVING_DEFAULT false

int led_moving_from_str(bool *moving, const char *str);
void led_msg_moving(struct led_msg *msg, bool moving);
bool led_msg_moving_is_legal(const struct led_msg *msg, bool moving);

enum led_direction {
	LED_DIRECTION_CLOCKWISE        = 0b0000,
	LED_DIRECTION_COUNTERCLOCKWISE = 0b0001,
};

#define LED_DIRECTION_DEFAULT LED_DIRECTION_CLOCKWISE

int led_direction_from_str(enum led_direction *direction, const char *str);
void led_msg_direction(struct led_msg *msg, enum led_direction direction);
bool led_msg_direction_is_legal(const struct led_msg *msg,
                                enum led_direction direction);

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
enum led_preset led_msg_preset_get(const struct led_msg *msg);
bool led_msg_preset_is_legal(const struct led_msg *msg, enum led_preset preset);

enum led_interval {
	LED_INTERVAL_SLOWEST = 0b000,
	LED_INTERVAL_SLOWER  = 0b001,
	LED_INTERVAL_NORMAL  = 0b010,
	LED_INTERVAL_FASTER  = 0b011,
	LED_INTERVAL_FASTEST = 0b100,
};

#define LED_INTERVAL_DEFAULT LED_INTERVAL_NORMAL

int led_interval_from_str(enum led_interval *interval, const char *str);
void led_msg_interval(struct led_msg *msg, enum led_interval interval);
bool led_msg_interval_is_legal(const struct led_msg *msg,
                               enum led_interval interval);

#define LED_GROUP_SIZE_MIN     ((u8) 3)
#define LED_GROUP_SIZE_MAX     ((u8) 6)
#define LED_GROUP_SIZE_DEFAULT ((u8) 3)

int led_group_size_from_str(u8 *group_size, const char *str);
void led_msg_group_size(struct led_msg *msg, u8 group_size);
bool led_msg_group_size_is_legal(const struct led_msg *msg, u8 group_size);

void led_msg_all_default(struct led_msg *msg);

struct led_color {
	u8 red;
	u8 green;
	u8 blue;
};

int led_color_from_str(struct led_color *color, const char *str);

void led_msg_color_logo(struct led_msg *msg, const struct led_color *color);
void led_msg_colors_ring(struct led_msg *msg, const struct led_color *colors);


#define LED_BATCH_CYCLES_SIZE ((size_t) 8)

/**
 * A batch of 1 or more update messages -- one message per cycle.
 */
struct led_batch {
	struct led_msg cycles[LED_BATCH_CYCLES_SIZE];
	// first len messages in `cycles` are to be sent when updating
	u8 len;
};

enum led_data_update {
	LED_DATA_UPDATE_NONE,
	LED_DATA_UPDATE_STATIC,
	LED_DATA_UPDATE_DYNAMIC,
};

struct led_data {
	enum led_data_update update;
	// called by update function
	struct dynamic_val value;
	// batches[val] is the batch to send for value val
	struct led_batch batches[DYNAMIC_VAL_MAX + 1];

	// no new message is sent if the previous value or batch is equal to the
	// current one, as an update would have no effect then
	s8 value_prev;
	struct led_batch *batch_prev;
	struct mutex mutex;
};

void led_data_init(struct led_data *data, enum led_which which);

int kraken_x62_update_led(struct usb_kraken *kraken, struct led_data *data);

#endif  /* LEVIATHAN_X62_LED_H_INCLUDED */
