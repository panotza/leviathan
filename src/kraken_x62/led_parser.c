/* Parsing of LED attributes.
 */

#include "led_parser.h"
#include "driver_data.h"
#include "led.h"
#include "percent.h"
#include "status.h"
#include "../util.h"

void led_parser_reg_init(struct led_parser_reg *parser)
{
	parser->moving = false;
	parser->direction = LED_DIRECTION_CLOCKWISE;
	parser->interval = LED_INTERVAL_NORMAL;
	parser->group_size = 3;

	parser->cycles = 0;
}

void led_parser_reg_to_data(struct led_parser_reg *parser,
                            struct led_data_reg *data)
{
	u8 i;
	for (i = 0; i < parser->cycles; i++) {
		struct led_msg *msg = &data->cycles[i];
		led_msg_preset(msg, parser->preset);
		led_msg_moving(msg, parser->moving);
		led_msg_direction(msg, parser->direction);
		led_msg_interval(msg, parser->interval);
		led_msg_group_size(msg, parser->group_size);
	}
	data->len = parser->cycles;
	parser->cycles_data_to_data(parser, data);
}

static int led_parser_reg_preset(struct led_parser_reg *parser,
                                 const char **buf)
{
	char preset_str[WORD_LEN_MAX + 1];
	int ret = str_scan_word(buf, preset_str);
	if (ret) {
		dev_err(parser->dev, "%s: preset missing\n",
		        parser->attr->attr.name);
		return ret;
	}
	ret = led_preset_from_str(&parser->preset, preset_str);
	if (ret) {
		dev_err(parser->dev, "%s: invalid preset: %s\n",
		        parser->attr->attr.name, preset_str);
		return ret;
	}
	return 0;
}

static enum led_parser_ret led_parser_key_moving(struct led_parser_reg *parser,
                                                 const char **buf)
{
	char word[WORD_LEN_MAX + 1];
	int ret;

	switch (parser->preset) {
	case LED_PRESET_ALTERNATING:
		break;
	default:
		return LED_PARSER_RET_PRESET;
	}

	ret = str_scan_word(buf, word);
	if (ret)
		return LED_PARSER_RET_VALUE_MISSING;
	ret = kstrtobool(word, &parser->moving);
	return ret ? LED_PARSER_RET_VALUE_INVALID : LED_PARSER_RET_OK;
}

static enum led_parser_ret led_parser_key_direction(
	struct led_parser_reg *parser, const char **buf)
{
	char word[WORD_LEN_MAX + 1];
	int ret;

	switch (parser->preset) {
	case LED_PRESET_SPECTRUM_WAVE:
	case LED_PRESET_MARQUEE:
	case LED_PRESET_COVERING_MARQUEE:
		break;
	default:
		return LED_PARSER_RET_PRESET;
	}

	ret = str_scan_word(buf, word);
	if (ret)
		return LED_PARSER_RET_VALUE_MISSING;
	ret = led_direction_from_str(&parser->direction, word);
	return ret ? LED_PARSER_RET_VALUE_INVALID : LED_PARSER_RET_OK;
}

static enum led_parser_ret led_parser_key_interval(
	struct led_parser_reg *parser, const char **buf)
{
	char word[WORD_LEN_MAX + 1];
	int ret;

	switch (parser->preset) {
	case LED_PRESET_FADING:
	case LED_PRESET_SPECTRUM_WAVE:
	case LED_PRESET_MARQUEE:
	case LED_PRESET_COVERING_MARQUEE:
	case LED_PRESET_ALTERNATING:
	case LED_PRESET_BREATHING:
	case LED_PRESET_PULSE:
	case LED_PRESET_TAI_CHI:
	case LED_PRESET_WATER_COOLER:
		break;
	default:
		return LED_PARSER_RET_PRESET;
	}

	ret = str_scan_word(buf, word);
	if (ret)
		return LED_PARSER_RET_VALUE_MISSING;
	ret = led_interval_from_str(&parser->interval, word);
	return ret ? LED_PARSER_RET_VALUE_INVALID : LED_PARSER_RET_OK;
}

static enum led_parser_ret led_parser_key_group_size(
	struct led_parser_reg *parser, const char **buf)
{
	int scanned;
	int ret;

	switch (parser->preset) {
	case LED_PRESET_MARQUEE:
		break;
	default:
		return LED_PARSER_RET_PRESET;
	}

	ret = sscanf(*buf, "%hhu%n", &parser->group_size, &scanned);
	*buf += scanned;
	if (ret != 1)
		return LED_PARSER_RET_VALUE_INVALID;
	return LED_PARSER_RET_OK;
}

