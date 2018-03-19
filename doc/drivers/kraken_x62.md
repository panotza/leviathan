# Driver-specific attributes of `kraken_x62`

## Querying the device's serial number

Attribute `serial_no` is an immutable property of the device.
It is an alphanumeric string.
```Shell
$ cat /sys/bus/usb/drivers/kraken_x62/$DEVICE/serial_no
0A1B2C3D4E5
```

## Monitoring the liquid temperature

Attribute `temp_liquid` is a read-only integer in °C.
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

The attribute format is
- source of dynamic value
- fan preset

The source is the source of the dynamically updating value — currently only `temp_liquid` (liquid temperature) is supported.

Fan preset may be
- `silent`
- `performance`
- `fixed` percent
- `custom` percent × 101

`silent` and `performance` are simple presets not followed by anything.
`fixed` is followed by the percentage which the fan will be set to for all values below and including 50 (it is automatically set to 100% for anything else).
`custom` is followed by 101 percentages to set the fan to, one for each value from 0 to 100.

```Shell
$ echo 'temp_liquid performance' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/fan_percent
$ echo 'temp_liquid fixed 75' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/fan_percent
```

## Setting the pump

Attribute `pump_percent` is a write-only specification of the pump's behavior.
All percentages must be within 50 – 100 %.
Pump speed is updated based on a dynamic value.

The format is the exact same as for `fan_percent` (but the presets are differently adjusted).

```Shell
$ echo 'temp_liquid silent' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/pump_percent
$ echo 'temp_liquid custom 50 51 52 [...] 100 100 100' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/pump_percent
```
(where `[...]` stands for 95 separate percentages)

## Setting the logo LED

Attribute `led_logo` is a write-only specification of the logo LED's behavior.

The attribute format starts with one of
- `static`: set to just one static update
- `dynamic`: set to regular updates based on a dynamic value

### Static update

`static` is followed by either only the word `off` to turn the LED off, or by
- number of cycles (n)
- preset
- moving
- direction
- interval
- group size
- color × n (one per cycle)

Number of cycles is a positive integer.

Preset is one of the presets described in the protocol, with the words separated by underscores (e.g `load` or `water_cooler`).

Moving is a boolean (`yes`/`no`/`1`/`0`/...).

Direction is `clockwise` or `counterclockwise`.

Interval is one of the intervals described in the protocol.

Group size is a number between 3 and 6, inclusive.

Any of moving, direction, interval, and group size may be `*`, which directs the driver to use their default value.

Each color is in hexadecimal format, either "RrGgBb" or "RGB" (which is equivalent to RRGGBB) (e.g. `fa0080` or `40a`).

### Dynamic update

`dynamic` is followed by
- source of dynamic value
- color × 51 (one per pair of cycles)

The source may be
- `temp_liquid`: liquid temperature in °C
- `fan_rpm` max: fan speed in RPM
- `pump_rpm` max: pump speed in RPM

`fan_rpm` and `pump_rpm` are followed by a maximum value *max*, for normalization of the values such that 100 corresponds to *max*.
E.g. `fan_rpm 2000` gives value 75 when the fan runs at 1500 RPM and 34 when it runs at 678 RPM.

Each color corresponds to a pair of adjacent values, the first to values 0–1, second to 2–3, etc. (but the last one only to value 100).
Each color may also be `off` to turn the LED off for those values.

```Shell
$ echo 'static off' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/led_logo
$ echo 'static 3 breathing * * faster * ff0080 44f abcdef' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/led_logo
$ echo 'dynamic pump_rpm 3000 f00 e00 [...] f0e f0f' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/led_logo
```
(where `[...]` stands for 47 separate colors)

## Setting the ring LEDs

Attribute `leds_ring` is a write-only specification of the ring LEDs' behavior.

The format is the same as for `led_logo`, except instead of 1 color per cycle or value pair, there are 8 colors (one per ring LED).
Each 8-sequence of colors may still be replaced by a single `off` to turn the LEDs off.

```Shell
$ echo 'static 1 fixed * * * * f00 ff8000 ff0 80ff00 0f0 00ff80 0ff 0080ff' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/leds_ring
$ echo 'static 2 alternating yes * slowest * ff8035 ff8035 ff8035 ff8035 ff8035 ff8035 ff8035 ff8035 222 222 222 222 222 222 222 222' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/leds_ring
$ echo 'dynamic temp_liquid off off off fff fff off off off [...] 70b000 70da09 70b07a 70ba09 ffffff ffffff 70b000 70da09' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/leds_ring
```
(where `[...]` stands for 49 × 8 = 392 separate colors)

## Setting all LEDs synchronized

Attribute `leds_sync` is a write-only specification of both the logo LED's and the ring LEDs' behavior in a synchronized manner.

The format is the same as for `led_logo`, except instead of 1 color per cycle or value pair, there are 9 colors (one for the logo plus 8  for the ring).
Each 9-sequence of colors may still be replaced by a single `off` to turn all the LEDs off.

```Shell
$ echo 'static 5 covering_marquee * counterclockwise normal * fff 0ff 0ff 0ff 0ff 0ff 0ff 0ff 0ff [...] 646420 f0f f0f f0f f0f f0f f0f f0f f0f' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/leds_sync
$ echo 'dynamic fan_rpm 1900 111 888 000 000 000 000 000 000 000 [...] eee fff fff fff fff fff fff fff 888' > /sys/bus/usb/drivers/kraken_x62/$DEVICE/leds_sync
```
(where `[...]` stands for 49 × 9 = 441 separate colors)
