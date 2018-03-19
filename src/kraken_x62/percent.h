#ifndef LEVIATHAN_X62_PERCENT_H_INCLUDED
#define LEVIATHAN_X62_PERCENT_H_INCLUDED

#include "dynamic.h"
#include "../common.h"

#include <linux/mutex.h>

#define PERCENT_MSG_SIZE ((size_t) 5)

struct percent_msg {
	u8 msg[PERCENT_MSG_SIZE];
};

enum percent_msg_which {
	PERCENT_MSG_WHICH_FAN  = 0x00,
	PERCENT_MSG_WHICH_PUMP = 0x40,
};

struct percent_data {
	u8 percent_min;
	u8 percent_max;

	bool update;
	// called by the update function
	struct dynamic_val value;
	// msgs[val] is the message to send for value val
	struct percent_msg msgs[DYNAMIC_VAL_MAX + 1];
	s8 value_prev;
	struct percent_msg *msg_prev;

	struct mutex mutex;
};

void percent_data_init(struct percent_data *data, enum percent_msg_which which);

int kraken_x62_update_percent(struct usb_kraken *kraken,
                              struct percent_data *data);

struct percent_parser {
	struct percent_data *data;
	const char *buf;
	struct device *dev;
	const char *attr;
};

int percent_parser_parse(struct percent_parser *parser);

#endif  /* LEVIATHAN_X62_PERCENT_H_INCLUDED */
