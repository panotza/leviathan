/* Common driver functionality that each device-specific driver must include and
 * specialize by defining the objects and functions.
 */

#ifndef LEVIATHAN_COMMON_H_INCLUDED
#define LEVIATHAN_COMMON_H_INCLUDED

#include <linux/hrtimer.h>
#include <linux/usb.h>
#include <linux/workqueue.h>

struct kraken_driver_data;

/**
 * The custom data stored in the interface, retrievable by usb_get_intfdata().
 * @data: the driver-specific data as a struct defined by the driver
 */
struct usb_kraken {
	struct usb_device *udev;
	struct usb_interface *interface;
	struct kraken_driver_data *data;

	struct hrtimer update_timer;
	struct workqueue_struct *update_workqueue;
	struct work_struct update_work;
};

/**
 * The driver's name.
 */
extern const char *kraken_driver_name;

/**
 * Driver-specific probe called from kraken_probe().  Driver-specific data must
 * be allocated here.
 */
extern int kraken_driver_probe(struct usb_interface *interface,
                               const struct usb_device_id *id);

/**
 * Driver-specific disconnect called from kraken_disconnect().  Driver-specific
 * data must be freed here.
 */
extern void kraken_driver_disconnect(struct usb_interface *interface);

/**
 * The driver's update function, called every second.
 */
extern void kraken_driver_update(struct usb_kraken *);

int kraken_probe(struct usb_interface *interface,
                 const struct usb_device_id *id);
void kraken_disconnect(struct usb_interface *interface);

#endif  /* LEVIATHAN_COMMON_H_INCLUDED */
