/* Parsing of LED attributes.
 */

#include "led_parser.h"
#include "driver_data.h"
#include "led.h"
#include "status.h"
#include "../util.h"

static int led_parser_preset_check_len(struct led_parser *parser,
                                       enum led_preset preset, u8 batch_len)
{
	bool ok = false;
	switch (preset) {
	case LED_PRESET_FIXED:
	case LED_PRESET_SPECTRUM_WAVE:
	case LED_PRESET_MARQUEE:
	case LED_PRESET_WATER_COOLER:
	case LED_PRESET_LOAD:
		ok = batch_len == 1;
		break;
	case LED_PRESET_ALTERNATING:
	case LED_PRESET_TAI_CHI:
		ok = batch_len == 2;
		break;
	case LED_PRESET_FADING:
	case LED_PRESET_COVERING_MARQUEE:
	case LED_PRESET_BREATHING:
	case LED_PRESET_PULSE:
		ok = batch_len >= 1 && batch_len <= LED_BATCH_CYCLES_SIZE;
		break;
	}
	if (!ok)
		dev_err(parser->dev,
		        "%s: invalid nr of cycles %u for given preset\n",
		        parser->attr, batch_len);
	return !ok;
}

static int led_parser_preset(struct led_parser *parser, struct led_batch *batch)
{
	char preset_str[WORD_LEN_MAX + 1];
	enum led_preset preset;
	u8 i;
	int ret = str_scan_word(&parser->buf, preset_str);
	if (ret) {
		dev_err(parser->dev, "%s: missing preset\n", parser->attr);
		return ret;
	}
	ret = led_preset_from_str(&preset, preset_str);
	if (ret) {
		dev_err(parser->dev, "%s: invalid preset %s\n", parser->attr,
		        preset_str);
		return ret;
	}
	if (!led_msg_preset_is_legal(&batch->cycles[0], preset)) {
		dev_err(parser->dev, "%s: illegal preset %s for LED(s)\n",
		        parser->attr, preset_str);
		return 1;
	}
	ret = led_parser_preset_check_len(parser, preset, batch->len);
	if (ret)
		return ret;
	for (i = 0; i < batch->len; i++)
		led_msg_preset(&batch->cycles[i], preset);
	return 0;
}

static int led_parser_moving(struct led_parser *parser,
                             struct led_batch *batch)
{
	char moving_str[WORD_LEN_MAX + 1];
	bool moving;
	u8 i;
	int ret = str_scan_word(&parser->buf, moving_str);
	if (ret) {
		dev_err(parser->dev, "%s: missing moving\n", parser->attr);
		return ret;
	}
	ret = led_moving_from_str(&moving, moving_str);
	if (ret) {
		dev_err(parser->dev, "%s: invalid moving %s\n", parser->attr,
		        moving_str);
		return ret;
	}
	if (!led_msg_moving_is_legal(&batch->cycles[0], moving)) {
		dev_err(parser->dev,
		        "%s: illegal moving %d for the given preset\n",
		        parser->attr, moving);
		return 1;
	}
	for (i = 0; i < batch->len; i++)
		led_msg_moving(&batch->cycles[i], moving);
	return 0;
}

static int led_parser_direction(struct led_parser *parser,
                                struct led_batch *batch)
{
	char direction_str[WORD_LEN_MAX + 1];
	enum led_direction direction;
	u8 i;
	int ret = str_scan_word(&parser->buf, direction_str);
	if (ret) {
		dev_err(parser->dev, "%s: missing direction\n", parser->attr);
		return ret;
	}
	ret = led_direction_from_str(&direction, direction_str);
	if (ret) {
		dev_err(parser->dev, "%s: invalid direction %s\n", parser->attr,
		        direction_str);
		return ret;
	}
	if (!led_msg_direction_is_legal(&batch->cycles[0], direction)) {
		dev_err(parser->dev,
		        "%s: illegal direction %s for the given preset\n",
		        parser->attr, direction_str);
		return 1;
	}
	for (i = 0; i < batch->len; i++)
		led_msg_direction(&batch->cycles[i], direction);
	return 0;
}

static int led_parser_interval(struct led_parser *parser,
                               struct led_batch *batch)
{
	char interval_str[WORD_LEN_MAX + 1];
	enum led_interval interval;
	u8 i;
	int ret = str_scan_word(&parser->buf, interval_str);
	if (ret) {
		dev_err(parser->dev, "%s: missing interval\n", parser->attr);
		return ret;
	}
	ret = led_interval_from_str(&interval, interval_str);
	if (ret) {
		dev_err(parser->dev, "%s: invalid interval %s\n", parser->attr,
		        interval_str);
		return ret;
	}
	if (!led_msg_interval_is_legal(&batch->cycles[0], interval)) {
		dev_err(parser->dev,
		        "%s: illegal interval %s for the given preset\n",
		        parser->attr, interval_str);
		return 1;
	}
	for (i = 0; i < batch->len; i++)
		led_msg_interval(&batch->cycles[i], interval);
	return 0;
}

