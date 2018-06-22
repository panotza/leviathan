#ifndef LEVIATHAN_X62_DRIVER_DATA_H_INCLUDED
#define LEVIATHAN_X62_DRIVER_DATA_H_INCLUDED

#include "led.h"
#include "percent.h"
#include "status.h"

#define DATA_SERIAL_NUMBER_SIZE ((size_t) 65)

struct kraken_driver_data {
	char serial_number[DATA_SERIAL_NUMBER_SIZE];

	struct status_data status;

	struct percent_data percent_fan;
	struct percent_data percent_pump;

	struct led_data led_logo;
	struct led_data leds_ring;
	struct led_data leds_sync;
};

#endif  /* LEVIATHAN_X62_DRIVER_DATA_H_INCLUDED */