static enum led_parser_ret led_parser_common_parse_key(
	struct led_parser_reg *parser, const char *key, const char **buf)
{
	if (strcmp("moving", key) == 0) {
		return led_parser_key_moving(parser, buf);
	} else if (strcmp("direction", key) == 0) {
		return led_parser_key_direction(parser, buf);
	} else if (strcmp("interval", key) == 0) {
		return led_parser_key_interval(parser, buf);
	} else if (strcmp("group_size", key) == 0) {
		return led_parser_key_group_size(parser, buf);
	} else {
		return LED_PARSER_RET_KEY;
	}
}

static int led_parser_reg_keys(struct led_parser_reg *parser, const char **buf)
{
	char key[WORD_LEN_MAX + 1];
	enum led_parser_ret ret;

	while (!str_scan_word(buf, key)) {
		ret = led_parser_common_parse_key(parser, key, buf);
		if (ret == LED_PARSER_RET_KEY) {
			if (parser->cycles == LED_DATA_CYCLES_SIZE) {
				dev_err(parser->dev,
				        "%s: too many cycles: max %zu\n",
				        parser->attr->attr.name,
				        LED_DATA_CYCLES_SIZE);
				return 1;
			}
			ret = parser->cycles_data_parse_key(parser, key, buf);
			parser->cycles++;
		}
		switch (ret) {
		case LED_PARSER_RET_OK:
			continue;
		case LED_PARSER_RET_KEY:
			dev_err(parser->dev, "%s: unknown key: %s\n",
			        parser->attr->attr.name, key);
			break;
		case LED_PARSER_RET_PRESET:
			dev_err(parser->dev,
			        "%s: illegal key for given preset: %s\n",
			        parser->attr->attr.name, key);
			break;
		case LED_PARSER_RET_VALUE_MISSING:
			dev_err(parser->dev, "%s: missing value for key %s\n",
			        parser->attr->attr.name, key);
			break;
		case LED_PARSER_RET_VALUE_INVALID:
			dev_err(parser->dev, "%s: invalid value for key %s\n",
			        parser->attr->attr.name, key);
			break;
		}
		return 1;
	}
	return 0;
}

static int led_parser_reg_check_cycles(struct led_parser_reg *parser)
{
	bool ok = false;
	switch (parser->preset) {
	case LED_PRESET_FIXED:
	case LED_PRESET_SPECTRUM_WAVE:
	case LED_PRESET_MARQUEE:
	case LED_PRESET_WATER_COOLER:
	case LED_PRESET_LOAD:
		ok = parser->cycles == 1;
		break;
	case LED_PRESET_ALTERNATING:
	case LED_PRESET_TAI_CHI:
		ok = parser->cycles == 2;
		break;
	case LED_PRESET_FADING:
	case LED_PRESET_COVERING_MARQUEE:
	case LED_PRESET_BREATHING:
	case LED_PRESET_PULSE:
		ok = parser->cycles >= 1 &&
			parser->cycles <= LED_DATA_CYCLES_SIZE;
		break;
	}
	if (!ok)
		dev_err(parser->dev,
		        "%s: invalid number of cycles for given preset: %d\n",
		        parser->attr->attr.name, parser->cycles);
	return !ok;
}

int led_parser_reg_parse(struct led_parser_reg *parser, const char *buf)
{
	int ret = led_parser_reg_preset(parser, &buf);
	if (ret)
		return ret;
	if (!parser->preset_legal(parser)) {
		dev_err(parser->dev, "%s: illegal preset\n",
		        parser->attr->attr.name);
		return 1;
	}
	ret = led_parser_reg_keys(parser, &buf);
	if (ret)
		return ret;
	ret = led_parser_reg_check_cycles(parser);
	return ret;
}


void led_parser_dyn_init(struct led_parser_dyn *parser)
{
	parser->ranges = 0;
}

void led_parser_dyn_to_data(struct led_parser_dyn *parser,
                            struct led_data_dyn *data)
{
	size_t i;
	u8 j;
	memcpy(&data->value, &parser->value, sizeof(data->value));
	// first fill value_msgs with the default message
	for (i = 0; i <= LED_DATA_DYN_VAL_MAX; i++) {
		data->value_msgs[i] = &data->msg_default;
	}
	// for each range
	for (i = 0; i < parser->ranges; i++) {
		// set the message for all values in the range
		struct led_msg *msg = &data->msgs[i];
		parser->ranges_data_to_msg(parser, i, msg);
		for (j = parser->range_mins[i]; j <= parser->range_maxes[i];
		     j++)
			data->value_msgs[j] = msg;
	}
	// force next update
	data->value_prev = LED_DATA_DYN_VAL_NONE;
	data->msg_prev = NULL;
}

static u8 led_data_dyn_temp_liquid(void *state,
                                   struct kraken_driver_data *driver_data)
{
	return status_data_temp_liquid(&driver_data->status);
}

