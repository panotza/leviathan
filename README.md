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
  * NZXT Kraken X42
  * NZXT Kraken M22 *[?]*

A *[?]* indicates that the device should be compatible based on the product specifications, but has not yet been tested with the driver and is therefore currently unsupported.

If you have an unsupported liquid cooler — whether it is present in the above list or not — and want to help out, see [`CONTRIBUTING.md`](CONTRIBUTING.md).

# Installation
The drivers to provide the basic interface to the hardware: see [`drivers/README.md`](drivers/README.md).

The daemon to provide dynamic updates on top of the drivers: see [`krakend/README.md`](krakend/README.md).

# Usage
The drivers can used to control the devices manually: see [`drivers/README.md`](drivers/README.md).

The daemon can be controlled through its socket file once it's running: see [`krakend/README.md`](krakend/README.md).
