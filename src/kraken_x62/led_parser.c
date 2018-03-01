/* Parsing of LED attributes.
 */

#include "led_parser.h"
#include "led.h"
#include "../util.h"

void led_parser_init(struct led_parser *parser)
{
	parser->moving = false;
	parser->direction = LED_DIRECTION_CLOCKWISE;
	parser->interval = LED_INTERVAL_NORMAL;
	parser->group_size = 3;

	parser->cycles = 0;
}

void led_parser_to_data(struct led_parser *parser, struct led_data *data)
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

static int led_parser_preset(struct led_parser *parser, const char **buf)
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

static enum led_parser_ret led_parser_key_moving(struct led_parser *parser,
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

static enum led_parser_ret led_parser_key_direction(struct led_parser *parser,
                                                    const char **buf)
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

static enum led_parser_ret led_parser_key_interval(struct led_parser *parser,
                                                   const char **buf)
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

static enum led_parser_ret led_parser_key_group_size(struct led_parser *parser,
                                                     const char **buf)
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

typedef enum led_parser_ret led_parser_key_common_fn(struct led_parser *parser,
                                                     const char **buf);

static const char * const LED_PARSER_KEYS_COMMON[] = {
	"moving",
	"direction",
	"interval",
	"group_size",
	NULL,
};

static led_parser_key_common_fn * const LED_PARSER_KEY_COMMON_FNS[] = {
	led_parser_key_moving,
	led_parser_key_direction,
	led_parser_key_interval,
	led_parser_key_group_size,
	NULL,
};

static enum led_parser_ret led_parser_common_parse_key(
	struct led_parser *parser, const char *key, const char **buf)
{
	size_t i;
	led_parser_key_common_fn *parse = NULL;
	for (i = 0; LED_PARSER_KEYS_COMMON[i] != NULL; i++)
		if (strcmp(LED_PARSER_KEYS_COMMON[i], key) == 0) {
			parse = LED_PARSER_KEY_COMMON_FNS[i];
			break;
		}
	if (parse == NULL)
		return LED_PARSER_RET_KEY;
	return parse(parser, buf);
}

static int led_parser_check_cycles(struct led_parser *parser)
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

static int led_parser_keys(struct led_parser *parser, const char **buf)
{
	char key[WORD_LEN_MAX + 1];
	enum led_parser_ret ret;

	while (!str_scan_word(buf, key)) {
		ret = led_parser_common_parse_key(parser, key, buf);
		if (ret == LED_PARSER_RET_KEY) {
			if (parser->cycles == LED_DATA_CYCLES_SIZE) {
				dev_err(parser->dev,
				        "%s: too many cycles: max %u\n",
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

int led_parser_parse(struct led_parser *parser, const char *buf)
{
	int ret = led_parser_preset(parser, &buf);
	if (ret)
		return ret;
	if (!parser->preset_legal(parser)) {
		dev_err(parser->dev, "%s: illegal preset\n",
		        parser->attr->attr.name);
		return 1;
	}
	ret = led_parser_keys(parser, &buf);
	if (ret)
		return ret;
	ret = led_parser_check_cycles(parser);
	return ret;
}
