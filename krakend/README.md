# krakend

Higher-level daemon which provides dynamic updates on top of the drivers.

# Running

It may be run manually, in which case it must be run as root:
```Shell
sudo krakend --socket-file-group=$GROUP
```
where `$GROUP` is the group that shall have access to the daemon's socket.
