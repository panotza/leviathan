# krakend

Higher-level daemon which provides dynamic updates on top of the drivers.

# Installation

Make sure Rust and its package manager `cargo` are [installed](https://www.rust-lang.org/en-US/install.html).

To build the daemon:
```Shell
make
```

And to install it:
```Shell
sudo make install
```

It may be run manually, in which case it must be run as root:
```Shell
sudo krakend --socket-file-group=$GROUP
```
where `$GROUP` is the group that shall have access to the daemon's socket.

## Starting at boot

Alternatively, the daemon may be added to your service manager if you'd like it to start at boot.

For `systemd`, simply run
```Shell
sudo make install-systemd
```
and add any users that need to have access to the daemon's socket to the newly created group for this (`krakend` by default).

# Usage

Once `krakend` is running, it can be controlled through its socket file by users in the file's owner group.
The default location of this file is `/run/krakend/socket`.
All communication between clients and daemon is done in [YAML 1.2](http://yaml.org), encoded UTF-8.

A client may connect to the socket and send exactly one request, to which the daemon sends one response on the connection.
Each request and response consists of one [YAML document](http://yaml.org/spec/1.2/spec.html#id2800132).
A request's YAML document *must* be explicitly terminated, i.e. end with a document end marker `...`.

Each following example consists of a request and the resulting response.

## Responses

A response is a YAML mapping.
It always includes key `error`.
If `error` maps to null then the corresponding request was successfully processed and any returned data is in the rest of the mapping.
Otherwise, `error` maps to an error message string and the rest of the mapping is not present.

Example — success:
```YAML
request: list-drivers
...
```
```YAML
error: null
drivers:
 kraken:
  - 1-7:1.0
  - 1-8:1.0
 kraken_x62:
  - 1-12:1.1
...
```

Example — error:
```YAML
request: list-drivers
...
```
```YAML
error: 'cannot access "/sys/bus/usb/drivers"'
...
```

## Requests

A request is a YAML mapping.
It always includes key `request` which maps to the type of request, as a string.

A general request has one of the following types:
- `list-drivers`

A request specific to a device also always includes key `driver` which maps to the driver as a string, and key `device` which maps to the device under the driver as a string.
It has one of the following types:
- `set-update-interval`
- `get`
- `set`


### `list-drivers`: list all available drivers and all their available devices

Request keys: *none*

Response keys:
- `drivers`: mapping of drivers (strings) to devices (sequences of strings)

Example — one driver with one device available:
```YAML
request: list-drivers
...
```
```YAML
error: null
drivers:
 kraken:
  - 1-7:1.0
...
```

Example — two drivers available; `kraken` has no devices available, `kraken_x62` has 2 devices available:
```YAML
request: list-drivers
...
```
```YAML
error: null
drivers:
 kraken:
 kraken_x62:
  - 1-13:1.0
  - 1-12:1.1
...
```

### `set-update-interval`: set the update interval

Request keys:
- `value`: the new update interval as an integer in milliseconds; written to the driver's `update_interval` attribute.

Response keys: *none*

Example — halting updates:
```YAML
request: set-update-interval
driver: kraken_x62
device: 1-2:1.0
value: 0
...
```
```YAML
error: null
...
```

### `get`: get the value of a driver attribute

Request keys:
- `attribute`: the driver attribute whose value to read

Response keys:
- `value`: the attribute's value

Any device attribute except `update_indicator` may be gotten.
Nonexistent attributes or insufficient permissions (i.e. write-only attributes) result in an error.

Example — getting the update interval:
```YAML
request: get
driver: kraken_x62
device: 1-2:1.0
attribute: update-interval
...
```
```YAML
error: null
value: 2000
...
```

Example — getting the fan speed:
```YAML
request: get
driver: kraken_x62
device: 1-2:1.0
attribute: fan_rpm
...
```
```YAML
error: null
value: 742
...
```

### `set`: set a driver attribute

Request keys:
- `attribute`: the driver attribute to set
- `value`: how to set the value

Response keys: *none*

The same attributes may be used as for a `get` request.

`value` is a mapping.
It always includes key `type`, which must map to one of `static` or `dynamic`.

A `static` value only sets the attribute to a value once, and doesn't update it again.
It always includes key `value` which maps to the attribute's new value.

A `dynamic` value sets the attribute based on a dynamic source.
The source is checked every update interval, converted to the new value, and the new value is set if it's different from the previously set value.
It always includes the keys:
- `source`: dynamic source, a mapping
- `convert`: how to convert the dynamic source to the attribute value to set

`source` always includes key `type`, which must map to one of the strings `command` or `file`.
A source with type command always includes key `command`, which maps to a non-empty string sequence containing an executable program and its arguments, which is executed each time to get the source.
A source with type file always includes key `file`, which is a filename string to read from to get the source.

`convert` is either null to indicate that the source is to be directly used as the attribute value, or a mapping.
Each key in the mapping is either an int, an int-range, a float, a float-range, or a string.
The source is attempted to be converted to each of these types in turn; the first one that converts successfully is looked up in the mapping, and the value it maps to, if any, is used.
The mapping must also contain a null key; this maps to the default value, used when none of the keys match.

An int-range or float-range is a mapping with two keys, `min` and `max`, where each maps to either null or to an int / float, respectively.
A source of the correct type matches the range if it is
- in [min, max] if min and max are non-null
- in (-∞, max] if min is null
- in [min, ∞) if max is null
- any number if both min and max are null

Example — statically setting pump:
```YAML
request: set
driver: kraken_x62
device: 1-2:1.0
attribute: pump_percent
value:
 type: static
 value: 80
...
```
```YAML
error: null
...
```

Example — dynamically setting logo LED based on liquid temperature:
```YAML
request: set
driver: kraken_x62
device: 1-2:1.0
attribute: led_logo
value:
 type: dynamic
 source:
  type: file
  file: "/sys/bus/usb/drivers/kraken_x62/1-2:1.0/temp_liquid"
 convert:
  { min: 20, max: 29 }: "1 fixed * * * * 00ffff"
  { min: 30, max: 39 }: "1 fixed * * * * 00ff00"
  { min: 40, max: 49 }: "1 fixed * * * * ffff00"
  { min: 50, max: null }: "1 fixed * * * * ff0000"
  null: "1 fixed * * * * ff00ff"
...
```
```YAML
error: null
...
```

Example — dynamically setting fan based on cpu temperature:
```YAML
request: set
driver: kraken_x62
device: 1-2:1.0
attribute: fan_percent
value:
 type: dynamic
 source:
  type: command
  # a custom script that gets CPU temperature from `sensors`
  command: ["/home/user/bin/temp-cpu"]
 convert:
  { min: 30.0, max: 40.0 }: "40"
  { min: 40.0, max: 60.0 }: "70"
  { min: 60.0, max: 80.0 }: "90"
  { min: 80.0, max: null }: "95"
  # this is output by the custom script if it thinks the temperature is "critical"
  "critical": "100"
  null: "35"
...
```
```YAML
error: null
...
```
