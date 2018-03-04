/* Handling of percent-attributes.
 */

#include "percent.h"
#include "../common.h"

const u8 PERCENT_MSG_HEADER[] = {
	0x02, 0x4d,
};

void percent_data_init(struct percent_data *data, u8 type_byte)
{
	memcpy(data->msg, PERCENT_MSG_HEADER, sizeof(PERCENT_MSG_HEADER));
	data->msg[2] = type_byte;
	mutex_init(&data->mutex);
}

u8 percent_data_get(struct percent_data *data)
{
	u8 percent;
	mutex_lock(&data->mutex);
	percent = data->msg[4];
	mutex_unlock(&data->mutex);
	return percent;
}

void percent_data_set(struct percent_data *data, u8 percent)
{
	mutex_lock(&data->mutex);
	data->msg[4] = percent;
	data->update = true;
	mutex_unlock(&data->mutex);
}

int kraken_x62_update_percent(struct usb_kraken *kraken,
                              struct percent_data *data)
{
	int ret, sent;

	mutex_lock(&data->mutex);
	if (!data->update) {
		mutex_unlock(&data->mutex);
		return 0;
	}
	ret = usb_interrupt_msg(kraken->udev, usb_sndctrlpipe(kraken->udev, 1),
	                        data->msg, sizeof(data->msg), &sent, 1000);
	data->update = false;
	mutex_unlock(&data->mutex);

	if (ret || sent != sizeof(data->msg)) {
		dev_err(&kraken->udev->dev,
		        "failed to set speed percent: I/O error\n");
		return ret ? ret : 1;
	}
	return 0;
}

int percent_from(const char *buf, unsigned int min, unsigned int max)
{
	unsigned int percent;
	int ret = kstrtouint(buf, 0, &percent);
	if (ret)
		return ret;
	if (percent < min)
		return min;
	if (percent > max)
		return max;
	return percent;
}
