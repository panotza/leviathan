# krakend

Higher-level userspace daemon which provides dynamic updates on top of the kernelspace drivers.

# Installation

Make sure Rust and its package manager `cargo` are [installed](https://www.rust-lang.org/en-US/install.html).

To build the daemon:
```Shell
make
```

And to install it:
```Shell
make install
```
The install prefix may be overridden (default `/usr/local`):
```Shell
make install prefix=/usr
```

The use of `sudo` may be disabled, if not needed or if the user doesn't have `sudo` privileges:
```Shell
make install SUDO= prefix=~/.local
```

# Running

The daemon may be run manually:
```Shell
sudo krakend --socket-file-group=$GROUP
```
where `$GROUP` is the group that shall have access to the daemon's socket.

## Starting at boot

Alternatively, the daemon may be added to your service manager if you'd like it to start at boot.

### Systemd

```Shell
make install-systemd
```
Then add any users that need to have access to the daemon's socket to the newly created (if not already existing) group for this (`krakend` by default).

The name of the group created may be overridden:
```Shell
make install-systemd socket_group=my_custom_group
```
As well as the systemd directory to install to (default `/etc/systemd/system`):
```Shell
make install-systemd systemd_dir=/usr/lib/systemd/user
```

# Usage

Once `krakend` is running, it can be controlled through its socket file by users in the file's owner group.
The location of this file is determined by the init system running the daemon (or when running manually: `/run/krakend/socket`).
