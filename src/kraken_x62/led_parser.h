#ifndef LEVIATHAN_X62_LED_PARSER_H_INCLUDED
#define LEVIATHAN_X62_LED_PARSER_H_INCLUDED

#include "led.h"

#include <linux/device.h>

struct led_parser {
	struct led_data *data;
	const char *buf;
	struct device *dev;
	const char *attr;
};

int led_parser_parse(struct led_parser *parser);

#endif  /* LEVIATHAN_X62_LED_PARSER_H_INCLUDED */
