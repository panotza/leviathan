#ifndef KRAKEN_X62_DYNAMIC_H_INCLUDED
#define KRAKEN_X62_DYNAMIC_H_INCLUDED

#include "../common.h"

/**
 * Legal values for dynamic_val are in [0, DYNAMIC_VAL_MAX].
 */
#define DYNAMIC_VAL_MAX        ((s8) 100)

#define DYNAMIC_VAL_STATE_SIZE ((size_t) 32)

struct dynamic_val {
	// gets the dynamic value; must return negative iff an error occurs
	s8 (*get)(void *state, struct kraken_driver_data *driver_data);
	// any state needed by get may be stored here
	u8 state[DYNAMIC_VAL_STATE_SIZE];
};

s8 dynamic_val_const_0(void *state, struct kraken_driver_data *driver_data);
s8 dynamic_val_temp_liquid(void *state, struct kraken_driver_data *driver_data);

int dynamic_val_parse(struct dynamic_val *value, const char **buf,
                      struct device *dev, const char *attr);

#endif  /* KRAKEN_X62_DYNAMIC_H_INCLUDED */
