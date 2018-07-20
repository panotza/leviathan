# drivers
The linux device drivers which provide the basic interface to the hardware.

# Installation
Make sure the headers for the kernel you are running are installed.

To build the drivers:
```Shell
make
```

To install a driver temporarily (until the next reboot):
```Shell
sudo insmod $DRIVER.ko
```
where `$DRIVER` is the name of the driver, i.e. either `kraken` or `kraken_x62`.

To install the driver permanently across reboots:
```Shell
sudo cp $DRIVER.ko /lib/modules/$VERSION/kernel/drivers/usb && sudo depmod && sudo modprobe $DRIVER
```
where `$VERSION` is the kernel version you're using, e.g. `4.16.0-2-amd64`.
After this, the driver should automatically load on boot.

**Note**: This is not permanent across kernel versions.
The process needs to be repeated whenever you upgrade your kernel — just move the `$DRIVER.ko` file into the new kernel version's `kernel/drivers/usb` directory and load with `modprobe`.

## Checking the installation
Run
```Shell
sudo dmesg
```
Near the bottom you should see `usbcore: registered new interface driver $DRIVER` or a similar message.
If your cooler is connected, then directly above this line you should see `$DRIVER 1-7:1.0: Kraken connected` or similar.
If you see both messages, the installation was successful, and there should be a directory for your cooler device's attributes in `/sys/bus/usb/drivers/$DRIVER`, e.g. `/sys/bus/usb/drivers/$DRIVER/1-7:1.0`.

## Troubleshooting `kraken_x62`
The most common issue with `kraken_x62` is the device not connecting after installation.
This is usually caused by module `usbhid` being already connected to the cooler.
To fix it, create a file `/etc/modprobe.d/usbhid-kraken-ignore.conf` with the contents
```
# 1e71:170e is the NZXT Kraken X*2 coolers
# 0x4 is HID_QUIRK_IGNORE
options usbhid quirks=0x1e71:0x170e:0x4
```
Then reload the `usbhid` module
```Shell
sudo modprobe -r usbhid && sudo modprobe usbhid
```
(You should be careful unloading USB modules as it may make your USB devices (keyboard etc.) unresponsive; therefore it's best to type all the commands out on a single line before executing them, like above.)
Also update initramfs to keep the configuration across reboots
```Shell
sudo update-initramfs -u
```

Finally reload the driver with
```Shell
sudo rmmod kraken_x62; sudo insmod kraken_x62.ko
```
or
```Shell
sudo modprobe -r kraken_x62; sudo modprobe kraken_x62
```
depending on how it was installed.

## Troubleshooting the general case
If you don't see any messages about the `$DRIVER` in `dmesg` then something went wrong and you should consider reporting it as a bug.
If you see a long, scary error message from the kernel (stacktrace, registry dump, etc.), the driver crashed and your kernel is in an invalid state; you should restart your computer before doing anything else (also consider doing any further testing of the driver in a virtual machine so you won't have to restart after each crash).

If you see the `registered new interface driver` message but not the `Kraken connected` or similar message, it is probably one of three possibilities:
* The cooler is not connected or not connected properly to the motherboard: check if it's connected properly / try reconnecting it.
* The cooler is not supported by the driver: see [`CONTRIBUTING.md`](../CONTRIBUTING.md).
* Another kernel USB module is already connected to the cooler, so the driver cannot connect to it: see [section Troubleshooting `kraken_x62`](#troubleshooting-kraken_x62).

# Usage
Each driver can be controlled with device files under `/sys/bus/usb/drivers/$DRIVER`, where `$DRIVER` is the driver name.
Find the symbolic links that point to the connected compatible devices.
In my case, there's only one Kraken connected.
```Shell
/sys/bus/usb/drivers/$DRIVER/2-1:1.0 -> ../../../../devices/pci0000:00/0000:00:06.0/usb2/2-1/2-1:1.0
```

Each attribute `$ATTRIBUTE` for device `$DEVICE` is exposed to the user through the file `/sys/bus/usb/drivers/$DRIVER/$DEVICE/kraken/$ATTRIBUTE`.
(Note the `kraken` directory before the attribute.)
This constitutes the driver's userspace interface for the device.

**Note**: Any attributes not documented here or in the driver-specific documentation are *not* part of the stable userspace interface and are subject to change anytime.

## Changing the update interval
Attribute `update_interval` is the number of milliseconds elapsed between successive USB update messages.
This is mainly useful for debugging; you probably don't need to change it from the default value.
The minimum interval is 500 ms — anything smaller is silently changed to 500.
A special value of 0 indicates that no USB updates are sent.
```Shell
$ cat /sys/bus/usb/drivers/$DRIVER/$DEVICE/kraken/update_interval
1000
$ echo $INTERVAL > /sys/bus/usb/drivers/$DRIVER/$DEVICE/kraken/update_interval
```

Module parameter `update_interval` can also be used to set the update interval at module load time.
Like the attribute, it is in milliseconds, and anything below the minimum of 500 is changed to 500.
A special value of 0 indicates that the USB update cycle is not to be started and no updates are to be sent.
```Shell
$ sudo insmod $DRIVER update_interval=$INTERVAL
```

## Syncing to the updates
Attribute `update_indicator` is a special read-only attribute.
Its purpose is to allow userspace programs to sync their actions to directly after the driver updates; it's not very useful for users handling the driver attributes manually.
When read, it blocks the read until the next update is finished (or the waiting task has been interrupted).
It's value is `1` if the next update has finished, `0` if the waiting task has been interrupted.
```Shell
$ time -p cat /sys/bus/usb/drivers/$DRIVER/$DEVICE/kraken/update_indicator
1
real 0.77
user 0.00
sys 0.00
```

## Driver-specific attributes
For documentation of the driver-specific attributes, see the files in [`doc/drivers/`](doc/drivers/).