static u8 led_data_dyn_value_normalized(u64 value, void *state)
{
	u64 *max = state;
	u64 normalized = value * 100 / *max;
	if (normalized > LED_DATA_DYN_VAL_MAX)
		normalized = LED_DATA_DYN_VAL_MAX;
	return normalized;
}

static u8 led_data_dyn_fan_rpm(void *state,
                               struct kraken_driver_data *driver_data)
{
	const u16 rpm = status_data_fan_rpm(&driver_data->status);
	return led_data_dyn_value_normalized(rpm, state);
}

static u8 led_data_dyn_pump_rpm(void *state,
                                struct kraken_driver_data *driver_data)
{
	const u16 rpm = status_data_pump_rpm(&driver_data->status);
	return led_data_dyn_value_normalized(rpm, state);
}

static int led_parser_dyn_source_normalized(const char **buf, void *state)
{
	u64 *max = state;
	unsigned long long max_ull;
	char max_str[WORD_LEN_MAX + 1];
	int ret = str_scan_word(buf, max_str);
	if (ret)
		return ret;
	ret = kstrtoull(max_str, 0, &max_ull);
	if (ret)
		return ret;
	*max = max_ull;
	return 0;
}

static int led_parser_dyn_source(struct led_parser_dyn *parser,
                                 const char **buf)
{
	char source[WORD_LEN_MAX + 1];
	int ret = str_scan_word(buf, source);
	if (ret) {
		dev_err(parser->dev, "%s: missing source\n",
		        parser->attr->attr.name);
		return ret;
	}
	ret = 0;
	if (strcasecmp(source, "temp_liquid") == 0) {
		parser->value.get = led_data_dyn_temp_liquid;
	} else if (strcasecmp(source, "fan_rpm") == 0) {
		parser->value.get = led_data_dyn_fan_rpm;
		ret = led_parser_dyn_source_normalized(buf,
		                                       parser->value.state);
	} else if (strcasecmp(source, "pump_rpm") == 0) {
		parser->value.get = led_data_dyn_pump_rpm;
		ret = led_parser_dyn_source_normalized(buf,
		                                       parser->value.state);
	} else {
		dev_err(parser->dev, "%s: illegal source: %s\n",
		        parser->attr->attr.name, source);
		return 1;
	}
	if (ret)
		dev_err(parser->dev, "%s: failed to parse source %s: %d\n",
		        parser->attr->attr.name, source, ret);
	return ret;
}

static int led_parser_dyn_ranges(struct led_parser_dyn *parser,
                                 const char **buf)
{
	char word[WORD_LEN_MAX + 1];
	int ret, scanned;
	enum led_parser_ret ret_parse;
	u8 min, max;
	while ((ret = sscanf(*buf, "%hhu-%hhu%n", &min, &max, &scanned)) == 2) {
		*buf += scanned;
		if (parser->ranges == LED_DATA_DYN_MSGS_SIZE) {
			dev_err(parser->dev,
			        "%s: too many value ranges: max %zu\n",
			        parser->attr->attr.name,
			        LED_DATA_DYN_MSGS_SIZE);
			return 1;
		}
		if (min > LED_DATA_DYN_VAL_MAX || max > LED_DATA_DYN_VAL_MAX) {
			dev_err(parser->dev,
			        "%s: range %u-%u exceeds max value %u",
			        parser->attr->attr.name, min, max,
			        LED_DATA_DYN_VAL_MAX);
			return 1;
		}
		parser->range_mins[parser->ranges] = min;
		parser->range_maxes[parser->ranges] = max;
		ret_parse = parser->ranges_data_parse(parser, buf);
		parser->ranges++;
		switch (ret_parse) {
		case LED_PARSER_RET_OK:
			continue;
		case LED_PARSER_RET_VALUE_MISSING:
			dev_err(parser->dev, "%s: missing value\n",
			        parser->attr->attr.name);
			break;
		case LED_PARSER_RET_VALUE_INVALID:
			dev_err(parser->dev, "%s: invalid value\n",
			        parser->attr->attr.name);
			break;
		default:
			dev_err(parser->dev, "%s: cannot parse value: ret %d\n",
			        parser->attr->attr.name, ret_parse);
			break;
		}
		return 1;
	}
	if (ret != 0 || !str_scan_word(buf, word)) {
		dev_err(parser->dev, "%s: unrecognized data left in buffer\n",
		        parser->attr->attr.name);
		return ret ? ret : 1;
	}
	return 0;
}

int led_parser_dyn_parse(struct led_parser_dyn *parser, const char *buf)
{
	int ret = led_parser_dyn_source(parser, &buf);
	if (ret)
		return ret;
	ret = led_parser_dyn_ranges(parser, &buf);
	return ret;
}