static int led_parser_group_size(struct led_parser *parser,
                                 struct led_batch *batch)
{
	char group_size_str[WORD_LEN_MAX + 1];
	u8 group_size;
	u8 i;
	int ret = str_scan_word(&parser->buf, group_size_str);
	if (ret) {
		dev_err(parser->dev, "%s: missing group size\n", parser->attr);
		return ret;
	}
	ret = led_group_size_from_str(&group_size, group_size_str);
	if (ret) {
		dev_err(parser->dev, "%s: invalid group size %s\n",
		        parser->attr, group_size_str);
		return ret;
	}
	if (!led_msg_group_size_is_legal(&batch->cycles[0], group_size)) {
		dev_err(parser->dev,
		        "%s: illegal group size %u for the given preset\n",
		        parser->attr, group_size);
		return 1;
	}
	for (i = 0; i < batch->len; i++)
		led_msg_group_size(&batch->cycles[i], group_size);
	return 0;
}

static int led_parser_color_logo(struct led_parser *parser, struct led_msg *msg)
{
	char color_str[WORD_LEN_MAX + 1];
	struct led_color color;
	int ret = str_scan_word(&parser->buf, color_str);
	if (ret) {
		dev_err(parser->dev, "%s: missing color\n", parser->attr);
		return ret;
	}
	ret = led_color_from_str(&color, color_str);
	if (ret) {
		dev_err(parser->dev, "%s: invalid color %s\n", parser->attr,
		        color_str);
		return ret;
	}
	led_msg_color_logo(msg, &color);
	return 0;
}

static int led_parser_colors_ring(struct led_parser *parser,
                                  struct led_msg *msg)
{
	char color_str[WORD_LEN_MAX + 1];
	struct led_color colors[LED_MSG_COLORS_RING];
	int ret;
	size_t i;
	for (i = 0; i < ARRAY_SIZE(colors); i++) {
		ret = str_scan_word(&parser->buf, color_str);
		if (ret) {
			dev_err(parser->dev, (i == 0) ? "%s: missing colors\n" :
			        "%s: invalid colors\n", parser->attr);
			return ret;
		}
		ret = led_color_from_str(&colors[i], color_str);
		if (ret) {
			dev_err(parser->dev, "%s: invalid colors ... %s\n",
			        parser->attr, color_str);
			return ret;
		}
	}
	led_msg_colors_ring(msg, colors);
	return 0;
}

static int led_parser_colors(struct led_parser *parser, struct led_msg *msg)
{
	int ret = 0;
	switch (led_msg_which_get(msg)) {
	case LED_WHICH_LOGO:
		ret = led_parser_color_logo(parser, msg);
		break;
	case LED_WHICH_RING:
		ret = led_parser_colors_ring(parser, msg);
		break;
	}
	return ret;
}

static int led_parser_batch_off(struct led_parser *parser,
                                 struct led_batch *batch)
{
	struct led_color colors[LED_MSG_COLORS_RING];
	struct led_msg *msg = &batch->cycles[0];
	batch->len = 1;
	led_msg_preset(msg, LED_PRESET_FIXED);
	led_msg_moving(msg, LED_MOVING_DEFAULT);
	led_msg_direction(msg, LED_DIRECTION_DEFAULT);
	led_msg_interval(msg, LED_INTERVAL_DEFAULT);
	led_msg_group_size(msg, LED_GROUP_SIZE_DEFAULT);

	memset(colors, 0x00, sizeof(colors));
	led_msg_color_logo(msg, &colors[0]);
	led_msg_colors_ring(msg, colors);
	return 0;
}

static int led_parser_batch(struct led_parser *parser, struct led_batch *batch)
{
	char len_str[WORD_LEN_MAX + 1];
	unsigned int len;
	size_t i;
	int ret = str_scan_word(&parser->buf, len_str);
	if (ret) {
		dev_err(parser->dev, "%s: missing nr of cycles\n",
		        parser->attr);
		return ret;
	}
	if (strcasecmp(len_str, "off") == 0) {
		ret = led_parser_batch_off(parser, batch);
		return ret;
	}
	ret = kstrtouint(len_str, 0, &len);
	if (ret || len < 1 || len > LED_BATCH_CYCLES_SIZE) {
		dev_err(parser->dev, "%s: invalid nr of cycles %s\n",
		        parser->attr, len_str);
		return ret ? ret : 1;
	}
	batch->len = len;

	if ((ret = led_parser_preset(parser, batch)) ||
	    (ret = led_parser_moving(parser, batch)) ||
	    (ret = led_parser_direction(parser, batch)) ||
	    (ret = led_parser_interval(parser, batch)) ||
	    (ret = led_parser_group_size(parser, batch)))
		return ret;

	for (i = 0; i < batch->len; i++) {
		ret = led_parser_colors(parser, &batch->cycles[i]);
		if (ret)
			return ret;
	}
	return 0;
}

