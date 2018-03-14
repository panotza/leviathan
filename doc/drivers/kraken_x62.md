# Driver-specific attributes of `kraken_x62`

## Querying the device's serial number

Attribute `serial_no` is an immutable property of the device.
It is an alphanumeric string.
```Shell
$ cat /sys/bus/usb/drivers/kraken_x62/$DEVICE/serial_no
0A1B2C3D4E5
```

## Monitoring the liquid temperature

Attribute `temp_liquid` is a read-only integer in degrees Celsius.
```Shell
$ cat /sys/bus/usb/drivers/kraken_x62/$DEVICE/temp_liquid
34
```

## Monitoring the fan

Attribute `fan_rpm` is a read-only integer in RPM.
For maximum expected value see device specifications.
```Shell
$ cat /sys/bus/usb/drivers/kraken_x62/$DEVICE/fan_rpm
769
```

## Monitoring the pump

Attribute `pump_rpm` is a read-only integer in RPM.
For maximum expected value see device specifications.
```Shell
$ cat /sys/bus/usb/drivers/kraken_x62/$DEVICE/pump_rpm
1741
```

## Setting the fan

Attribute `fan_percent` is a write-only specification of the fan's behavior.
All percentages must be within 35 – 100 %.
Fan speed is updated based on a dynamic value.
The first word is the dynamic value source — currently only `temp_liquid` (liquid temperature) is supported.

The next word decides how the value determines the speed.
It is one of `silent`, `performance`, `fixed`, or `custom`.
`silent` and `performance` are simple presets and must not be followed by anything.
`fixed` must be followed by a percentage, to which the fan will be set for all values below and including 50 (the rest of the values always give 100%).
`custom` must be followed by 101 percentages, one for each value from 0 to 100.

```Shell
$ echo 'temp_liquid performance' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/fan_percent
$ echo 'temp_liquid fixed 75' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/fan_percent
```

## Setting the pump

Attribute `pump_percent` is a write-only specification of the pump's behavior.
All percentages must be within 50 – 100 %.
Pump speed is updated based on a dynamic value.
It is set the exact same way as `fan_percent`.

```Shell
$ echo 'temp_liquid silent' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/pump_percent
$ echo 'temp_liquid custom 50 51 52 [...] 100 100 100' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/pump_percent
```

## Setting the logo LED

Attribute `led_logo` is a write-only specification of the logo LED's behavior.
The first word decides how the LED is updated, `static` for just one update, `dynamic` for regular updates based on a dynamic value.

A static update is either just the word `off` to turn the LED off, or the number of cycles followed by:
- preset
- moving
- direction
- interval
- group size

Preset is one of the presets described in the protocol, with words separated by underscores.
Moving is a boolean (yes/no/1/0).
Direction is `clockwise` or `counterclockwise`.
Interval is one of the intervals described in the protocol.
Group size is a number between 3 and 6, inclusive.
Any of moving, direction, interval, and group size may be `*`, which means to use their default value.
After the group size follow the colors, one per cycle.
Each color is in hexadecimal format, either "RrGgBb" or "RGB" (which is equivalent to RRGGBB).

A dynamic update starts with the dynamic value source, one of:
- `temp_liquid`: liquid temperature in degrees Celsius
- `fan_rpm`: fan speed in RPM
- `pump_rpm`: pump speed in RPM

`fan_rpm` and `pump_rpm` are followed by a maximum value -- the values are normalized such that 100 corresponds to the maximum value.
E.g. `fan_rpm 2000` gives 75 when the fan runs at 1500 RPM and 34 when it runs at 678 RPM.
After that follow the colors, one per pair of values, i.e. one for values 0–1, one for 2–3, one for 4–5, etc.
Each color may also be `off`, which indicates to turn the LED off.

```Shell
$ echo 'static off' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/led_logo
$ echo 'static 3 breathing * * faster * ff0080 44f abcdef' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/led_logo
$ echo 'dynamic pump_rpm 3000 f00 e00 [...] f0e f0f' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/led_logo
```

## Setting the ring LEDs

Attribute `leds_ring` is a write-only specification of the ring LEDs' behavior.
It is set the exact same way as `led_logo`, except there are 8 colors (one per LED) for each pair of values instead of only 1.
Each 8-sequence of colors may be replaced by a single `off`, which indicates to turn the LEDs off.

```Shell
$ echo 'static 1 fixed * * * * f00 ff8000 ff0 80ff00 0f0 00ff80 0ff 0080ff' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/leds_ring
$ echo 'static 2 alternating yes * slowest * ff8035 ff8035 ff8035 ff8035 ff8035 ff8035 ff8035 ff8035 222 222 222 222 222 222 222 222' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/leds_ring
$ echo 'dynamic temp_liquid off off [...] 70b000 70da09' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/leds_ring
```

## Setting all LEDs synchronized

Attribute `leds_sync` is a write-only specification of both the logo LED's and the ring LEDs' behavior in a synchronized manner.
It is set the exact same way as `leds_ring`, except there is 1 color for the logo plus 8 colors for the ring for each pair of values.
Each 9-sequence of colors may be replaced by a single `off`, which indicates to turn all the LEDs off.

```Shell
$ echo 'static 5 covering_marquee * counterclockwise normal * fff 0ff 0ff 0ff 0ff 0ff 0ff 0ff 0ff [...] 646420 f0f f0f f0f f0f f0f f0f f0f f0f' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/leds_sync
$ echo 'dynamic fan_rpm 111 888 000 000 000 000 000 000 000 [...] eee fff fff fff fff fff fff fff 888' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/leds_sync
```
