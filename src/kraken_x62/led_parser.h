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

struct led_parser;

typedef bool led_parser_preset_legal_fn(struct led_parser *parser);
typedef enum led_parser_ret led_parser_parse_key_fn(
	struct led_parser *parser, const char *key, const char **buf);
typedef void led_parser_to_data_fn(struct led_parser *parser,
                                   struct led_data *data);

struct led_parser {
	struct device *dev;
	struct device_attribute *attr;

	enum led_preset preset;
	led_parser_preset_legal_fn *preset_legal;
	bool moving;
	enum led_direction direction;
	enum led_interval interval;
	u8 group_size;

	u8 cycles;
	void *cycles_data;
	led_parser_parse_key_fn *cycles_data_parse_key;
	led_parser_to_data_fn *cycles_data_to_data;
};

void led_parser_init(struct led_parser *parser);
void led_parser_to_data(struct led_parser *parser, struct led_data *data);
int led_parser_parse(struct led_parser *parser, const char *buf);

#endif  /* LEVIATHAN_X62_LED_PARSER_H_INCLUDED */