static s8 led_data_val_const_0(void *state,
                               struct kraken_driver_data *driver_data)
{
	return 0;
}

static int led_parser_static(struct led_parser *parser)
{
	int ret;
	// static is implemented with a constant-0 value and the single batch
	// being stored at index 0
	parser->data->value.get = led_data_val_const_0;
	ret = led_parser_batch(parser, &parser->data->batches[0]);
	if (ret)
		return ret;
	parser->data->update = LED_DATA_UPDATE_STATIC;
	return 0;
}

static s8 led_data_val_temp_liquid(void *state,
                                   struct kraken_driver_data *driver_data)
{
	return status_data_temp_liquid(&driver_data->status);
}

static s8 led_data_val_normalized(s64 value, void *state)
{
	s64 *max = state;
	s64 normalized = value * 100 / *max;
	if (normalized > LED_DATA_VAL_MAX)
		normalized = LED_DATA_VAL_MAX;
	return normalized;
}

static s8 led_data_val_fan_rpm(void *state,
                               struct kraken_driver_data *driver_data)
{
	const u16 rpm = status_data_fan_rpm(&driver_data->status);
	return led_data_val_normalized(rpm, state);
}

static s8 led_data_val_pump_rpm(void *state,
                                struct kraken_driver_data *driver_data)
{
	const u16 rpm = status_data_pump_rpm(&driver_data->status);
	return led_data_val_normalized(rpm, state);
}

static int led_parser_source_normalized(struct led_parser *parser)
{
	s64 *max = (s64 *) parser->data->value.state;
	unsigned long long max_ull;
	char max_str[WORD_LEN_MAX + 1];
	int ret = str_scan_word(&parser->buf, max_str);
	if (ret) {
		dev_err(parser->dev, "%s: missing source max value\n",
		        parser->attr);
		return ret;
	}
	ret = kstrtoull(max_str, 0, &max_ull);
	if (ret) {
		dev_err(parser->dev, "%s: invalid source max value %s\n",
		        parser->attr, max_str);
		return ret;
	}
	*max = max_ull;
	return 0;
}

static int led_parser_dynamic(struct led_parser *parser)
{
	char source[WORD_LEN_MAX + 1];
	size_t i;
	int ret = str_scan_word(&parser->buf, source);
	if (ret) {
		dev_err(parser->dev, "%s: missing source\n", parser->attr);
		return ret;
	}
	ret = 0;
	if (strcasecmp(source, "temp_liquid") == 0) {
		parser->data->value.get = led_data_val_temp_liquid;
	} else if (strcasecmp(source, "fan_rpm") == 0) {
		parser->data->value.get = led_data_val_fan_rpm;
		ret = led_parser_source_normalized(parser);
	} else if (strcasecmp(source, "pump_rpm") == 0) {
		parser->data->value.get = led_data_val_pump_rpm;
		ret = led_parser_source_normalized(parser);
	} else {
		dev_err(parser->dev, "%s: illegal source %s\n", parser->attr,
		        source);
		return 1;
	}
	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(parser->data->batches); i++) {
		ret = led_parser_batch(parser, &parser->data->batches[i]);
		if (ret)
			return ret;
	}
	parser->data->update = LED_DATA_UPDATE_DYNAMIC;
	return 0;
}


int led_parser_parse(struct led_parser *parser)
{
	char update[WORD_LEN_MAX + 1];
	int ret = str_scan_word(&parser->buf, update);
	if (ret) {
		dev_err(parser->dev, "%s: missing update type", parser->attr);
		return 1;
	}
	if (strcasecmp(update, "static") == 0) {
		ret = led_parser_static(parser);
	} else if (strcasecmp(update, "dynamic") == 0) {
		ret = led_parser_dynamic(parser);
	} else {
		dev_err(parser->dev, "%s: illegal update type %s", parser->attr,
		        update);
		return 1;
	}
	if (ret)
		return ret;
	ret = str_scan_word(&parser->buf, update);
	if (!ret) {
		dev_err(parser->dev,
		        "%s: unrecognized data left in buffer: %s...\n",
		        parser->attr, update);
		parser->data->update = LED_DATA_UPDATE_NONE;
		return 1;
	}

	parser->data->value_prev = -1;
	parser->data->batch_prev = NULL;
	return 0;
}
