/* Common driver functionality that each device-specific driver must include and
 * specialize by defining the objects and functions.
 */

#ifndef LEVIATHAN_COMMON_H_INCLUDED
#define LEVIATHAN_COMMON_H_INCLUDED

#include <linux/hrtimer.h>
#include <linux/sysfs.h>
#include <linux/usb.h>
#include <linux/wait.h>
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

	const struct attribute_group *attr_group;

	// any update syncs waiting for an update wait on this; updates wake
	// everything on this up
	struct wait_queue_head update_sync_waitqueue;
	// waiting update syncs set this to false; updates set it to true
	bool update_sync_condition;

	// the update work and queue
	struct workqueue_struct *update_workqueue;
	struct work_struct update_work;
	// the update interval and timer (a value of ktime_set(0, 0) means that
	// updates are halted)
	ktime_t update_interval;
	struct hrtimer update_timer;
	// the last update's success
	int update_retval;
};

/**
 * The driver-specific attributes (which are to be put in the default attribute
 * group).  NOTE: Must be NULL-terminated.
 */
extern const struct attribute *KRAKEN_DRIVER_ATTRS[];

/**
 * The usb driver's data.
 */
extern struct usb_driver kraken_usb_driver;

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
 * The driver's update function, called every update cycle.
 */
extern int kraken_driver_update(struct usb_kraken *kraken);

int kraken_probe(struct usb_interface *interface,
                 const struct usb_device_id *id);
void kraken_disconnect(struct usb_interface *interface);

#endif  /* LEVIATHAN_COMMON_H_INCLUDED */
