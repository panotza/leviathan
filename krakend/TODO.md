- [x] --daemonize={Yes,no}

- [x] --socket-file={Create-file,systemd}

  - [x] --socket-file-group=GROUP
  
- [ ] crate feature for systemd

- [ ] If SIGTERM is received, shut down the daemon and exit cleanly.

- [ ] If SIGHUP is received, reload the configuration files, if this applies.

- [x] Provide a correct exit code from the main daemon process, as this is used by the init system to detect service errors and problems.

- [x] For integration in systemd, provide a .service unit file that carries information about starting, stopping and otherwise maintaining the daemon.

- [ ] As much as possible, rely on the init system's functionality to limit the access of the daemon to files, services and other resources, i.e. in the case of systemd, rely on systemd's resource limit control instead of implementing your own, rely on systemd's privilege dropping code instead of implementing it in the daemon, and similar.

- [x] If your daemon provides services to other local processes or remote clients via a socket, it should be made socket-activatable following the scheme pointed out below.  Like D-Bus activation, this enables on-demand starting of services as well as it allows improved parallelization of service start-up.

- [ ] If applicable, a daemon should notify the init system about startup completion or status updates via the sd_notify(3) interface.  (--notify={None,systemd})

- [ ] logging

  - [ ] Instead of using the syslog() call to log directly to the system syslog service, a new-style daemon may choose to simply log to standard error via fprintf(), which is then forwarded to syslog by the init system.  If log levels are necessary, these can be encoded by prefixing individual log lines with strings like "<4>" (for log level 4 "WARNING" in the syslog priority scheme), following a similar style as the Linux kernel's printk() level system.  For details, see sd-daemon(3) and systemd.exec(5).

- [ ] ? sub error types which automatically convert into the supertype ?
