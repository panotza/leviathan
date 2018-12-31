- [ ] Communication protocol

  - [ ] Describe what jsonrpc is.

  - [ ] Definition of the procedures.

- [ ] If SIGTERM is received, shut down the daemon and exit cleanly.

- [ ] If SIGHUP is received, reload the configuration files, if this applies.

- [ ] As much as possible, rely on the init system's functionality to limit the access of the daemon to files, services and other resources, i.e. in the case of systemd, rely on systemd's resource limit control instead of implementing your own, rely on systemd's privilege dropping code instead of implementing it in the daemon, and similar.

- [ ] crate feature for systemd

  - [ ] document in README + the requirement of libsystemd

- [ ] logging

  - [ ] Instead of using the syslog() call to log directly to the system syslog service, a new-style daemon may choose to simply log to standard error via fprintf(), which is then forwarded to syslog by the init system.  If log levels are necessary, these can be encoded by prefixing individual log lines with strings like "<4>" (for log level 4 "WARNING" in the syslog priority scheme), following a similar style as the Linux kernel's printk() level system.  For details, see sd-daemon(3) and systemd.exec(5).

- [ ] ? sub error types which automatically convert into the supertype ?
