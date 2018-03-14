/* Dynamic values.
 */

#include "dynamic.h"
#include "driver_data.h"
#include "status.h"
#include "../util.h"

s8 dynamic_val_const_0(void *state, struct kraken_driver_data *driver_data)
{
	return 0;
}

s8 dynamic_val_temp_liquid(void *state, struct kraken_driver_data *driver_data)
{
	return status_data_temp_liquid(&driver_data->status);
}

static s8 dynamic_val_normalized(s64 value, void *state)
{
	s64 *max = state;
	s64 normalized = value * 100 / *max;
	if (normalized > DYNAMIC_VAL_MAX)
		normalized = DYNAMIC_VAL_MAX;
	return normalized;
}

static s8 dynamic_val_fan_rpm(void *state,
                              struct kraken_driver_data *driver_data)
{
	const u16 rpm = status_data_fan_rpm(&driver_data->status);
	return dynamic_val_normalized(rpm, state);
}

static s8 dynamic_val_pump_rpm(void *state,
                               struct kraken_driver_data *driver_data)
{
	const u16 rpm = status_data_pump_rpm(&driver_data->status);
	return dynamic_val_normalized(rpm, state);
}

static int dynamic_val_parse_normalized(struct dynamic_val *value,
                                        const char **buf,
                                        struct device *dev, const char *attr)
{
	s64 *max = (s64 *) value->state;
	unsigned long long max_ull;
	char max_str[WORD_LEN_MAX + 1];
	int ret = str_scan_word(buf, max_str);
	if (ret) {
		dev_err(dev, "%s: missing dynamic value max\n", attr);
		return ret;
	}
	ret = kstrtoull(max_str, 0, &max_ull);
	if (ret) {
		dev_err(dev, "%s: invalid dynamic value max %s\n", attr,
		        max_str);
		return ret;
	}
	*max = max_ull;
	return 0;
}

int dynamic_val_parse(struct dynamic_val *value, const char **buf,
                      struct device *dev, const char *attr)
{
	char source[WORD_LEN_MAX + 1];
	int ret = str_scan_word(buf, source);
	if (ret) {
		dev_err(dev, "%s: missing dynamic value source\n", attr);
		return ret;
	}
	ret = 0;
	if (strcasecmp(source, "temp_liquid") == 0) {
		value->get = dynamic_val_temp_liquid;
	} else if (strcasecmp(source, "fan_rpm") == 0) {
		value->get = dynamic_val_fan_rpm;
		ret = dynamic_val_parse_normalized(value, buf, dev, attr);
	} else if (strcasecmp(source, "pump_rpm") == 0) {
		value->get = dynamic_val_pump_rpm;
		ret = dynamic_val_parse_normalized(value, buf, dev, attr);
	} else {
		dev_err(dev, "%s: illegal dynamic value source %s\n", attr,
		        source);
		return 1;
	}
	return ret;
}
