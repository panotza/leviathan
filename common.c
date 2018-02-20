/* Implementation of common driver functionality.
 */

#include "common.h"

#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/workqueue.h>

#define UPDATE_INTERVAL_DEFAULT (ms_to_ktime(1000))
#define UPDATE_INTERVAL_MIN     (ms_to_ktime(500))

static ssize_t update_interval_show(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	const s64 interval_ms = ktime_to_ms(kraken->update_interval);
	return scnprintf(buf, PAGE_SIZE, "%lld\n", interval_ms);
}

static ssize_t
update_interval_store(struct device *dev, struct device_attribute *attr,
                      const char *buf, size_t count)
{
	struct usb_kraken *kraken = usb_get_intfdata(to_usb_interface(dev));
	u64 interval_ms;
	int ret = kstrtoull(buf, 0, &interval_ms);
	if (ret) {
		return ret;
	}
	if (interval_ms < ktime_to_ms(UPDATE_INTERVAL_MIN)) {
		kraken->update_interval = UPDATE_INTERVAL_MIN;
	} else {
		kraken->update_interval = ms_to_ktime(interval_ms);
	}
	return count;
}

static DEVICE_ATTR(update_interval, S_IRUGO | S_IWUSR | S_IWGRP,
                   update_interval_show, update_interval_store);

static int kraken_create_device_files(struct usb_interface *interface)
{
	int retval;
	if ((retval = device_create_file(
		     &interface->dev, &dev_attr_update_interval)))
		goto error_update_interval;
	if ((retval = kraken_driver_create_device_files(interface)))
		goto error_driver_files;

	return 0;
error_driver_files:
	device_remove_file(&interface->dev, &dev_attr_update_interval);
error_update_interval:
	return retval;
}

static void kraken_remove_device_files(struct usb_interface *interface)
{
	kraken_driver_remove_device_files(interface);

	device_remove_file(&interface->dev, &dev_attr_update_interval);
}

static enum hrtimer_restart kraken_update_timer(struct hrtimer *update_timer)
{
	struct usb_kraken *kraken
		= container_of(update_timer, struct usb_kraken, update_timer);
	bool ret = queue_work(kraken->update_workqueue, &kraken->update_work);
	if (!ret) {
		dev_warn(&kraken->udev->dev, "work already on a queue\n");
	}
	hrtimer_forward(update_timer, ktime_get(), kraken->update_interval);
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
	retval = kraken_create_device_files(interface);
	if (retval) {
		dev_err(&interface->dev,
		        "failed to create device files: %d\n", retval);
		goto error_create_files;
	}

	kraken->update_interval = UPDATE_INTERVAL_DEFAULT;
	hrtimer_init(&kraken->update_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	kraken->update_timer.function = &kraken_update_timer;
	hrtimer_start(&kraken->update_timer, kraken->update_interval,
	              HRTIMER_MODE_REL);

	snprintf(workqueue_name, sizeof workqueue_name,
	         "%s_up", kraken_driver_name);
	kraken->update_workqueue
		= create_singlethread_workqueue(workqueue_name);
	INIT_WORK(&kraken->update_work, &kraken_update_work);

	return 0;
error_create_files:
	kraken_driver_disconnect(interface);
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

	kraken_remove_device_files(interface);
	kraken_driver_disconnect(interface);

	usb_set_intfdata(interface, NULL);
	usb_put_dev(kraken->udev);
	kfree(kraken);
}
