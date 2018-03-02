#ifndef LEVIATHAN_X62_PERCENT_H_INCLUDED
#define LEVIATHAN_X62_PERCENT_H_INCLUDED

#include "../common.h"

#include <linux/mutex.h>

#define PERCENT_MSG_SIZE ((size_t) 5)

struct percent_data {
	u8 msg[PERCENT_MSG_SIZE];
	bool update;
	struct mutex mutex;
};

void percent_data_init(struct percent_data *data, u8 type_byte);
u8 percent_data_get(struct percent_data *data);
void percent_data_set(struct percent_data *data, u8 percent);

int kraken_x62_update_percent(struct usb_kraken *kraken,
                              struct percent_data *data);

int percent_from(const char *buf, unsigned int min, unsigned int max);

#endif  /* LEVIATHAN_X62_PERCENT_H_INCLUDED */
