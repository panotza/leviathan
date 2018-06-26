# Driver-specific attributes of `kraken`

## Changing the speed
The speed must be between 30 and 100.
```Shell
echo SPEED > /sys/bus/usb/drivers/kraken/DEVICE/speed
```

## Changing the color
The color must be in hexadecimal format (e.g., `ff00ff` for magenta).
```Shell
echo COLOR > /sys/bus/usb/drivers/kraken/DEVICE/color
```

The alternate color for the alternating mode can be set similarly.
```Shell
echo COLOR > /sys/bus/usb/drivers/kraken/DEVICE/alternate_color
```

## Changing the alternating and blinking interval
The interval is in seconds and must be between 1 and 255.
```Shell
echo INTERVAL > /sys/bus/usb/drivers/kraken/DEVICE/interval
```

## Changing the mode
The mode must be one of normal, alternating, blinking and off.
```Shell
echo MODE > /sys/bus/usb/drivers/kraken/DEVICE/mode
```

## Monitoring the liquid temperature
The liquid temperature is returned in °C.
```Shell
cat /sys/bus/usb/drivers/kraken/DEVICE/temp
```

## Monitoring the pump speed
The pump speed is returned in RPM.
```Shell
cat /sys/bus/usb/drivers/kraken/DEVICE/pump
```

## Monitoring the fan speed
The fan speed is returned in RPM.
```Shell
cat /sys/bus/usb/drivers/kraken/DEVICE/fan
```
