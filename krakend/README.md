# krakend

Higher-level daemon which provides dynamic updates on top of the drivers.

# Installation

Make sure Rust and its package manager `cargo` are [installed](https://www.rust-lang.org/en-US/install.html)).

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
and add any users that need to have access to the daemon's socket to the newly created `krakend` group.
