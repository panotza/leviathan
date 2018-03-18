# leviathan
Linux device drivers that support controlling and monitoring NZXT Kraken water coolers

NZXT is **NOT** involved in this project, do **NOT** contact them if your device is damaged while using this software.

Also, while it doesn't seem like the hardware could be damaged by silly USB messages (apart from overheating), I do **NOT** take any responsibility for any damage done to your cooler.

# Supported devices

* Driver `kraken` (for Vendor/Product ID `2433:b200`)
  * NZXT Kraken X61 
  * NZXT Kraken X41
  * NZXT Kraken X31 (Only for controlling the fan/pump speed, since there's no controllable LED on the device)
* Driver `kraken_x62` (for Vendor/Product ID `1e71:170e`)
  * NZXT Kraken X72 *[?]*
  * NZXT Kraken X62
  * NZXT Kraken X52 *[?]*
  * NZXT Kraken X42 *[?]*
  * NZXT Kraken M22 *[?]*

A *[?]* indicates that the device should be compatible based on the product specifications, but has not yet been tested with the driver and is therefore currently unsupported.

If you have an unsupported liquid cooler — whether it is present in the above list or not — and want to help out, see [CONTRIBUTING.md](CONTRIBUTING.md).

# Installation
Make sure the headers for the kernel you are running are installed.
```Shell
make && sudo insmod $DRIVER.ko
```
where `$DRIVER` is the name of the driver, either `kraken` or `kraken_x62`.

## Troubleshooting
To check if the installation was successful, run
```Shell
sudo dmesg
```
Near the bottom you should see `usbcore: registered new interface driver $DRIVER` or a similar message.
If your cooler is connected, then directly above this line you should see `$DRIVER 1-7:1.0: Kraken connected` or similar.
If you don't see either message then something went wrong and you should consider reporting it as a bug.
If you see a long, scary error message from the kernel (stacktrace, registry dump, etc.), your kernel is in an invalid state and you should also restart your computer before doing anything else (also consider doing any further testing of the driver in a virtual machine so you won't have to restart after each crash).

If you see the `usbcore` message but not the driver message, it is probably one of three possibilities:
* The cooler is not connected or not connected properly to the motherboard: check if it's connected properly / try reconnecting it.
* The cooler is not supported by the driver: see [CONTRIBUTING.md](CONTRIBUTING.md).
* Another kernel USB module is already connected to the cooler, so the driver cannot connect to it.

In the last case, search the kernel logs for any mention of `NZXT`
```Shell
sudo dmesg | grep -F 'NZXT'
```
For example, if you see a line like the following
```
hid-generic 0003:1E71:170E.0002: hiddev0,hidraw0: USB HID v1.10 Device [NZXT.-Inc. NZXT USB Device] on usb-0000:00:1d.0-1/input0
```
you'll know that the module in question is `hid_generic` or related to `hid_generic`.
You can then unload the kraken driver, temporarily unload the suspected module, load the driver, and load the module again.
Check the kernel logs again; you should now hopefully see a message like `$DRIVER 1-7:1.0: Kraken connected` near the bottom.
If that does not work, try related modules you think might be the cause in turn.

I've already troubleshooted this particular case, so I know that the cause of the issue is module `usbhid`, and the solution is:
```Shell
sudo rmmod $DRIVER && sudo modprobe -r usbhid && sudo insmod $DRIVER.ko && sudo modprobe usbhid
```
(You should be careful unloading USB modules as it may make your USB devices (keyboard etc.) unresponsive; therefore it's best to type all the commands out on a single line before executing them, like above.)

# Usage
Each driver can be controlled with device files under `/sys/bus/usb/drivers/$DRIVER`, where `$DRIVER` is the driver name.
Each attribute `$ATTRIBUTE` for a device `$DEVICE` is exposed to the user through the file `/sys/bus/usb/drivers/$DRIVER/$DEVICE/$ATTRIBUTE`.

Find the symbolic links that point to the connected compatible devices.
In my case, there's only one Kraken connected.
```Shell
/sys/bus/usb/drivers/$DRIVER/2-1:1.0 -> ../../../../devices/pci0000:00/0000:00:06.0/usb2/2-1/2-1:1.0
```

## Changing the update interval
Attribute `update_interval` is the number of milliseconds elapsed between successive USB update messages.
This is mainly useful for debugging; you probably don't need to change it from the default value.
The minimum interval is 500 ms — anything smaller is silently changed to 500.
A special value of 0 indicates that no USB updates are sent.
```Shell
$ cat /sys/bus/usb/drivers/$DRIVER/$DEVICE/update_interval
1000
$ echo $INTERVAL > /sys/bus/usb/drivers/$DRIVER/$DEVICE/update_interval
```

## Driver-specific attributes

For documentation of the driver-specific attributes, see the files in [doc/drivers/](doc/drivers/).
