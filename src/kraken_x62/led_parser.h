#ifndef LEVIATHAN_X62_LED_PARSER_H_INCLUDED
#define LEVIATHAN_X62_LED_PARSER_H_INCLUDED

#include "led.h"

#include <linux/device.h>

enum led_parser_ret {
	LED_PARSER_RET_OK,
	LED_PARSER_RET_KEY,
	LED_PARSER_RET_PRESET,
	LED_PARSER_RET_VALUE_MISSING,
	LED_PARSER_RET_VALUE_INVALID,
};

struct led_parser_reg;

typedef bool led_parser_reg_preset_legal_fn(struct led_parser_reg *parser);
typedef enum led_parser_ret led_parser_reg_parse_key_fn(
	struct led_parser_reg *parser, const char *key, const char **buf);
typedef void led_parser_reg_to_data_fn(struct led_parser_reg *parser,
                                       struct led_data_reg *data);

struct led_parser_reg {
	struct device *dev;
	struct device_attribute *attr;

	enum led_preset preset;
	led_parser_reg_preset_legal_fn *preset_legal;
	bool moving;
	enum led_direction direction;
	enum led_interval interval;
	u8 group_size;

	u8 cycles;
	void *cycles_data;
	led_parser_reg_parse_key_fn *cycles_data_parse_key;
	led_parser_reg_to_data_fn *cycles_data_to_data;
};

void led_parser_reg_init(struct led_parser_reg *parser);
void led_parser_reg_to_data(struct led_parser_reg *parser,
                            struct led_data_reg *data);
int led_parser_reg_parse(struct led_parser_reg *parser, const char *buf);


struct led_parser_dyn;

typedef enum led_parser_ret led_parser_dyn_parse_fn(
	struct led_parser_dyn *parser, const char **buf);
typedef void led_parser_dyn_to_msg_fn(struct led_parser_dyn *parser,
                                      size_t range, struct led_msg *msg);

struct led_parser_dyn {
	struct device *dev;
	struct device_attribute *attr;

	struct led_data_dyn_val value;
	u8 range_mins[LED_DATA_DYN_MSGS_SIZE];
	u8 range_maxes[LED_DATA_DYN_MSGS_SIZE];

	size_t ranges;
	void *ranges_data;
	led_parser_dyn_parse_fn *ranges_data_parse;
	led_parser_dyn_to_msg_fn *ranges_data_to_msg;
};

void led_parser_dyn_init(struct led_parser_dyn *parser);
void led_parser_dyn_to_data(struct led_parser_dyn *parser,
                            struct led_data_dyn *data);
int led_parser_dyn_parse(struct led_parser_dyn *parser, const char *buf);

#endif  /* LEVIATHAN_X62_LED_PARSER_H_INCLUDED */
