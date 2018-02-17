/* Implementation of common driver functionality.
 */

#include "common.h"

#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/workqueue.h>

static enum hrtimer_restart kraken_update_timer(struct hrtimer *update_timer)
{
	struct usb_kraken *kraken
		= container_of(update_timer, struct usb_kraken, update_timer);
	queue_work(kraken->update_workqueue, &kraken->update_work);
	hrtimer_forward(update_timer, ktime_get(), ktime_set(1, 0));
	return HRTIMER_RESTART;
}

static void kraken_update_work(struct work_struct *update_work)
{
	struct usb_kraken *kraken
		= container_of(update_work, struct usb_kraken, update_work);
	kraken_driver_update(kraken);
}

int kraken_probe(struct usb_interface *interface,
                 const struct usb_device_id *id)
{
	char workqueue_name[64];
	int retval = -ENOMEM;
	struct usb_device *udev = interface_to_usbdev(interface);

	struct usb_kraken *kraken = kmalloc(sizeof *kraken, GFP_KERNEL);
	if (kraken == NULL) {
		goto error_kraken;
	}
	kraken->udev = usb_get_dev(udev);
	usb_set_intfdata(interface, kraken);

	retval = kraken_driver_probe(interface, id);
	if (retval) {
		goto error_driver_probe;
	}

	hrtimer_init(&kraken->update_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	kraken->update_timer.function = &kraken_update_timer;
	hrtimer_start(&kraken->update_timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	snprintf(workqueue_name, sizeof workqueue_name,
	         "%s_up", kraken_driver_name);
	kraken->update_workqueue
		= create_singlethread_workqueue(workqueue_name);
	INIT_WORK(&kraken->update_work, &kraken_update_work);

	return 0;
error_driver_probe:
	usb_set_intfdata(interface, NULL);
	usb_put_dev(kraken->udev);
	kfree(kraken);
error_kraken:
	return retval;
}

void kraken_disconnect(struct usb_interface *interface)
{
	struct usb_kraken *kraken = usb_get_intfdata(interface);

	flush_workqueue(kraken->update_workqueue);
	destroy_workqueue(kraken->update_workqueue);
	hrtimer_cancel(&kraken->update_timer);

	kraken_driver_disconnect(interface);

	usb_set_intfdata(interface, NULL);
	usb_put_dev(kraken->udev);
	kfree(kraken);
}
